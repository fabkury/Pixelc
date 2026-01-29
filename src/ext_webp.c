#include "ext_webp.h"
#include "s/file.h"
#include "s/log.h"
#include "m/sca/float.h"

#include <webp/decode.h>
#include <webp/encode.h>
#include <webp/demux.h>
#include <webp/mux.h>

//
// Import functions
//

uImage webp_load_image(const char *file) {
    s_log("webp_load_image: %s", file);

    sString *data = s_file_read(file, false);
    if (!s_string_valid(data)) {
        s_log_error("failed to read file: %s", file);
        return u_image_new_invalid();
    }

    int width, height;
    if (!WebPGetInfo((const uint8_t *)data->data, data->size, &width, &height)) {
        s_log_error("failed to get WEBP info: %s", file);
        s_string_kill(&data);
        return u_image_new_invalid();
    }

    s_log("webp dimensions: %dx%d", width, height);

    uint8_t *rgba = WebPDecodeRGBA((const uint8_t *)data->data, data->size, &width, &height);
    s_string_kill(&data);

    if (!rgba) {
        s_log_error("failed to decode WEBP: %s", file);
        return u_image_new_invalid();
    }

    uImage img = u_image_new_empty(width, height, 1);
    if (!u_image_valid(img)) {
        s_log_error("failed to create image");
        WebPFree(rgba);
        return u_image_new_invalid();
    }

    // Copy RGBA data to image
    memcpy(img.data, rgba, width * height * 4);
    WebPFree(rgba);

    s_log("webp_load_image success: %dx%d", width, height);
    return img;
}

bool webp_is_animated(const char *file) {
    s_log("webp_is_animated: %s", file);

    sString *data = s_file_read(file, false);
    if (!s_string_valid(data)) {
        s_log_error("failed to read file: %s", file);
        return false;
    }

    WebPData webp_data;
    webp_data.bytes = (const uint8_t *)data->data;
    webp_data.size = data->size;

    WebPAnimDecoderOptions dec_options;
    WebPAnimDecoderOptionsInit(&dec_options);
    dec_options.color_mode = MODE_RGBA;

    WebPAnimDecoder *dec = WebPAnimDecoderNew(&webp_data, &dec_options);
    if (!dec) {
        s_string_kill(&data);
        return false;
    }

    WebPAnimInfo anim_info;
    if (!WebPAnimDecoderGetInfo(dec, &anim_info)) {
        WebPAnimDecoderDelete(dec);
        s_string_kill(&data);
        return false;
    }

    bool is_animated = anim_info.frame_count > 1;

    WebPAnimDecoderDelete(dec);
    s_string_kill(&data);

    s_log("webp_is_animated: %s -> %d frames", file, (int)anim_info.frame_count);
    return is_animated;
}

uSprite webp_load_animated(const char *file, float *frame_times_out, int *frame_count_out) {
    s_log("webp_load_animated: %s", file);

    if (frame_count_out) *frame_count_out = 0;

    sString *data = s_file_read(file, false);
    if (!s_string_valid(data)) {
        s_log_error("failed to read file: %s", file);
        return u_sprite_new_invalid();
    }

    WebPData webp_data;
    webp_data.bytes = (const uint8_t *)data->data;
    webp_data.size = data->size;

    WebPAnimDecoderOptions dec_options;
    WebPAnimDecoderOptionsInit(&dec_options);
    dec_options.color_mode = MODE_RGBA;

    WebPAnimDecoder *dec = WebPAnimDecoderNew(&webp_data, &dec_options);
    if (!dec) {
        s_log_error("failed to create WEBP anim decoder: %s", file);
        s_string_kill(&data);
        return u_sprite_new_invalid();
    }

    WebPAnimInfo anim_info;
    if (!WebPAnimDecoderGetInfo(dec, &anim_info)) {
        s_log_error("failed to get WEBP anim info: %s", file);
        WebPAnimDecoderDelete(dec);
        s_string_kill(&data);
        return u_sprite_new_invalid();
    }

    s_log("webp animation: %dx%d, %d frames",
          anim_info.canvas_width, anim_info.canvas_height, anim_info.frame_count);

    int frame_count = (int)anim_info.frame_count;
    int width = (int)anim_info.canvas_width;
    int height = (int)anim_info.canvas_height;

    if (frame_count <= 0) {
        s_log_error("invalid frame count: %d", frame_count);
        WebPAnimDecoderDelete(dec);
        s_string_kill(&data);
        return u_sprite_new_invalid();
    }

    // Create sprite with frames as cols, 1 row (like the GIF handling does)
    uSprite sprite = u_sprite_new_empty(width, height, frame_count, 1);
    if (!u_sprite_valid(sprite)) {
        s_log_error("failed to create sprite");
        WebPAnimDecoderDelete(dec);
        s_string_kill(&data);
        return u_sprite_new_invalid();
    }

    int frame_idx = 0;
    int prev_timestamp = 0;

    while (WebPAnimDecoderHasMoreFrames(dec)) {
        uint8_t *frame_rgba;
        int timestamp;

        if (!WebPAnimDecoderGetNext(dec, &frame_rgba, &timestamp)) {
            s_log_error("failed to decode frame %d", frame_idx);
            break;
        }

        if (frame_idx < frame_count) {
            // Copy frame data to sprite layer
            uColor_s *dst = u_sprite_sprite(sprite, frame_idx, 0);
            memcpy(dst, frame_rgba, width * height * 4);

            // Calculate frame duration in seconds
            if (frame_times_out) {
                int duration_ms = timestamp - prev_timestamp;
                frame_times_out[frame_idx] = duration_ms / 1000.0f;
                if (frame_times_out[frame_idx] < 0.01f) {
                    frame_times_out[frame_idx] = 0.1f; // Default 100ms
                }
            }
            prev_timestamp = timestamp;
        }

        frame_idx++;
    }

    if (frame_count_out) *frame_count_out = frame_idx;

    WebPAnimDecoderDelete(dec);
    s_string_kill(&data);

    s_log("webp_load_animated success: %dx%d, %d frames", width, height, frame_idx);
    return sprite;
}

