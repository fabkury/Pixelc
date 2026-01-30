#include "ext_gif.h"
#include "s/file.h"
#include "s/log.h"

#include <gif_lib.h>
#include <string.h>
#include <stdlib.h>

//
// Helper functions
//

// Read GIF from memory buffer
typedef struct {
    const uint8_t *data;
    size_t size;
    size_t pos;
} GifMemoryReader;

static int gif_read_func(GifFileType *gif, GifByteType *buf, int count) {
    GifMemoryReader *reader = (GifMemoryReader *)gif->UserData;
    size_t remaining = reader->size - reader->pos;
    size_t to_read = (size_t)count < remaining ? (size_t)count : remaining;
    memcpy(buf, reader->data + reader->pos, to_read);
    reader->pos += to_read;
    return (int)to_read;
}

// Get frame delay from Graphics Control Extension (in seconds)
static float get_frame_delay(GifFileType *gif, int frame_idx) {
    // Default delay: 100ms
    float delay = 0.1f;

    for (int i = 0; i < gif->SavedImages[frame_idx].ExtensionBlockCount; i++) {
        ExtensionBlock *ext = &gif->SavedImages[frame_idx].ExtensionBlocks[i];
        if (ext->Function == GRAPHICS_EXT_FUNC_CODE && ext->ByteCount >= 4) {
            // Delay is in centiseconds (1/100 sec), stored in bytes 1-2 (little-endian)
            int delay_cs = ext->Bytes[1] | (ext->Bytes[2] << 8);
            delay = delay_cs / 100.0f;
            if (delay < 0.01f) {
                delay = 0.1f;  // Default to 100ms if delay is 0 or too small
            }
            break;
        }
    }

    return delay;
}

// Get transparency index from Graphics Control Extension (-1 if none)
static int get_transparent_index(GifFileType *gif, int frame_idx) {
    for (int i = 0; i < gif->SavedImages[frame_idx].ExtensionBlockCount; i++) {
        ExtensionBlock *ext = &gif->SavedImages[frame_idx].ExtensionBlocks[i];
        if (ext->Function == GRAPHICS_EXT_FUNC_CODE && ext->ByteCount >= 4) {
            // Byte 0 contains flags, bit 0 indicates transparency
            if (ext->Bytes[0] & 0x01) {
                return ext->Bytes[3];  // Transparent color index
            }
            break;
        }
    }
    return -1;
}

// Get disposal method from Graphics Control Extension
static int get_disposal_method(GifFileType *gif, int frame_idx) {
    for (int i = 0; i < gif->SavedImages[frame_idx].ExtensionBlockCount; i++) {
        ExtensionBlock *ext = &gif->SavedImages[frame_idx].ExtensionBlocks[i];
        if (ext->Function == GRAPHICS_EXT_FUNC_CODE && ext->ByteCount >= 4) {
            // Disposal method is in bits 2-4 of byte 0
            return (ext->Bytes[0] >> 2) & 0x07;
        }
    }
    return 0;  // No disposal specified
}

//
// Import functions
//

bool gif_is_animated(const char *file) {
    s_log("gif_is_animated: %s", file);

    sString *data = s_file_read(file, false);
    if (!s_string_valid(data)) {
        s_log_error("failed to read file: %s", file);
        return false;
    }

    GifMemoryReader reader = {
        .data = (const uint8_t *)data->data,
        .size = data->size,
        .pos = 0
    };

    int error = 0;
    GifFileType *gif = DGifOpen(&reader, gif_read_func, &error);
    if (!gif) {
        s_log_error("failed to open GIF: %s (error %d)", file, error);
        s_string_kill(&data);
        return false;
    }

    if (DGifSlurp(gif) != GIF_OK) {
        s_log_error("failed to read GIF: %s", file);
        DGifCloseFile(gif, &error);
        s_string_kill(&data);
        return false;
    }

    bool is_animated = gif->ImageCount > 1;

    s_log("gif_is_animated: %s -> %d frames", file, gif->ImageCount);

    DGifCloseFile(gif, &error);
    s_string_kill(&data);

    return is_animated;
}

