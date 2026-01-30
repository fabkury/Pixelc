#include <string.h>
#include "e/io.h"
#include "r/r.h"
#include "u/pose.h"
#include "u/image.h"
#include "u/button.h"
#include "canvas.h"
#include "cameractrl.h"
#include "selectionctrl.h"
#include "dialog.h"
#include "ext_gif.h"
#include "ext_webp.h"

static const uColor_s BG_A_COLOR = {{136, 136, 102, 255}};
static const uColor_s BG_B_COLOR = {{143, 143, 102, 255}};
static const vec4 TITLE_COLOR = {{0, 0.6, 0.3, 1}};

//
// private
//


typedef struct {
    RoText info;
    RoSingle import;

    bool import_available;
    bool is_webp;
    bool is_animated_webp;
    int webp_frame_count;
    bool is_gif;
    bool is_animated_gif;
    int gif_frame_count;

    RoText to_canvas_txt;
    RoSingle to_canvas_btn;

    RoText as_selection_txt;
    RoSingle as_selection_btn;

    RoSingle upload;
    RoText upload_label;
    bool upload_available;

} Impl;


static void kill_fn() {
    Impl *impl = dialog.impl;
    ro_text_kill(&impl->info);
    ro_single_kill(&impl->import);
    ro_single_kill(&impl->upload);
    ro_text_kill(&impl->upload_label);
    ro_text_kill(&impl->to_canvas_txt);
    ro_single_kill(&impl->to_canvas_btn);
    ro_text_kill(&impl->as_selection_txt);
    ro_single_kill(&impl->as_selection_btn);
    s_free(impl);
}

static void update(float dtime) {
    // noop
}

static void render(const mat4 *cam_mat) {
    Impl *impl = dialog.impl;
    ro_text_render(&impl->info, cam_mat);
    if (impl->import_available) {
        ro_single_render(&impl->import, cam_mat);

        ro_text_render(&impl->to_canvas_txt, cam_mat);
        ro_single_render(&impl->to_canvas_btn, cam_mat);

        ro_text_render(&impl->as_selection_txt, cam_mat);
        ro_single_render(&impl->as_selection_btn, cam_mat);
    }
    if (impl->upload_available) {
        ro_text_render(&impl->upload_label, cam_mat);
        ro_single_render(&impl->upload, cam_mat);
    }
}

static void uploaded_image(const char *file, bool ascii, const char *user_file_name, void *user_data) {
    s_log("image uploaded: %s (user file: %s)", file, user_file_name);

    // Check for GIF first (JavaScript saves to import.gif)
    if (gif_is_animated("import.gif")) {
        s_log("animated gif detected");
        // For animated gif, we'll load it directly when the user clicks import
        // Just recreate the dialog to show the preview
    } else {
        // Check if it's a static gif (file exists but not animated)
        uImage img = gif_load_image("import.gif");
        if (u_image_valid(img)) {
            // Static gif was uploaded - convert to PNG for compatibility
            s_log("static gif detected, converting to PNG");
            u_image_save_file(img, "import.png");
            u_image_kill(&img);
        } else {
            // Not a GIF, check for WEBP
            // The JavaScript auto-detects and saves to import.webp for WEBP files
            if (webp_is_animated("import.webp")) {
                s_log("animated webp detected");
                // For animated webp, we'll load it directly when the user clicks import
                // Just recreate the dialog to show the preview
            } else {
                // Check if it's a static webp (file exists but not animated)
                img = webp_load_image("import.webp");
                if (u_image_valid(img)) {
                    // Static webp was uploaded - convert to PNG for compatibility
                    s_log("static webp detected, converting to PNG");
                    u_image_save_file(img, "import.png");
                    u_image_kill(&img);
                }
                // For PNG uploads, import.png is already in place (no conversion needed)
            }
        }
    }

    dialog_create_import();
}

