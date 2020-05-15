#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <SDL.h>

typedef unsigned long long u64;
typedef unsigned int       u32;
typedef unsigned short     u16;
typedef unsigned char      u8;
typedef   signed long long i64;
typedef   signed int       i32;
typedef   signed short     i16;
typedef   signed char      i8;
typedef double             f64;
typedef float              f32;

#define ArrayCount(array) (sizeof(array) / sizeof(array[0]))

#include "main.h"
#include "game.c"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define Kilobytes(count) ((count) * 1024LL)
#define Megabytes(count) (Kilobytes(count) * 1024LL)
#define Gigabytes(count) (Megabytes(count) * 1024LL)

// ==============================================
// Platform Utilities
// ==============================================

u32 NumCpus() {
    u32 core_count = 1;

#ifdef WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    core_count = sysinfo.dwNumberOfProcessors;
#elif MACOS
    size_t attempt_count = 2;
    size_t     mib_count = 2;

    i32 mib     [    mib_count];
    i32 attempts[attempt_count];

    attempts[0] = HW_AVAILCPU;
    attempts[1] = HW_NCPU;

    for (size_t i = 0; i < attempt_count; i += 1) {
        // Set the mib for hw.ncpu
        mib[0] = CTL_HW;
        mib[1] = attempts[i];

        // Get the number of CPUs for the system.
        sysctl(mib, 2, &core_count, &len, NULL, 0);

        if (core_count > 1) {
            break;
        }
    }
#else
    core_count = sysconf(_SC_NPROCESSORS_ONLN);
#endif

    return(core_count);
}

// ==============================================
// Threading
// ==============================================

struct QueueEntry {
    void*    data;
    WorkerFn worker_fn;
};

static const u32 JOB_COUNT = 256;
static const u32 JOB_MASK  = 255;

struct JobQueue {
           u32            pool_size;
           u32            completion_goal;
           SDL_atomic_t   completion_count;
           SDL_atomic_t   read;
           SDL_atomic_t   write;
    struct SDL_semaphore* semaphore;
    struct QueueEntry     entries[256];
};

inline u32 CpuCoreCount(struct JobQueue* queue) {
    return(queue->pool_size + 1);
}

// Pushes a job into the queue so that it can be processed on multiple threads.
// Safety:
// This function should only be called from the main thread.
void PushJob(struct JobQueue* queue, void* data, WorkerFn worker_fn) {
    u32 read  = SDL_AtomicGet(&queue->read);
    u32 write = SDL_AtomicGet(&queue->write);

    u32 next_write = (write + 1) & JOB_MASK;


    // NOTE(Hector):
    // At the moment, this code will panic and abort the program if we're going to write
    // over old data. The other options to consider are clobbering the data or dropping the job
    // and signalling to the caller that we couldn't push.
    SDL_assert(next_write != read);

    struct QueueEntry* entry = &queue->entries[write];

    entry->data            = data;
    entry->worker_fn       = worker_fn;
    queue->completion_goal += 1;

    SDL_CompilerBarrier();
    SDL_AtomicSet(&queue->write, next_write);
    SDL_SemPost(queue->semaphore);
}

// This is a lock-free algorithm that allows multiple threads to read jobs from
// the queue and perform the specified work.
bool ProcessNextJob(struct JobQueue* queue) {
    bool more_work_to_do = true;

    u32 read  = SDL_AtomicGet(&queue->read);
    u32 write = SDL_AtomicGet(&queue->write);

    SDL_CompilerBarrier();

    if (read != write) {
        if (SDL_AtomicCAS(&queue->read, read, (read + 1) & JOB_MASK)) {
            struct QueueEntry* entry = &queue->entries[read];
            SDL_assert(entry->worker_fn != NULL);
            entry->worker_fn(entry->data);
            SDL_CompilerBarrier();
            SDL_AtomicIncRef(&queue->completion_count);
        }
    } else {
        more_work_to_do = false;
    }

    return(more_work_to_do);
}

// Safety:
// This function should only be called from the main thread.
void CompleteRemainingWork(struct JobQueue* queue) {
    while (
        SDL_AtomicGet(&queue->completion_count) !=
                       queue->completion_goal
    ) { ProcessNextJob(queue); }

    SDL_AtomicSet(&queue->completion_count, 0);
    queue->completion_goal = 0;
}

static volatile bool threads_should_run;

struct ThreadInfo {
           u32       index;
    struct JobQueue* queue;
};

i32 ThreadMain(void* user_data) {
    struct ThreadInfo* thread_info = (struct ThreadInfo*)user_data;
    struct JobQueue*   queue       = thread_info->queue;

    do {
        bool more_work_to_do = ProcessNextJob(queue);
        if (!more_work_to_do) {
            SDL_SemWait(queue->semaphore);
        }
    } while (threads_should_run);

    return(EXIT_SUCCESS);
}

