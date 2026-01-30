/**
 * makapix_import.c - Makapix Club Integration for Pixelc
 *
 * Provides functions for importing WebP and GIF animations directly from
 * Makapix Club via postMessage. Uses the same code path as the
 * Import dialog's "Copy to canvas" functionality.
 */

#include "s/s.h"
#include "u/sprite.h"
#include "canvas.h"
#include "cameractrl.h"
#include "ext_gif.h"
#include "ext_webp.h"

#include <stdio.h>

/**
 * Load a WebP animation from raw bytes.
 *
 * Called from JavaScript after receiving MAKAPIX_EDIT_WEBP message.
 * The WebP data is written to a temp file and loaded using the same
 * webp_load_animated() function that the Import dialog uses.
 *
 * @param webp_data     Pointer to raw WebP file bytes
 * @param data_size     Size of the WebP data in bytes
 */
void pixelc_load_webp_data(const unsigned char *webp_data, int data_size) {
    s_log("makapix_import: loading WebP data, %d bytes", data_size);

    if (!webp_data || data_size <= 0) {
        s_log_error("makapix_import: invalid parameters");
        return;
    }

    // Write WebP data to virtual filesystem (same location as Import dialog uses)
    const char *filename = "import.webp";
    FILE *f = fopen(filename, "wb");
    if (!f) {
        s_log_error("makapix_import: failed to open %s for writing", filename);
        return;
    }

    size_t written = fwrite(webp_data, 1, data_size, f);
    fclose(f);

    if (written != (size_t)data_size) {
        s_log_error("makapix_import: write failed (%zu/%d bytes)", written, data_size);
        return;
    }

    s_log("makapix_import: wrote %d bytes to %s", data_size, filename);

    // Load using the same function as Import dialog
    float frame_times[CANVAS_MAX_FRAMES];
    int frame_count = 0;
    uSprite sprite = webp_load_animated(filename, frame_times, &frame_count);

    if (!u_sprite_valid(sprite)) {
        s_log_error("makapix_import: webp_load_animated failed");
        return;
    }

    s_log("makapix_import: loaded %d frames at %dx%d",
          frame_count, sprite.img.cols, sprite.img.rows);

    // Copy frame times to canvas (same as dialog_import.c)
    for (int i = 0; i < frame_count && i < CANVAS_MAX_FRAMES; i++) {
        canvas.frame_times[i] = frame_times[i];
    }

    // Set sprite on canvas (same as dialog_import.c, save=true)
    canvas_set_sprite(sprite, true);

    // Reset camera to show the full image
    cameractrl_set_home();

    s_log("makapix_import: successfully loaded animation");
}

/**
 * Load a GIF image from raw bytes.
 *
 * Called from JavaScript after receiving MAKAPIX_EDIT_GIF message.
 * The GIF data is written to a temp file and loaded using the same
 * gif_load_animated() or gif_load_image() functions that the Import dialog uses.
 *
 * @param gif_data      Pointer to raw GIF file bytes
 * @param data_size     Size of the GIF data in bytes
 */
void pixelc_load_gif_data(const unsigned char *gif_data, int data_size) {
    s_log("makapix_import: loading GIF data, %d bytes", data_size);

    if (!gif_data || data_size <= 0) {
        s_log_error("makapix_import: invalid parameters");
        return;
    }

    // Write GIF data to virtual filesystem (same location as Import dialog uses)
    const char *filename = "import.gif";
    FILE *f = fopen(filename, "wb");
    if (!f) {
        s_log_error("makapix_import: failed to open %s for writing", filename);
        return;
    }

    size_t written = fwrite(gif_data, 1, data_size, f);
    fclose(f);

    if (written != (size_t)data_size) {
        s_log_error("makapix_import: write failed (%zu/%d bytes)", written, data_size);
        return;
    }

    s_log("makapix_import: wrote %d bytes to %s", data_size, filename);

    // Check if it's animated
    bool is_animated = gif_is_animated(filename);

    if (is_animated) {
        // Load animated GIF using the same function as Import dialog
        float frame_times[CANVAS_MAX_FRAMES];
        int frame_count = 0;
        uSprite sprite = gif_load_animated(filename, frame_times, &frame_count);

        if (!u_sprite_valid(sprite)) {
            s_log_error("makapix_import: gif_load_animated failed");
            return;
        }

        s_log("makapix_import: loaded %d frames at %dx%d",
              frame_count, sprite.img.cols, sprite.img.rows);

        // Copy frame times to canvas (same as dialog_import.c)
        for (int i = 0; i < frame_count && i < CANVAS_MAX_FRAMES; i++) {
            canvas.frame_times[i] = frame_times[i];
        }

        // Set sprite on canvas (same as dialog_import.c, save=true)
        canvas_set_sprite(sprite, true);
    } else {
        // Load static GIF as single-frame sprite
        uImage img = gif_load_image(filename);

        if (!u_image_valid(img)) {
            s_log_error("makapix_import: gif_load_image failed");
            return;
        }

        s_log("makapix_import: loaded static GIF at %dx%d", img.cols, img.rows);

        // Convert image to sprite (1 frame)
        uSprite sprite = u_sprite_new_empty(img.cols, img.rows, 1, 1);
        if (!u_sprite_valid(sprite)) {
            s_log_error("makapix_import: failed to create sprite");
            u_image_kill(&img);
            return;
        }

        // Copy image data to sprite
        memcpy(u_sprite_sprite(sprite, 0, 0), img.data, img.cols * img.rows * sizeof(uColor_s));
        u_image_kill(&img);

        // Set default frame time
        canvas.frame_times[0] = 0.1f;

        // Set sprite on canvas
        canvas_set_sprite(sprite, true);
    }

    // Reset camera to show the full image
    cameractrl_set_home();

    s_log("makapix_import: successfully loaded GIF");
}