static bool pointer_event(ePointer_s pointer) {
    Impl *impl = dialog.impl;

    if (impl->upload_available && u_button_clicked(&impl->upload.rect, pointer)) {
        s_log("import image upload...");
        e_io_ask_for_file_upload("import.image", false, uploaded_image, NULL);
        // return after hide, hide kills this dialog
        return true;
    }

    if (impl->import_available && u_button_clicked(&impl->to_canvas_btn.rect, pointer)) {
        s_log("import to canvas");

        if (impl->is_animated_gif) {
            // Load animated gif with frame times
            float frame_times[CANVAS_MAX_FRAMES];
            int frame_count = 0;
            uSprite sprite = gif_load_animated("import.gif", frame_times, &frame_count);
            if (!u_sprite_valid(sprite)) {
                dialog_create_import();
                return true;
            }
            // Copy frame times to canvas
            for (int i = 0; i < frame_count && i < CANVAS_MAX_FRAMES; i++) {
                canvas.frame_times[i] = frame_times[i];
            }
            canvas_set_sprite(sprite, true);
        } else if (impl->is_animated_webp) {
            // Load animated webp with frame times
            float frame_times[CANVAS_MAX_FRAMES];
            int frame_count = 0;
            uSprite sprite = webp_load_animated("import.webp", frame_times, &frame_count);
            if (!u_sprite_valid(sprite)) {
                dialog_create_import();
                return true;
            }
            // Copy frame times to canvas
            for (int i = 0; i < frame_count && i < CANVAS_MAX_FRAMES; i++) {
                canvas.frame_times[i] = frame_times[i];
            }
            canvas_set_sprite(sprite, true);
        } else {
            uSprite sprite = u_sprite_new_file(1, 1, "import.png");
            if (!u_sprite_valid(sprite)) {
                dialog_create_import();
                // return after hide, hide kills this dialog
                return true;
            }
            canvas_set_sprite(sprite, true);
        }
        cameractrl_set_home();
        dialog_hide();
        // return after hide, hide kills this dialog
        return true;
    }

    if (impl->import_available && u_button_clicked(&impl->as_selection_btn.rect, pointer)) {
        s_log("import to canvas");
        uImage img = u_image_new_file(1, "import.png");
        if (!u_image_valid(img)) {
            dialog_create_import();
            return true;
        }
        s_log("tool import");
        selectionctrl_paste_image(img);
        u_image_kill(&img);
        dialog_hide();
        // return after hide, hide kills this dialog
        return true;
    }

    return true;
}

static void on_action(bool ok) {
    dialog_hide();
}

//
// public
//


