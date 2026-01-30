#ifndef EXT_GIF_H
#define EXT_GIF_H

//
// GIF image format support using giflib
// Supports both static and animated GIF (import only)
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

// Checks if a GIF file is animated
// Returns true if the file contains multiple frames
bool gif_is_animated(const char *file);

// Loads a static GIF image (first frame only)
// Returns invalid image on error
uImage gif_load_image(const char *file);

// Loads an animated GIF file, extracting frames and timing
// frame_times_out: array to receive frame durations in seconds (must be pre-allocated)
// frame_count_out: receives the number of frames
// Returns a sprite with frames as cols, 1 row
// Returns invalid sprite on error
uSprite gif_load_animated(const char *file, float *frame_times_out, int *frame_count_out);

#ifdef __cplusplus
}
#endif

#endif // EXT_GIF_H