// ==============================================
// Offscreen Buffer
// ==============================================

void InitOffscreenBuffer(
    struct SDL_Window* window,
    struct SDL_Renderer* renderer,
    struct SDL_Texture** texture,
    struct OffscreenBuffer* offscreen_buffer
) {
    i32 window_width;
    i32 window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);

    // Not sure if I need to care about this stuff.
    // u32 window_id = SDL_GetWindowID(window);
    // f32 diagonal_dpi;
    // f32 horizontal_dpi;
    // f32 vertical_dpi;
    // SDL_GetDisplayDPI(window_id, &diagonal_dpi, &horizontal_dpi, &vertical_dpi);

    *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        window_width,
        window_height
    );

    offscreen_buffer->bytes_per_pixel = 4;
    offscreen_buffer->width           = window_width;
    offscreen_buffer->height          = window_height;
}

// ==============================================
// Entry Point
// ==============================================

i32 main(i32 argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC | SDL_INIT_AUDIO) == 0) {
        // Create tracking variables for multi-threading.
        u32 num_cpus = NumCpus() - 1;

        struct SDL_Thread* threads     [num_cpus];
        struct ThreadInfo  thread_infos[num_cpus];
        struct JobQueue    job_queue = {};

        job_queue.semaphore = SDL_CreateSemaphore(0);
        job_queue.pool_size = num_cpus;

        // Spin up the threads.
        threads_should_run = true;

        for (u32 i = 0; i < num_cpus; i += 1) {
            struct ThreadInfo* info = &thread_infos[i];

            info->index = i;
            info->queue = &job_queue;
            threads[i]  = SDL_CreateThread(&ThreadMain, NULL, info);
        }

        struct SDL_Window* window = SDL_CreateWindow(
            "Demon Teacher",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            1280, 720,
            SDL_WINDOW_SHOWN
        );

        struct SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);

        if (window != NULL && renderer != NULL) {
            bool is_close_requested = false;

            // TODO(Hector):
            // Ideally I would like to use mmap and munmap for the memory allocations, but since this is a game
            // jam and I need it to run on windows...
            struct Memory memory  = {};
            memory.permanent_size = Megabytes(64);
            memory.permanent      = calloc(1, memory.permanent_size);
            memory.transient_size = Gigabytes(4);
            memory.transient      = calloc(1, memory.transient_size);

            // InputState input_state = {};

            struct OffscreenBuffer offscreen_buffer = {};
            struct SDL_Texture* texture;
            InitOffscreenBuffer(window, renderer, &texture, &offscreen_buffer);

            // SoundBuffer sound_buffer = {};

            if (memory.permanent && memory.transient) {
                i32 joystick_count = SDL_NumJoysticks();

                SDL_GameController* controller = NULL;
                SDL_Joystick*       joystick   = NULL;
                SDL_Haptic*         haptic     = NULL;

                for (i32 i = 0; i < joystick_count; i += 1) {
                    if (SDL_IsGameController(i)) {
                        controller = SDL_GameControllerOpen(i);

                        if (controller) {
                            joystick = SDL_GameControllerGetJoystick(controller);
                            haptic   = SDL_HapticOpenFromJoystick(joystick);
                            break;
                        } else {
                            SDL_Log("Could not initialise controller. %s\n", SDL_GetError());
                        }
                    }
                }

                // NOTE(Hector):
                // We don't need this if to guard the SDL functions, we just don't want to
                // try if no controller was even enabled.
                if (controller != NULL) {
                    if (SDL_HapticRumbleInit(haptic) != 0) {
                        SDL_HapticClose(haptic);
                        haptic = NULL;
                        SDL_Log("Failed to initialise haptic feedback. %s\n", SDL_GetError());
                    }
                }

                SDL_AudioSpec audio_spec = {};
                audio_spec.freq     = 48000;
                audio_spec.format   = AUDIO_S16LSB;
                audio_spec.channels = 2;
                audio_spec.samples  = 4096;
                audio_spec.callback = NULL;
                audio_spec.userdata = NULL;

                SDL_AudioDeviceID device = -1;
                if (SDL_OpenAudio(&audio_spec, NULL) == 0) {
                    // We're using SDL_OpenAudio, so the device id is always 1.
                    device = 1;
                } else {
                    SDL_Log("Unable to initialise audio. %s\n", SDL_GetError());
                }

                if (audio_spec.format == AUDIO_S16LSB) {
                    u64 frequency    = SDL_GetPerformanceFrequency();
                    u64 begin_time   = SDL_GetPerformanceCounter();
                    u64   end_time   = 0;
                    u64 delta_time   = 0;
                    u64 begin_cycles = __rdtsc();
                    u64   end_cycles = 0;
                    u64 delta_cycles = 0;

                    // struct AudioBuffer audio_buffer = {};
                    // audio_buffer.tone_hz            = 256;
                    // audio_buffer.samples_per_second = audio_spec.freq;
                    // audio_buffer.bytes_per_sample   = sizeof(i16) * 2;

                    struct InputState input_state = {};

                    SDL_Event event;
                    while (!is_close_requested) {
                          end_time   = begin_time;
                        begin_time   = SDL_GetPerformanceCounter();
                        delta_time   = begin_time - end_time;
                          end_cycles = begin_cycles;
                        begin_cycles = __rdtsc();
                        delta_cycles = begin_cycles - end_cycles;

                        f64 mspf = ((f64)delta_time * 1000.0) / (f64)frequency;
                        f64 fps  = (f64)frequency / (f64)delta_time;
                        f64 mcpf = (f64)delta_cycles / (1000.0 * 1000.0);

                        while (SDL_PollEvent(&event)) {
                            switch (event.type) {
                                case SDL_QUIT:
                                    is_close_requested = true;
                                    break;

                                case SDL_WINDOWEVENT:
                                    if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                                        if (texture != NULL) {
                                            SDL_DestroyTexture(texture);
                                            texture = NULL;
                                        }

                                        InitOffscreenBuffer(window, renderer, &texture, &offscreen_buffer);
                                    }
                                    break;

                                case SDL_CONTROLLERDEVICEADDED:
                                    break;
                                case SDL_CONTROLLERDEVICEREMOVED:
                                    break;
                                case SDL_CONTROLLERDEVICEREMAPPED:
                                    break;

                                case SDL_KEYDOWN:
                                case SDL_KEYUP: {
                                    SDL_Keycode keycode     =  event.key.keysym.sym;
                                    bool         is_pressed = (event.key.state  == SDL_PRESSED);
                                    bool        was_pressed = (event.key.state  == SDL_RELEASED)
                                                           || (event.key.repeat != 0);

                                    if (is_pressed != was_pressed) {
                                        SDL_Log("Is: %i, Was: %i", is_pressed, was_pressed);

                                        // Right handed controls
                                             if (keycode == SDLK_w) {}
                                        else if (keycode == SDLK_a) {}
                                        else if (keycode == SDLK_s) {}
                                        else if (keycode == SDLK_d) {}
                                        else if (keycode == SDLK_q) {}
                                        else if (keycode == SDLK_e) {}
                                        else if (keycode == SDLK_LSHIFT) {}
                                        // Left handed controls
                                        else if (keycode == SDLK_i) {}
                                        else if (keycode == SDLK_j) {}
                                        else if (keycode == SDLK_k) {}
                                        else if (keycode == SDLK_l) {}
                                        else if (keycode == SDLK_o) {}
                                        else if (keycode == SDLK_u) {}
                                        else if (keycode == SDLK_RSHIFT) {}
                                        // Misc
                                        else if (keycode == SDLK_ESCAPE) { is_close_requested = true; }
                                        else { /* do nothing */ }
                                    }
                                } break;
                            }
                        }

                        SDL_LockTexture(texture, NULL, &offscreen_buffer.pixels, &offscreen_buffer.pitch);
                        UpdateAndRender(&memory, &input_state, &job_queue, &offscreen_buffer);
                        SDL_UnlockTexture(texture);
                        SDL_RenderCopy(renderer, texture, NULL, NULL);
                        SDL_RenderPresent(renderer);

                        // SDL_Log("%.02f f/s, %.02f ms/f, %.02f Mcy/f\n", fps, mspf, mcpf);
                    }
                } else {
                    // Complain.
                    SDL_Log("Unable to get the desired audio format.");
                }

                SDL_CloseAudio();

                // NOTE(Hector):
                // No need to close the joystick separately because it's part of the controller.
                SDL_GameControllerClose(controller);
                SDL_HapticClose(haptic);

                free(memory.permanent);
                free(memory.transient);
            } else {
                SDL_Log("Unable to allocate memory.");
            }

            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
        } else {
            SDL_Log("Unable to create window or renderer. %s\n", SDL_GetError());
        }

        // Signal to the threads that they should stop and then wait for them.
        threads_should_run = false;

        for (u32 i = 0; i < num_cpus; i += 1) {
            SDL_SemPost(job_queue.semaphore);
        }

        for (u32 i = 0; i < num_cpus; i += 1) {
            SDL_WaitThread(threads[i], NULL);
        }

        SDL_DestroySemaphore(job_queue.semaphore);
    } else {
        SDL_Log("Failed to initialise SDL. %s\n", SDL_GetError());
    }

    SDL_Quit();
    return(EXIT_SUCCESS);
}