void dialog_create_import() {
    dialog_hide();
    canvas_reload();
    s_log("create");
    Impl *impl = s_new0(Impl, 1);
    dialog.impl = impl;

    // Check for animated gif first, then animated webp
    impl->is_animated_gif = gif_is_animated("import.gif");
    impl->is_animated_webp = !impl->is_animated_gif && webp_is_animated("import.webp");

    uImage img;
    if (impl->is_animated_gif) {
        // Load first frame for preview
        float frame_times[CANVAS_MAX_FRAMES];
        uSprite sprite = gif_load_animated("import.gif", frame_times, &impl->gif_frame_count);
        if (u_sprite_valid(sprite)) {
            // Create a new image with just the first frame
            img = u_image_new_empty(sprite.img.cols, sprite.img.rows, 1);
            if (u_image_valid(img)) {
                memcpy(img.data, u_sprite_sprite(sprite, 0, 0),
                       sprite.img.cols * sprite.img.rows * sizeof(uColor_s));
            }
            impl->is_gif = true;
            u_sprite_kill(&sprite);
        } else {
            img = u_image_new_invalid();
        }
    } else if (impl->is_animated_webp) {
        // Load first frame for preview
        float frame_times[CANVAS_MAX_FRAMES];
        uSprite sprite = webp_load_animated("import.webp", frame_times, &impl->webp_frame_count);
        if (u_sprite_valid(sprite)) {
            // Create a new image with just the first frame
            img = u_image_new_empty(sprite.img.cols, sprite.img.rows, 1);
            if (u_image_valid(img)) {
                memcpy(img.data, u_sprite_sprite(sprite, 0, 0),
                       sprite.img.cols * sprite.img.rows * sizeof(uColor_s));
            }
            impl->is_webp = true;
            u_sprite_kill(&sprite);
        } else {
            img = u_image_new_invalid();
        }
    } else {
        img = u_image_new_file(1, "import.png");
    }

    impl->import_available = u_image_valid(img)
                             && img.cols <= CANVAS_MAX_SIZE
                             && img.rows <= CANVAS_MAX_SIZE
                             && canvas_size_valid(img.cols, img.rows, 1, 1);

    float pos = 16;

    impl->info = ro_text_new_font55(64);
    ro_text_set_color(&impl->info, DIALOG_TEXT_COLOR);

    if (!impl->import_available) {
        ro_text_set_text(&impl->info, "Select an image\n"
                                      "to import");
        impl->info.pose = u_pose_new(DIALOG_LEFT + 8, DIALOG_TOP - pos - 4, 1, 2);
        pos += 26;

    } else {

        char text[128];
        if (impl->is_animated_gif) {
            snprintf(text, sizeof text, "cols: %i\n"
                                        "rows: %i\n"
                                        "frames: %i", img.cols, img.rows, impl->gif_frame_count);
        } else if (impl->is_animated_webp) {
            snprintf(text, sizeof text, "cols: %i\n"
                                        "rows: %i\n"
                                        "frames: %i", img.cols, img.rows, impl->webp_frame_count);
        } else {
            snprintf(text, sizeof text, "cols: %i\n"
                                        "rows: %i", img.cols, img.rows);
        }
        ro_text_set_text(&impl->info, text);
        impl->info.pose = u_pose_new(DIALOG_LEFT + 50, DIALOG_TOP - pos - 4, 1, 2);

        impl->import = ro_single_new(r_texture_new(img.cols, img.rows,
                                                   1, 1, img.data));

        float width = 32;
        float height = 32;
        if (img.cols > img.rows) {
            height = height * img.rows / img.cols;
        } else if (img.cols < img.rows) {
            width = width * img.cols / img.rows;
        }
        impl->import.rect.pose = u_pose_new(DIALOG_LEFT + 8 + 16, DIALOG_TOP - pos -16, width, height);

        pos += 34;

        if (impl->is_animated_gif || impl->is_animated_webp) {
            pos += 10;  // Extra space for frames info
        }

        impl->to_canvas_txt = ro_text_new_font55(32);
        ro_text_set_text(&impl->to_canvas_txt, "Copy into\n"
                                               "      canvas:");
        ro_text_set_color(&impl->to_canvas_txt, DIALOG_TEXT_COLOR);
        impl->to_canvas_txt.pose = u_pose_new(DIALOG_LEFT + 8, DIALOG_TOP - pos, 1, 2);

        impl->to_canvas_btn = ro_single_new(r_texture_new_file(2, 1, "res/button_to.png"));
        impl->to_canvas_btn.rect.pose = u_pose_new_aa(DIALOG_LEFT + DIALOG_WIDTH - 20, DIALOG_TOP - pos - 4,
                                                      16, 16);
        pos += 30;


        impl->as_selection_txt = ro_text_new_font55(32);
        ro_text_set_text(&impl->as_selection_txt, "As selection:");
        ro_text_set_color(&impl->as_selection_txt, DIALOG_TEXT_COLOR);
        impl->as_selection_txt.pose = u_pose_new(DIALOG_LEFT + 8, DIALOG_TOP - pos, 1, 2);

        impl->as_selection_btn = ro_single_new(r_texture_new_file(2, 1, "res/button_selection.png"));
        impl->as_selection_btn.rect.pose = u_pose_new_aa(DIALOG_LEFT + DIALOG_WIDTH - 20,
                                                         DIALOG_TOP - pos + 2,
                                                         16, 16);
        pos += 8;
    }

    u_image_kill(&img);

    dialog.impl_height = pos;

#ifndef PLATFORM_CXXDROID
    // Use wider dialog to fit label and button
    dialog.impl_width = 150;
    float import_left = -dialog.impl_width / 2;

    impl->upload = ro_single_new(r_texture_new_file(2, 1, "res/button_dialog_upload.png"));
    impl->upload_available = true;

    // Create label
    impl->upload_label = ro_text_new_font55(8);
    ro_text_set_text(&impl->upload_label, "Upload:");
    ro_text_set_color(&impl->upload_label, DIALOG_TEXT_COLOR);

    // Single row for unified upload button
    float btn_top = DIALOG_TOP - pos - 18;

    impl->upload_label.pose = u_pose_new(import_left + 8, btn_top + 6, 1, 2);
    impl->upload.rect.pose = u_pose_new_aa(import_left + dialog.impl_width - 8 - 64, btn_top, 64, 16);

    dialog.impl_height += 20;  // Add height for single button row
#endif


    dialog_set_title("import", TITLE_COLOR);
    dialog_set_bg_color(BG_A_COLOR, BG_B_COLOR);
    dialog.kill = kill_fn;
    dialog.update = update;
    dialog.render = render;
    dialog.pointer_event = pointer_event;
//    dialog.opt_on_cancel_cb = tooltip_on_action;
    dialog.opt_on_ok_cb = on_action;
}

