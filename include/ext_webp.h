#ifndef EXT_WEBP_H
#define EXT_WEBP_H

//
// WEBP image format support using libwebp
// Supports both lossy and lossless WEBP (static and animated)
// Export is always lossless
//

#include "s/s.h"
#include "u/image.h"
#include "u/sprite.h"

#ifdef __cplusplus
extern "C" {
#endif

//
// Import functions
//

// Loads a static WEBP image (handles both lossy and lossless)
// Returns invalid image on error
uImage webp_load_image(const char *file);

// Loads an animated WEBP file, extracting frames and timing
// frame_times_out: array to receive frame durations in seconds (must be pre-allocated)
// frame_count_out: receives the number of frames
// Returns a sprite with frames as cols, 1 row
// Returns invalid sprite on error
uSprite webp_load_animated(const char *file, float *frame_times_out, int *frame_count_out);

// Checks if a WEBP file is animated
// Returns true if the file contains multiple frames
bool webp_is_animated(const char *file);

//
// Export functions (always lossless)
//

// Saves a static image as lossless WEBP
// Returns true on success
bool webp_save_image(uImage img, const char *file);

// Saves an animated sprite as lossless animated WEBP
// sprite: sprite with frames as cols
// frame_times: array of frame durations in seconds
// Returns true on success
bool webp_save_animated(uSprite sprite, const float *frame_times, const char *file);

#ifdef __cplusplus
}
#endif

#endif // EXT_WEBP_H
