#include "locale.h"
#include "game.h"
#include "maths.h"

struct RenderInfo {
    struct OffscreenBuffer* buffer;

    u32 x;
    u32 y;
    u32 w;
    u32 h;

    u8 r;
    u8 g;
    u8 b;

    u32 x_offset;
    u32 y_offset;
};

void Render(void* data) {
    struct RenderInfo* job = (struct RenderInfo*)data;

    u32 min_x = job->x;
    u32 max_x = Min(job->x + job->w, job->buffer->width);
    u32 min_y = job->y;
    u32 max_y = Min(job->y + job->h, job->buffer->height);

    u32 colour = (
        (0xFF   << 24) |
        (job->r << 16) |
        (job->g << 8)  |
        (job->b << 0)
    );

    u8* row = (u8*)job->buffer->pixels
        + min_x * job->buffer->bytes_per_pixel
        + min_y * job->buffer->pitch;

    for (u32 y = min_y; y < max_y; y += 1) {

        u32* pixel = (u32*)row;
        for (u32 x = min_x; x < max_x; x += 1) {
            *pixel = ((x + job->x_offset) << 8) | y + job->y_offset;
            // *pixel = colour;
            pixel += 1;
        }

        row += job->buffer->pitch;
    }
}

void UpdateAndRender(
    struct Memory*          memory,
    struct InputState*      input_state,
    struct JobQueue*        queue,
    struct OffscreenBuffer* offscreen_buffer
) {
    struct GameState* state = (struct GameState*)memory->permanent;

    if (!state->initialised) {
        state->initialised = true;
        state->x_offset    = 0;
        state->y_offset    = 0;
        state->locale      = &en_gb;
    }

    state->x_offset += 1; // input_state->move_horizontal;
    state->y_offset += 1; // input_state->move_vertical;

    u32 pixel_width  = offscreen_buffer->width;
    u32 pixel_height = offscreen_buffer->height;

    u32 cpu_core_count          = CpuCoreCount(queue);
    u32 pixels_per_chunk_width  = pixel_width  / (cpu_core_count / 2);
    u32 pixels_per_chunk_height = pixel_height / (cpu_core_count / 2);

    u32 remaining_pixels_x = pixel_width  % pixels_per_chunk_width;
    u32 remaining_pixels_y = pixel_height % pixels_per_chunk_height;
    u32 chunks_per_width   = pixel_width  / pixels_per_chunk_width;
    u32 chunks_per_height  = pixel_height / pixels_per_chunk_height;
    u32 total_chunks       = chunks_per_width * chunks_per_height;

    StackAlloc(struct RenderInfo, jobs, total_chunks);

    for (u32 y = 0; y < chunks_per_height; y += 1) {
        for (u32 x = 0; x < chunks_per_width; x += 1) {
            u32 index = x + y * chunks_per_width;
            struct RenderInfo* job = &jobs[index];

            bool last_row = y == chunks_per_height - 1;
            bool last_col = x == chunks_per_width  - 1;

            job->buffer   = offscreen_buffer;
            job->x        = x * pixels_per_chunk_width;
            job->y        = y * pixels_per_chunk_height;
            job->w        = pixels_per_chunk_width  + (last_col ? remaining_pixels_x : 0);
            job->h        = pixels_per_chunk_height + (last_row ? remaining_pixels_y : 0);
            job->r        = (255 / chunks_per_width  * x);
            job->g        = (255 / chunks_per_height * y);
            job->b        = 0;
            job->x_offset = state->x_offset;
            job->y_offset = state->y_offset;

            PushJob(queue, job, Render);
        }
    }

    CompleteRemainingWork(queue);

    StackFree(jobs);
}