uImage gif_load_image(const char *file) {
    s_log("gif_load_image: %s", file);

    sString *data = s_file_read(file, false);
    if (!s_string_valid(data)) {
        s_log_error("failed to read file: %s", file);
        return u_image_new_invalid();
    }

    GifMemoryReader reader = {
        .data = (const uint8_t *)data->data,
        .size = data->size,
        .pos = 0
    };

    int error = 0;
    GifFileType *gif = DGifOpen(&reader, gif_read_func, &error);
    if (!gif) {
        s_log_error("failed to open GIF: %s (error %d)", file, error);
        s_string_kill(&data);
        return u_image_new_invalid();
    }

    if (DGifSlurp(gif) != GIF_OK) {
        s_log_error("failed to read GIF: %s", file);
        DGifCloseFile(gif, &error);
        s_string_kill(&data);
        return u_image_new_invalid();
    }

    if (gif->ImageCount < 1) {
        s_log_error("GIF has no images: %s", file);
        DGifCloseFile(gif, &error);
        s_string_kill(&data);
        return u_image_new_invalid();
    }

    int width = gif->SWidth;
    int height = gif->SHeight;

    s_log("gif dimensions: %dx%d", width, height);

    uImage img = u_image_new_empty(width, height, 1);
    if (!u_image_valid(img)) {
        s_log_error("failed to create image");
        DGifCloseFile(gif, &error);
        s_string_kill(&data);
        return u_image_new_invalid();
    }

    // Initialize with background color (or transparent)
    uint8_t *img_bytes = (uint8_t *)img.data;
    memset(img_bytes, 0, width * height * 4);

    // Render first frame
    SavedImage *frame = &gif->SavedImages[0];
    ColorMapObject *cmap = frame->ImageDesc.ColorMap ? frame->ImageDesc.ColorMap : gif->SColorMap;

    if (!cmap) {
        s_log_error("no color map found");
        u_image_kill(&img);
        DGifCloseFile(gif, &error);
        s_string_kill(&data);
        return u_image_new_invalid();
    }

    int transparent_idx = get_transparent_index(gif, 0);

    int frame_left = frame->ImageDesc.Left;
    int frame_top = frame->ImageDesc.Top;
    int frame_width = frame->ImageDesc.Width;
    int frame_height = frame->ImageDesc.Height;

    for (int y = 0; y < frame_height; y++) {
        for (int x = 0; x < frame_width; x++) {
            int dst_x = frame_left + x;
            int dst_y = frame_top + y;

            if (dst_x >= 0 && dst_x < width && dst_y >= 0 && dst_y < height) {
                int idx = frame->RasterBits[y * frame_width + x];

                if (idx != transparent_idx && idx < cmap->ColorCount) {
                    GifColorType *color = &cmap->Colors[idx];
                    int dst_offset = (dst_y * width + dst_x) * 4;
                    img_bytes[dst_offset + 0] = color->Red;
                    img_bytes[dst_offset + 1] = color->Green;
                    img_bytes[dst_offset + 2] = color->Blue;
                    img_bytes[dst_offset + 3] = 255;
                }
            }
        }
    }

    DGifCloseFile(gif, &error);
    s_string_kill(&data);

    s_log("gif_load_image success: %dx%d", width, height);
    return img;
}

