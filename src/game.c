#include "locale.h"
#include "game.h"
#include "maths.h"

void DrawRect(
    struct OffscreenBuffer* buffer,
    u32 x, u32 y, u32 w, u32 h,
    u8 r, u8 g, u8 b
) {
    u32 min_x = x;
    u32 max_x = Min(x + w, buffer->width);
    u32 min_y = y;
    u32 max_y = Min(y + h, buffer->height);

    u32 colour = (
        (0xFF << 24) |
        (r    << 16) |
        (g    << 8)  |
        (b    << 0)
    );

    u8* row = (u8*)buffer->pixels
            + min_x * buffer->bytes_per_pixel
            + min_y * buffer->pitch;

    for (u32 y = min_y; y < max_y; y += 1) {
        u32* pixel = (u32*)row;

        for (u32 x = min_x; x < max_x; x += 1) {
            *pixel++ = colour;
        }

        row += buffer->pitch;
    }
}

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

void ThreadDrawRect(void* data) {
    struct RenderInfo* job = (struct RenderInfo*)data;
    DrawRect(job->buffer, job->x, job->y, job->w, job->h, job->r, job->g, job->b);
}

void UpdateAndRender(
    struct Memory*          memory,
    struct InputState*      input_state,
    struct JobQueue*        queue,
    struct OffscreenBuffer* offscreen_buffer,
    struct AudioBuffer*     audio_buffer
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

    // audio
    {
        static i32 tone_hz      = 256;
        static i16 tone_volume  = 3000;
        static f32 sample_index = 0.0f;

        i32  wave_period  = audio_buffer->samples_per_second / tone_hz;
        i32  sample_count = audio_buffer->samples_size / audio_buffer->bytes_per_sample;
        u16* samples      = audio_buffer->samples;

        for (i32 i = 0; i < sample_count; i += 1) {
            f32 sin_value = sinf(2.0f * PI * sample_index / (f32)wave_period);
            i16 sample_value = (i16)(sin_value * tone_volume);

            *samples++ = sample_value;
            *samples++ = sample_value;

            sample_index += 1.0f;
        }
    }

    // rendering
    {
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

        // Allocate into an arena instead.
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

                PushJob(queue, job, ThreadDrawRect);
            }
        }

        CompleteRemainingWork(queue);

        StackFree(jobs);
    }

    // Draw Mouse cursor
    {
        u32 w = 6;
        u32 h = 6;
        u32 x = input_state->mouse_x - w / 2;
        u32 y = input_state->mouse_y - h / 2;

        DrawRect(
            offscreen_buffer,
            x, y, w, h,
            255, 255, 255
        );
    }
}