//
// Export functions (always lossless)
//

bool webp_save_image(uImage img, const char *file) {
    s_log("webp_save_image: %s (%dx%d)", file, img.cols, img.rows);

    if (!u_image_valid(img)) {
        s_log_error("invalid image");
        return false;
    }

    uint8_t *output = NULL;
    size_t output_size = WebPEncodeLosslessRGBA(
        (const uint8_t *)img.data,
        img.cols,
        img.rows,
        img.cols * 4,  // stride
        &output
    );

    if (output_size == 0 || !output) {
        s_log_error("failed to encode lossless WEBP");
        return false;
    }

    sStr_s content = {(char *)output, output_size};
    bool success = s_file_write(file, content, false);

    WebPFree(output);

    if (!success) {
        s_log_error("failed to write file: %s", file);
        return false;
    }

    s_log("webp_save_image success: %zu bytes", output_size);
    return true;
}

bool webp_save_animated(uSprite sprite, const float *frame_times, const char *file) {
    s_log("webp_save_animated: %s (%dx%d, %d frames)",
          file, sprite.img.cols, sprite.img.rows, sprite.cols);

    if (!u_sprite_valid(sprite)) {
        s_log_error("invalid sprite");
        return false;
    }

    if (sprite.rows != 1) {
        s_log_error("expected sprite with 1 row (frames as cols)");
        return false;
    }

    int width = sprite.img.cols;
    int height = sprite.img.rows;
    int frame_count = sprite.cols;

    // Initialize animation encoder options
    WebPAnimEncoderOptions enc_options;
    if (!WebPAnimEncoderOptionsInit(&enc_options)) {
        s_log_error("failed to init encoder options");
        return false;
    }

    // Create animation encoder
    WebPAnimEncoder *enc = WebPAnimEncoderNew(width, height, &enc_options);
    if (!enc) {
        s_log_error("failed to create animation encoder");
        return false;
    }

    int timestamp_ms = 0;
    bool success = true;

    for (int frame = 0; frame < frame_count; frame++) {
        // Configure lossless encoding for this frame
        WebPConfig config;
        if (!WebPConfigInit(&config)) {
            s_log_error("failed to init config for frame %d", frame);
            success = false;
            break;
        }
        config.lossless = 1;
        config.quality = 100;

        // Import frame data
        WebPPicture pic;
        if (!WebPPictureInit(&pic)) {
            s_log_error("failed to init picture for frame %d", frame);
            success = false;
            break;
        }

        pic.width = width;
        pic.height = height;
        pic.use_argb = 1;

        // Get frame data from sprite
        uColor_s *frame_data = u_sprite_sprite(sprite, frame, 0);

        if (!WebPPictureImportRGBA(&pic, (const uint8_t *)frame_data, width * 4)) {
            s_log_error("failed to import RGBA for frame %d", frame);
            WebPPictureFree(&pic);
            success = false;
            break;
        }

        // Add frame to encoder
        if (!WebPAnimEncoderAdd(enc, &pic, timestamp_ms, &config)) {
            s_log_error("failed to add frame %d: %s", frame, WebPAnimEncoderGetError(enc));
            WebPPictureFree(&pic);
            success = false;
            break;
        }

        WebPPictureFree(&pic);

        // Calculate next timestamp
        float frame_time = frame_times ? frame_times[frame] : 0.1f;
        if (frame_time < 0.01f) frame_time = 0.1f;
        timestamp_ms += (int)(frame_time * 1000);
    }

    if (success) {
        // Add NULL frame to signal end
        if (!WebPAnimEncoderAdd(enc, NULL, timestamp_ms, NULL)) {
            s_log_error("failed to finalize animation: %s", WebPAnimEncoderGetError(enc));
            success = false;
        }
    }

    WebPData webp_data;
    WebPDataInit(&webp_data);

    if (success) {
        // Assemble the final animation
        if (!WebPAnimEncoderAssemble(enc, &webp_data)) {
            s_log_error("failed to assemble animation: %s", WebPAnimEncoderGetError(enc));
            success = false;
        }
    }

    WebPAnimEncoderDelete(enc);

    if (success) {
        // Write to file
        sStr_s content = {(char *)webp_data.bytes, webp_data.size};
        success = s_file_write(file, content, false);

        if (!success) {
            s_log_error("failed to write file: %s", file);
        } else {
            s_log("webp_save_animated success: %zu bytes", webp_data.size);
        }
    }

    WebPDataClear(&webp_data);

    return success;
}
