#include "e/io.h"
#include "r/r.h"
#include "u/pose.h"
#include "u/image.h"
#include "u/button.h"
#include "canvas.h"
#include "cameractrl.h"
#include "selectionctrl.h"
#include "dialog.h"
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

    RoText to_canvas_txt;
    RoSingle to_canvas_btn;

    RoText as_selection_txt;
    RoSingle as_selection_btn;

    RoSingle upload;
    RoSingle upload_webp;
    RoText upload_label;
    RoText upload_webp_label;
    bool upload_available;

} Impl;


static void kill_fn() {
    Impl *impl = dialog.impl;
    ro_text_kill(&impl->info);
    ro_single_kill(&impl->import);
    ro_single_kill(&impl->upload);
    ro_single_kill(&impl->upload_webp);
    ro_text_kill(&impl->upload_label);
    ro_text_kill(&impl->upload_webp_label);
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
        ro_text_render(&impl->upload_webp_label, cam_mat);
        ro_single_render(&impl->upload_webp, cam_mat);
    }
}

static void uploaded(const char *file, bool ascii, const char *user_file_name, void *user_data) {
    dialog_create_import();
}

static void uploaded_webp(const char *file, bool ascii, const char *user_file_name, void *user_data) {
    // Convert the webp to png for the import system or handle animated webp
    s_log("webp uploaded: %s", file);

    if (webp_is_animated(file)) {
        s_log("animated webp detected");
        // For animated webp, we'll load it directly when the user clicks import
        // Just recreate the dialog to show the preview
    } else {
        // For static webp, convert to png for compatibility with existing import
        uImage img = webp_load_image(file);
        if (u_image_valid(img)) {
            u_image_save_file(img, "import.png");
            u_image_kill(&img);
        }
    }

    dialog_create_import();
}

static bool pointer_event(ePointer_s pointer) {
    Impl *impl = dialog.impl;

    if (impl->upload_available && u_button_clicked(&impl->upload.rect, pointer)) {
        s_log("import upload...");
        e_io_ask_for_file_upload("import.png", false, uploaded, NULL);
        // return after hide, hide kills this dialog
        return true;
    }

    if (impl->upload_available && u_button_clicked(&impl->upload_webp.rect, pointer)) {
        s_log("import webp upload...");
        e_io_ask_for_file_upload("import.webp", false, uploaded_webp, NULL);
        // return after hide, hide kills this dialog
        return true;
    }

    if (impl->import_available && u_button_clicked(&impl->to_canvas_btn.rect, pointer)) {
        s_log("import to canvas");

        if (impl->is_animated_webp) {
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

    // Check for animated webp first
    impl->is_animated_webp = webp_is_animated("import.webp");

    uImage img;
    if (impl->is_animated_webp) {
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
        ro_text_set_text(&impl->info, "failed to load\n"
                                      "import image");
        impl->info.pose = u_pose_new(DIALOG_LEFT + 8, DIALOG_TOP - pos - 4, 1, 2);
        pos += 26;

    } else {

        char text[128];
        if (impl->is_animated_webp) {
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

        if (impl->is_animated_webp) {
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
    // Use wider dialog to fit labels and buttons
    dialog.impl_width = 150;
    float import_left = -dialog.impl_width / 2;

    impl->upload = ro_single_new(r_texture_new_file(2, 1, "res/button_dialog_upload.png"));
    impl->upload_webp = ro_single_new(r_texture_new_file(2, 1, "res/button_dialog_upload.png"));
    impl->upload_available = true;

    // Create labels
    impl->upload_label = ro_text_new_font55(8);
    ro_text_set_text(&impl->upload_label, "PNG:");
    ro_text_set_color(&impl->upload_label, DIALOG_TEXT_COLOR);

    impl->upload_webp_label = ro_text_new_font55(8);
    ro_text_set_text(&impl->upload_webp_label, "WEBP:");
    ro_text_set_color(&impl->upload_webp_label, DIALOG_TEXT_COLOR);

    // Stack buttons vertically with labels on the left
    float btn_top1 = DIALOG_TOP - pos - 18;
    float btn_top2 = DIALOG_TOP - pos - 40;

    impl->upload_label.pose = u_pose_new(import_left + 8, btn_top1 + 6, 1, 2);
    impl->upload.rect.pose = u_pose_new_aa(import_left + dialog.impl_width - 8 - 64, btn_top1, 64, 16);

    impl->upload_webp_label.pose = u_pose_new(import_left + 8, btn_top2 + 6, 1, 2);
    impl->upload_webp.rect.pose = u_pose_new_aa(import_left + dialog.impl_width - 8 - 64, btn_top2, 64, 16);

    dialog.impl_height += 40;  // Add extra height for vertically stacked buttons
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

