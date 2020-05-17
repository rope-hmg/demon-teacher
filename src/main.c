// TODO(Hector):
// 1. Clean up the code
// 2. Create a windows platform layer.

#ifdef WINDOWS
    #include <windows.h>
    #include <malloc.h>

    // MSVC doesn't support C99 because it sucks.
    #define StackAlloc(type, name, size) type* name = (type*)_malloca(size)
    #define StackFree(ptr) _freea(ptr)
#else
    #include <unistd.h>

    #define StackAlloc(type, name, size) type name[size]
    #define StackFree(ptr)
#endif

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
#define Assert(condition) SDL_assert(condition)

#include "main.h"
#include "game.c"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define Kilobytes(count) ((count) * 1024LL)
#define Megabytes(count) (Kilobytes(count) * 1024LL)
#define Gigabytes(count) (Megabytes(count) * 1024LL)
#define Terabytes(count) (Gigabytes(count) * 1024LL)

// ==============================================
// Platform Utilities
// ==============================================

u32 NumCpus() {
    u32 core_count = 1;

#ifdef WINDOWS
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
// Game Memory
// ==============================================

struct Memory InitMemory(u64 permanent_size, u64 transient_size) {
    // TODO(Hector):
    // Ideally I would like to use mmap and munmap for the memory allocations, but since this is a game
    // jam and I need it to run on windows...
    // During internal builds once this is using mmap, I want to specify the base addres so that
    // when we reload the game library all the pointers are still valid.
// #ifdef INTERNAL
//             u64 base_address = ...;
// #else
//             u64 base_address = 0;
// #endif

    u64   total_size   = permanent_size + transient_size;
    void* total_memory = calloc(1, total_size);

    struct Memory memory  = {};
    memory.permanent_size = permanent_size;
    memory.transient_size = transient_size;
    memory.permanent      = total_memory;
    memory.transient      = ((u8*)total_memory) + permanent_size;

    return(memory);
}

void FreeMemory(struct Memory memory) {
    // Since the permanent section is the pointer to the allocated address that's
    // the only thing we need to free.
    free(memory.permanent);
}

// ==============================================
// File IO
// ==============================================

struct DebugFile DebugOpenFile(char* filename) {
    struct DebugFile  file = {};
    struct SDL_RWops* io   = SDL_RWFromFile(filename, "r");

    if (io) {
        u64 size = SDL_RWsize(io);
        file.data = malloc(size);

        SDL_RWread(io, file.data, size, 1);
        SDL_RWclose(io);
    }

    return(file);
}

void DebugCloseFile(struct DebugFile file) {
    free(file.data);
}

// ==============================================
// Timing Info
// ==============================================

struct TimingInfo {
    i32 game_update_hz;
    f32 target_seconds_per_frame;
};

struct TimingInfo GetTimingInfo(struct SDL_Window* window) {
    struct TimingInfo timing_info = {};

    // If we fail to get the display mode information or the refresh rate is
    // unspecified or for what ever reason, we will default to 60Hz.
    i32 refresh_rate  = 60;
    i32 display_index = SDL_GetWindowDisplayIndex(window);

    SDL_DisplayMode display_mode;
    if (SDL_GetDesktopDisplayMode(display_index, &display_mode) == 0) {
        refresh_rate = (display_mode.refresh_rate == 0)
            ? refresh_rate
            : display_mode.refresh_rate;
    }

    timing_info.game_update_hz           = refresh_rate;
    timing_info.target_seconds_per_frame = Reciprocal((f32)refresh_rate);

    return(timing_info);
}

// ==============================================
// Audio
// ==============================================

void OpenAudio(SDL_AudioDeviceID* device, struct AudioBuffer* buffer) {
    // TODO(Hector):
    // Work out how big these buffers actually need to be.
    i32 audio_frequency  = 48000;
    i32 channel_count    = 2;
    i32 bytes_per_sample = sizeof(i16) * channel_count;
    // i32 buffer_size      = audio_frequency * bytes_per_sample;
    // i32 buffer_size      = 1024;

    SDL_AudioSpec audio_spec = {};
    audio_spec.freq     = audio_frequency;
    audio_spec.format   = AUDIO_S16LSB;
    audio_spec.channels = channel_count;
    audio_spec.samples  = audio_frequency * sizeof(u16);

    char*          device_name     = NULL;
    bool           is_capture      = false;
    SDL_AudioSpec* desired         = NULL;
    i32            allowed_changes = 0;

    *device = SDL_OpenAudioDevice(device_name, is_capture, &audio_spec, desired, allowed_changes);

    // buffer->samples            = malloc(buffer_size);
    // buffer->samples_size       = buffer_size;
    buffer->samples_per_second = audio_spec.freq;
    buffer->bytes_per_sample   = bytes_per_sample;
}

void CloseAudio(struct AudioBuffer buffer) {
    // free(buffer.samples);
    SDL_CloseAudio();
}

// ==============================================
// Entry Point
// ==============================================

i32 main(i32 argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC | SDL_INIT_AUDIO) == 0) {
        // Create tracking variables for multi-threading.
        u32 num_cpus = NumCpus() - 1;

        StackAlloc(struct SDL_Thread*, threads     , num_cpus);
        StackAlloc(struct ThreadInfo , thread_infos, num_cpus);

        struct JobQueue job_queue = {};

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

        SDL_ShowCursor(SDL_DISABLE);

        if (window != NULL && renderer != NULL) {
            bool   is_close_requested = false;
            struct Memory memory      = InitMemory(Megabytes(64), Gigabytes(4));

            struct OffscreenBuffer offscreen_buffer = {};
            struct SDL_Texture*    texture;
            InitOffscreenBuffer(window, renderer, &texture, &offscreen_buffer);

            if (memory.permanent) {
                struct TimingInfo timing_info = GetTimingInfo(window);

                SDL_AudioDeviceID audio_device;
                struct AudioBuffer audio_buffer;
                OpenAudio(&audio_device, &audio_buffer);

                if (audio_device != 0) {
                    u64 frequency    = SDL_GetPerformanceFrequency();
                    u64 begin_time   = SDL_GetPerformanceCounter();
                    u64 begin_cycles = __rdtsc();
                    u64 end_time     = 0;
                    u64 end_cycles   = 0;

                    struct InputState input_state = {};

                    SDL_Event event;
                    while (!is_close_requested) {
                        end_time     = begin_time;
                        end_cycles   = begin_cycles;
                        begin_time   = SDL_GetPerformanceCounter();
                        begin_cycles = __rdtsc();

                        f32 seconds_elapsed = ((f32)begin_time   - (f32)end_time) / (f32)frequency;
                        f32  cycles_elapsed = ((f32)begin_cycles - (f32)end_cycles);

                        // NOTE(Hector):
                        // The timing stuff calculated above is for the previous frame, so we're going to wait
                        // here to make sure that we're actually taking the correct amount of time per frame.
                        // I'm not 100% sure we need to do this, since we're already using vsync. I guess just
                        // because my monitor allows vsync, doesn't mean they all do. The quandaries...
                        if (seconds_elapsed < timing_info.target_seconds_per_frame) {
                            u32 ms_to_sleep = (u32)(timing_info.target_seconds_per_frame - seconds_elapsed) * 1000;
                            if (ms_to_sleep >= 1) ms_to_sleep -= 1;

                            SDL_Delay(ms_to_sleep);

                            #define SecondsElapsed() ((f32)SDL_GetPerformanceCounter() - (f32)end_time) / (f32)frequency
                            while (SecondsElapsed() < timing_info.target_seconds_per_frame);
                        }

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

                                case SDL_MOUSEMOTION:
                                    input_state.mouse_x = event.motion.x;
                                    input_state.mouse_y = event.motion.y;
                                    break;

                                case SDL_MOUSEBUTTONDOWN:
                                case SDL_MOUSEBUTTONUP: {
                                    u8   button   =  event.button.button;
                                    bool  is_down = (event.button.state == SDL_PRESSED);
                                    bool was_down = (event.button.state == SDL_RELEASED);

                                    input_state.mouse_double_click = event.button.clicks == 2;

                                    if (button == SDL_BUTTON_LEFT) {
                                        input_state.mouse_left. is_down =  is_down;
                                        input_state.mouse_left.was_down = was_down;
                                    } else if (button == SDL_BUTTON_RIGHT) {
                                        input_state.mouse_right. is_down =  is_down;
                                        input_state.mouse_right.was_down = was_down;
                                    } else { /* do nothing */ }
                                } break;

                                case SDL_KEYDOWN:
                                case SDL_KEYUP: {
                                    SDL_Keycode keycode  =  event.key.keysym.sym;
                                    bool         is_down = (event.key.state  == SDL_PRESSED);
                                    bool        was_down = (event.key.state  == SDL_RELEASED)
                                                        || (event.key.repeat != 0);

                                    // Right handed controls
                                    if (keycode == SDLK_w) {
                                        input_state.move_up. is_down =  is_down;
                                        input_state.move_up.was_down = was_down;
                                    } else if (keycode == SDLK_a) {
                                        input_state.move_left. is_down =  is_down;
                                        input_state.move_left.was_down = was_down;
                                    } else if (keycode == SDLK_s) {
                                        input_state.move_down. is_down =  is_down;
                                        input_state.move_down.was_down = was_down;
                                    } else if (keycode == SDLK_d) {
                                        input_state.move_right. is_down =  is_down;
                                        input_state.move_right.was_down = was_down;
                                    } else if (keycode == SDLK_q) {
                                        input_state.inventory. is_down =  is_down;
                                        input_state.inventory.was_down = was_down;
                                    } else if (keycode == SDLK_e) {
                                        input_state.action. is_down =  is_down;
                                        input_state.action.was_down = was_down;
                                    } else if (keycode == SDLK_LSHIFT) {
                                        input_state.maintain_facing. is_down =  is_down;
                                        input_state.maintain_facing.was_down = was_down;
                                    }
                                    // Left handed controls
                                    else if (keycode == SDLK_i) {
                                        input_state.move_up. is_down =  is_down;
                                        input_state.move_up.was_down = was_down;
                                    } else if (keycode == SDLK_j) {
                                        input_state.move_left. is_down =  is_down;
                                        input_state.move_left.was_down = was_down;
                                    } else if (keycode == SDLK_k) {
                                        input_state.move_down. is_down =  is_down;
                                        input_state.move_down.was_down = was_down;
                                    } else if (keycode == SDLK_l) {
                                        input_state.move_right. is_down =  is_down;
                                        input_state.move_right.was_down = was_down;
                                    } else if (keycode == SDLK_o) {
                                        input_state.inventory. is_down =  is_down;
                                        input_state.inventory.was_down = was_down;
                                    } else if (keycode == SDLK_u) {
                                        input_state.action. is_down =  is_down;
                                        input_state.action.was_down = was_down;
                                    } else if (keycode == SDLK_RSHIFT) {
                                        input_state.maintain_facing. is_down =  is_down;
                                        input_state.maintain_facing.was_down = was_down;
                                    }
                                    // Misc
                                    else if (keycode == SDLK_ESCAPE) {
                                        input_state.escape. is_down =  is_down;
                                        input_state.escape.was_down = was_down;
                                    } else { /* do nothing */ }
                                } break;
                            }
                        }

                        // TODO(Hector):
                        // Actually work out how many bytes we need to write. Probably need to use seconds_elapsed
                        // to know how long the last frame took and something something...
                        u32 one_frames_worth = audio_buffer.samples_per_second
                                             * audio_buffer.bytes_per_sample
                                             * timing_info.target_seconds_per_frame;

                        u32 queued_audio_bytes = SDL_GetQueuedAudioSize(audio_device);
                        u32 bytes_to_write     = one_frames_worth - queued_audio_bytes;
                        printf("%i %i %i\n", one_frames_worth, queued_audio_bytes, bytes_to_write);

                        audio_buffer.samples      = malloc(bytes_to_write);
                        audio_buffer.samples_size = bytes_to_write;

                        SDL_LockTexture(texture, NULL, &offscreen_buffer.pixels, &offscreen_buffer.pitch);
                        UpdateAndRender(&memory, &input_state, &job_queue, &offscreen_buffer, &audio_buffer);
                        SDL_UnlockTexture(texture);

                        SDL_QueueAudio(audio_device, audio_buffer.samples, bytes_to_write);
                        free(audio_buffer.samples);
                        // TODO(Hector):
                        // Clean this up.
                        static bool audio_is_paused = true;
                        if (audio_is_paused) {
                            audio_is_paused = false;
                            SDL_PauseAudioDevice(audio_device, audio_is_paused);
                        }

                        SDL_RenderCopy(renderer, texture, NULL, NULL);
                        SDL_RenderPresent(renderer);
                    }

                    CloseAudio(audio_buffer);
                } else {
                    SDL_Log("Unable to initialise audio. %s\n", SDL_GetError());
                }

                FreeMemory(memory);
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

        StackFree(threads);
        StackFree(thread_infos);
    } else {
        SDL_Log("Failed to initialise SDL. %s\n", SDL_GetError());
    }

    SDL_Quit();
    return(EXIT_SUCCESS);
}