/**
 * Load a PNG image from raw bytes.
 *
 * Called from JavaScript after receiving MAKAPIX_EDIT_PNG message.
 * The PNG data is written to a temp file and loaded using SDL2_Image's
 * IMG_Load() via u_image_new_file().
 *
 * @param png_data      Pointer to raw PNG file bytes
 * @param data_size     Size of the PNG data in bytes
 */
void pixelc_load_png_data(const unsigned char *png_data, int data_size) {
    s_log("makapix_import: loading PNG data, %d bytes", data_size);

    if (!png_data || data_size <= 0) {
        s_log_error("makapix_import: invalid parameters");
        return;
    }

    // Write PNG data to virtual filesystem
    const char *filename = "import.png";
    FILE *f = fopen(filename, "wb");
    if (!f) {
        s_log_error("makapix_import: failed to open %s for writing", filename);
        return;
    }

    size_t written = fwrite(png_data, 1, data_size, f);
    fclose(f);

    if (written != (size_t)data_size) {
        s_log_error("makapix_import: write failed (%zu/%d bytes)", written, data_size);
        return;
    }

    s_log("makapix_import: wrote %d bytes to %s", data_size, filename);

    // Load PNG using SDL2_Image via u_image_new_file
    uImage img = u_image_new_file(1, filename);

    if (!u_image_valid(img)) {
        s_log_error("makapix_import: u_image_new_file failed for PNG");
        return;
    }

    s_log("makapix_import: loaded PNG at %dx%d", img.cols, img.rows);

    // Convert image to sprite (1 frame, 1 layer)
    uSprite sprite = u_sprite_new_empty(img.cols, img.rows, 1, 1);
    if (!u_sprite_valid(sprite)) {
        s_log_error("makapix_import: failed to create sprite");
        u_image_kill(&img);
        return;
    }

    // Copy image data to sprite
    memcpy(u_sprite_sprite(sprite, 0, 0), img.data, img.cols * img.rows * sizeof(uColor_s));
    u_image_kill(&img);

    // Set default frame time
    canvas.frame_times[0] = 0.1f;

    // Set sprite on canvas (save=true)
    canvas_set_sprite(sprite, true);

    // Reset camera to show the full image
    cameractrl_set_home();

    s_log("makapix_import: successfully loaded PNG");
}

/**
 * Load a BMP image from raw bytes.
 *
 * Called from JavaScript after receiving MAKAPIX_EDIT_BMP message.
 * The BMP data is written to a temp file and loaded using SDL2_Image's
 * IMG_Load() via u_image_new_file().
 *
 * @param bmp_data      Pointer to raw BMP file bytes
 * @param data_size     Size of the BMP data in bytes
 */
void pixelc_load_bmp_data(const unsigned char *bmp_data, int data_size) {
    s_log("makapix_import: loading BMP data, %d bytes", data_size);

    if (!bmp_data || data_size <= 0) {
        s_log_error("makapix_import: invalid parameters");
        return;
    }

    // Write BMP data to virtual filesystem
    const char *filename = "import.bmp";
    FILE *f = fopen(filename, "wb");
    if (!f) {
        s_log_error("makapix_import: failed to open %s for writing", filename);
        return;
    }

    size_t written = fwrite(bmp_data, 1, data_size, f);
    fclose(f);

    if (written != (size_t)data_size) {
        s_log_error("makapix_import: write failed (%zu/%d bytes)", written, data_size);
        return;
    }

    s_log("makapix_import: wrote %d bytes to %s", data_size, filename);

    // Load BMP using SDL2_Image via u_image_new_file (handles BMP natively)
    uImage img = u_image_new_file(1, filename);

    if (!u_image_valid(img)) {
        s_log_error("makapix_import: u_image_new_file failed for BMP");
        return;
    }

    s_log("makapix_import: loaded BMP at %dx%d", img.cols, img.rows);

    // Convert image to sprite (1 frame, 1 layer)
    uSprite sprite = u_sprite_new_empty(img.cols, img.rows, 1, 1);
    if (!u_sprite_valid(sprite)) {
        s_log_error("makapix_import: failed to create sprite");
        u_image_kill(&img);
        return;
    }

    // Copy image data to sprite
    memcpy(u_sprite_sprite(sprite, 0, 0), img.data, img.cols * img.rows * sizeof(uColor_s));
    u_image_kill(&img);

    // Set default frame time
    canvas.frame_times[0] = 0.1f;

    // Set sprite on canvas (save=true)
    canvas_set_sprite(sprite, true);

    // Reset camera to show the full image
    cameractrl_set_home();

    s_log("makapix_import: successfully loaded BMP");
}
