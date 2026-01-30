/**
 * makapix_import.c - Makapix Club Integration for Pixelc
 *
 * Provides functions for importing WebP animations directly from
 * Makapix Club via postMessage. Uses the same code path as the
 * Import dialog's "Copy to canvas" functionality.
 */

#include "s/s.h"
#include "u/sprite.h"
#include "canvas.h"
#include "cameractrl.h"
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
