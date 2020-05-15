// ==============================================
// Provided by the platform
// ==============================================

struct Memory {
    void* permanent;
    u64   permanent_size;
    void* transient;
    u64   transient_size;
};

struct InputState {
    bool is_analog;

    bool move_up;
    u32  move_up_count;
    bool move_left;
    u32  move_left_count;
    bool move_down;
    u32  move_down_count;
    bool move_right;
    u32  move_right_count;

    i32 move_horizontal;
    i32 move_vertical;
};

struct OffscreenBuffer {
    void* pixels;
    u32   bytes_per_pixel;
    i32   pitch;
    i32   width;
    i32   height;
};

struct AudioBuffer {
    u16* samples;
    i32  samples_per_second;
    i32  bytes_per_sample;
};

struct JobQueue;

typedef void (*WorkerFn)(void*);

u32  CpuCoreCount         (struct JobQueue* queue);
void PushJob              (struct JobQueue* queue, void* data, WorkerFn worker_fn);
void CompleteRemainingWork(struct JobQueue* queue);

void SetWindowTitle(char* title);

void ReadFile();
void WriteFile();

// ==============================================
// Provided by the game
// ==============================================

void UpdateAndRender(
    struct Memory*          memory,
    struct InputState*      input_state,
    struct JobQueue*        queue,
    struct OffscreenBuffer* offscreen_buffer
);
