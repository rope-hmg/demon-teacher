// ==============================================
// Provided by the platform
// ==============================================

// ==============================================
// Threading

struct JobQueue;

typedef void (*WorkerFn)(void*);

u32  CpuCoreCount         (struct JobQueue* queue);
void PushJob              (struct JobQueue* queue, void* data, WorkerFn worker_fn);
void CompleteRemainingWork(struct JobQueue* queue);

// ==============================================
// File IO

struct DebugFile {
    u64   size;
    void* data;
};

struct DebugFile DebugOpenFile(char* filename);
void             DebugCloseFile(struct DebugFile);

// ==============================================
// Update and Render

struct Memory {
    void* permanent;
    u64   permanent_size;
    void* transient;
    u64   transient_size;
};

struct ButtonState {
    bool  is_down;
    bool was_down;
};

struct InputState {
    i32                mouse_x;
    i32                mouse_y;
    bool               mouse_double_click;
    struct ButtonState mouse_left;
    struct ButtonState mouse_right;

    struct ButtonState move_up;         // W / I
    struct ButtonState move_left;       // A / J
    struct ButtonState move_down;       // S / K
    struct ButtonState move_right;      // D / L
    struct ButtonState inventory;       // Q / O
    struct ButtonState action;          // E / U
    struct ButtonState maintain_facing; // shift (left / right)
    struct ButtonState escape;          // escape
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
    i32  samples_size;
    i32  samples_per_second;
    i32  bytes_per_sample;
};

// ==============================================
// Provided by the game
// ==============================================

void UpdateAndRender(
    struct Memory*          memory,
    struct InputState*      input_state,
    struct JobQueue*        queue,
    struct OffscreenBuffer* offscreen_buffer,
    struct AudioBuffer*     audio_buffer
);