uSprite gif_load_animated(const char *file, float *frame_times_out, int *frame_count_out) {
    s_log("gif_load_animated: %s", file);

    if (frame_count_out) *frame_count_out = 0;

    sString *data = s_file_read(file, false);
    if (!s_string_valid(data)) {
        s_log_error("failed to read file: %s", file);
        return u_sprite_new_invalid();
    }

    GifMemoryReader reader = {
        .data = (const uint8_t *)data->data,
        .size = data->size,
        .pos = 0
    };

    int error = 0;
    GifFileType *gif = DGifOpen(&reader, gif_read_func, &error);
    if (!gif) {
        s_log_error("failed to open GIF: %s (error %d)", file, error);
        s_string_kill(&data);
        return u_sprite_new_invalid();
    }

    if (DGifSlurp(gif) != GIF_OK) {
        s_log_error("failed to read GIF: %s", file);
        DGifCloseFile(gif, &error);
        s_string_kill(&data);
        return u_sprite_new_invalid();
    }

    int frame_count = gif->ImageCount;
    int width = gif->SWidth;
    int height = gif->SHeight;

    s_log("gif animation: %dx%d, %d frames", width, height, frame_count);

    if (frame_count <= 0) {
        s_log_error("invalid frame count: %d", frame_count);
        DGifCloseFile(gif, &error);
        s_string_kill(&data);
        return u_sprite_new_invalid();
    }

    // Create sprite with frames as cols, 1 row
    uSprite sprite = u_sprite_new_empty(width, height, frame_count, 1);
    if (!u_sprite_valid(sprite)) {
        s_log_error("failed to create sprite");
        DGifCloseFile(gif, &error);
        s_string_kill(&data);
        return u_sprite_new_invalid();
    }

    // Canvas for compositing frames (GIF frames can be partial and overlay previous)
    uint8_t *canvas = calloc(width * height * 4, 1);
    if (!canvas) {
        s_log_error("failed to allocate canvas");
        u_sprite_kill(&sprite);
        DGifCloseFile(gif, &error);
        s_string_kill(&data);
        return u_sprite_new_invalid();
    }

    // Process each frame
    for (int frame_idx = 0; frame_idx < frame_count; frame_idx++) {
        SavedImage *frame = &gif->SavedImages[frame_idx];
        ColorMapObject *cmap = frame->ImageDesc.ColorMap ? frame->ImageDesc.ColorMap : gif->SColorMap;

        if (!cmap) {
            s_log_warn("frame %d has no color map, skipping", frame_idx);
            continue;
        }

        int transparent_idx = get_transparent_index(gif, frame_idx);
        int disposal = get_disposal_method(gif, frame_idx);

        int frame_left = frame->ImageDesc.Left;
        int frame_top = frame->ImageDesc.Top;
        int frame_width = frame->ImageDesc.Width;
        int frame_height = frame->ImageDesc.Height;

        // Render frame onto canvas
        for (int y = 0; y < frame_height; y++) {
            for (int x = 0; x < frame_width; x++) {
                int dst_x = frame_left + x;
                int dst_y = frame_top + y;

                if (dst_x >= 0 && dst_x < width && dst_y >= 0 && dst_y < height) {
                    int idx = frame->RasterBits[y * frame_width + x];

                    if (idx != transparent_idx && idx < cmap->ColorCount) {
                        GifColorType *color = &cmap->Colors[idx];
                        int dst_offset = (dst_y * width + dst_x) * 4;
                        canvas[dst_offset + 0] = color->Red;
                        canvas[dst_offset + 1] = color->Green;
                        canvas[dst_offset + 2] = color->Blue;
                        canvas[dst_offset + 3] = 255;
                    }
                }
            }
        }

        // Copy canvas to sprite frame
        uColor_s *dst = u_sprite_sprite(sprite, frame_idx, 0);
        memcpy(dst, canvas, width * height * 4);

        // Get frame timing
        if (frame_times_out) {
            frame_times_out[frame_idx] = get_frame_delay(gif, frame_idx);
        }

        // Handle disposal method for next frame
        // 0 = No disposal specified (leave as is)
        // 1 = Do not dispose (leave as is)
        // 2 = Restore to background (clear frame area)
        // 3 = Restore to previous (not commonly used, treat as leave as is)
        if (disposal == 2) {
            // Clear the frame area to transparent
            for (int y = 0; y < frame_height; y++) {
                for (int x = 0; x < frame_width; x++) {
                    int dst_x = frame_left + x;
                    int dst_y = frame_top + y;
                    if (dst_x >= 0 && dst_x < width && dst_y >= 0 && dst_y < height) {
                        int dst_offset = (dst_y * width + dst_x) * 4;
                        canvas[dst_offset + 0] = 0;
                        canvas[dst_offset + 1] = 0;
                        canvas[dst_offset + 2] = 0;
                        canvas[dst_offset + 3] = 0;
                    }
                }
            }
        }
    }

    free(canvas);

    if (frame_count_out) *frame_count_out = frame_count;

    DGifCloseFile(gif, &error);
    s_string_kill(&data);

    s_log("gif_load_animated success: %dx%d, %d frames", width, height, frame_count);
    return sprite;
}
