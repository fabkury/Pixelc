#include "e/io.h"
#include "r/r.h"
#include "u/pose.h"
#include "u/image.h"
#include "u/sprite.h"
#include "u/button.h"
#include "m/int.h"
#include "m/sca/float.h"
#include "io.h"
#include "modal.h"
#include "canvas.h"
#include "dialog.h"
#include "ext_gifenc.h"


static const uColor_s BG_A_COLOR = {{136, 136, 102, 255}};
static const uColor_s BG_B_COLOR = {{143, 143, 102, 255}};
static const vec4 TITLE_COLOR = {{0, 0.6, 0.3, 1}};

//
// private
//


typedef struct {

    RoText hd_multiplyer_txt;
    RoText hd_multiplyer_num;
    mat4 hd_multiplyer_hitbox;
    
    RoText merge_txt;
    RoSingle merge_toggle;
    
    RoText png_txt;
    RoSingle png_btn;
    RoSingle png_hd_btn;
    
    RoText gif_txt;
    RoSingle gif_btn;
    RoSingle gif_hd_btn;

    RoText project_txt;
    RoSingle project_btn;

    RoText makapix_txt;
    RoSingle makapix_btn;

    TextInput *textinput;
} Impl;

static void kill_fn() {
    Impl *impl = dialog.impl;
    
    ro_text_kill(&impl->hd_multiplyer_txt);
    ro_text_kill(&impl->hd_multiplyer_num);
    
    ro_text_kill(&impl->merge_txt);
    ro_single_kill(&impl->merge_toggle);
    
    ro_text_kill(&impl->png_txt);
    ro_single_kill(&impl->png_btn);
    ro_single_kill(&impl->png_hd_btn);
    
    ro_text_kill(&impl->gif_txt);
    ro_single_kill(&impl->gif_btn);
    ro_single_kill(&impl->gif_hd_btn);

    ro_text_kill(&impl->project_txt);
    ro_single_kill(&impl->project_btn);

    ro_text_kill(&impl->makapix_txt);
    ro_single_kill(&impl->makapix_btn);

    textinput_kill(&impl->textinput);
    
    s_free(impl);
}

static void update(float dtime) {
    Impl *impl = dialog.impl;
    if(impl->textinput) {
        char *end;
        int size = (int) strtol(impl->textinput->text, &end, 10);
        bool ok = end && end != impl->textinput->text && size>0 && size<=128;
        
        impl->textinput->ok_active = ok;
        
        if (impl->textinput->state == TEXTINPUT_DONE) {
            io.hd_multiplyer = size;
        }
        if (impl->textinput->state != TEXTINPUT_IN_PROGRESS) {
            textinput_kill(&impl->textinput);
            modal_set_textinput(NULL);
        }
    }
    
    char buf[16];
    snprintf(buf, sizeof buf, "%i", io.hd_multiplyer);
    ro_text_set_text(&impl->hd_multiplyer_num, buf);
    
    impl->merge_toggle.rect.sprite.x = io.image_save_merged? 1 : 0;
}

static void render(const mat4 *cam_mat) {
    Impl *impl = dialog.impl;

    ro_text_render(&impl->hd_multiplyer_txt, cam_mat);
    ro_text_render(&impl->hd_multiplyer_num, cam_mat);
    
    ro_text_render(&impl->merge_txt, cam_mat);
    ro_single_render(&impl->merge_toggle, cam_mat);
    
    ro_text_render(&impl->png_txt, cam_mat);
    ro_single_render(&impl->png_btn, cam_mat);
    ro_single_render(&impl->png_hd_btn, cam_mat);
    
    ro_text_render(&impl->gif_txt, cam_mat);
    ro_single_render(&impl->gif_btn, cam_mat);
    ro_single_render(&impl->gif_hd_btn, cam_mat);

    ro_text_render(&impl->project_txt, cam_mat);
    ro_single_render(&impl->project_btn, cam_mat);

    ro_text_render(&impl->makapix_txt, cam_mat);
    ro_single_render(&impl->makapix_btn, cam_mat);
}


static bool pointer_event(ePointer_s pointer) {
    Impl *impl = dialog.impl;
    
    if(pointer.id == 0
            && pointer.action == E_POINTER_DOWN
            && u_pose_aa_contains(impl->hd_multiplyer_hitbox, pointer.pos.xy)) {
        s_log("edit hd min size");
        impl->textinput = textinput_new("HD multiplyer", 0);
        modal_set_textinput(impl->textinput);
        snprintf(impl->textinput->text, TEXTINPUT_MAX_CHARS, "%i", io.hd_multiplyer);
        impl->textinput->shiftstate = TEXTINPUT_SHIFT_ALT;
    }
    
    if(u_button_toggled(&impl->merge_toggle.rect, pointer)) {
        bool merge = u_button_is_pressed(&impl->merge_toggle.rect);
        s_log("save merged toggled: %i", merge);
        io.image_save_merged = merge;
    }
    
    if(u_button_clicked(&impl->png_btn.rect, pointer)) {
        s_log("save png");
        io_image_save();
        dialog_hide();
        // return after hide, hide kills this dialog
        return true;
    }
    if(u_button_clicked(&impl->png_hd_btn.rect, pointer)) {
        s_log("save png hd");
        io_image_hd_save();
        dialog_hide();
        // return after hide, hide kills this dialog
        return true;
    }
    
    if(u_button_clicked(&impl->gif_btn.rect, pointer)) {
        s_log("save gif");
        io_gif_save();
        dialog_hide();
        // return after hide, hide kills this dialog
        return true;
    }
    if(u_button_clicked(&impl->gif_hd_btn.rect, pointer)) {
        s_log("save gif hd");
        io_gif_hd_save();
        dialog_hide();
        // return after hide, hide kills this dialog
        return true;
    }

    if(u_button_clicked(&impl->project_btn.rect, pointer)) {
        s_log("project btn");
        dialog_create_project();
        // return after create, create hides this dialog, which kills it
        return true;
    }

    if(u_button_clicked(&impl->makapix_btn.rect, pointer)) {
        s_log("export to makapix");
        uImage img;
        if (io.image_save_merged)
            img = canvas_get_merged_image();
        else
            img = canvas_get_full_image();

        int frame_count = canvas.RO.frames;
        if (frame_count > 1) {
            // Multiple frames: export as GIF
            s_log("exporting %d frames as gif", frame_count);
            uSprite sprite = u_sprite_new_reorder_from_image(frame_count, img);

            // Use the save_gif logic inline (simplified from io.c)
            int w = sprite.img.cols;
            int h = sprite.img.rows;
            ucvec3 *palette = s_new0(ucvec3, 256);
            int size = 1;

            // Build color palette
            for (int idx = 0; idx < w * h * sprite.img.layers; idx++) {
                uColor_s col = *u_image_pixel_index(sprite.img, idx, 0);
                if (col.a == 0) continue;
                bool found = false;
                for (int p = 1; p < size; p++) {
                    if (palette[p].x == col.r && palette[p].y == col.g && palette[p].z == col.b) {
                        found = true;
                        break;
                    }
                }
                if (!found && size < 256) {
                    palette[size] = col.rgb;
                    size++;
                }
            }

            ge_GIF *gif = ge_new_gif("makapix_export.gif", w, h, (su8 *)palette, 8, 0, 0);
            if (gif) {
                for (int frame = 0; frame < sprite.cols; frame++) {
                    for (int idx = 0; idx < w * h; idx++) {
                        uColor_s col = *u_image_pixel_index(sprite.img, idx, frame);
                        int col_id = 0;
                        if (col.a != 0) {
                            for (int p = 1; p < size; p++) {
                                if (palette[p].x == col.r && palette[p].y == col.g && palette[p].z == col.b) {
                                    col_id = p;
                                    break;
                                }
                            }
                        }
                        gif->frame[idx] = (su8) col_id;
                    }
                    ge_add_frame(gif, sca_ceil(100 * canvas.frame_times[frame]));
                }
                ge_close_gif(gif);
            }
            s_free(palette);

            e_io_export_to_makapix("makapix_export.gif", sprite.img.cols, sprite.img.rows, frame_count);
            u_sprite_kill(&sprite);
        } else {
            // Single frame: export as PNG
            s_log("exporting single frame as png");
            u_image_save_file(img, "makapix_export.png");
            e_io_export_to_makapix("makapix_export.png", img.cols, img.rows, 1);
        }
        u_image_kill(&img);
        dialog_hide();
        return true;
    }

    return true;
}

static void on_action(bool ok) {
    // cancel
    dialog_hide();
}

//
// public
//


void dialog_create_save() {
    dialog_hide();
    canvas_reload();
    s_log("create");
    Impl *impl = s_malloc0(sizeof *impl);
    dialog.impl = impl;
    
    int pos = 18;
    
    impl->hd_multiplyer_txt = ro_text_new_font55(16);
    ro_text_set_text(&impl->hd_multiplyer_txt, "hd multiplyer:");
    ro_text_set_color(&impl->hd_multiplyer_txt, DIALOG_TEXT_COLOR);
    impl->hd_multiplyer_num = ro_text_new_font55(8);
    ro_text_set_color(&impl->hd_multiplyer_num, DIALOG_TEXT_EDIT_COLOR);
    impl->hd_multiplyer_txt.pose = u_pose_new(DIALOG_LEFT + 6, DIALOG_TOP - pos, 1, 2);
    impl->hd_multiplyer_num.pose = u_pose_new(DIALOG_LEFT + 95, DIALOG_TOP - pos, 1, 2);
    impl->hd_multiplyer_hitbox = u_pose_new_aa(DIALOG_LEFT, DIALOG_TOP - pos + 4, DIALOG_WIDTH, 10 + 8);
    
    pos += 16;

    impl->merge_txt = ro_text_new_font55(32);
    ro_text_set_color(&impl->merge_txt, DIALOG_TEXT_COLOR);
    ro_text_set_text(&impl->merge_txt, "save layers\n"
            "      merged?");
    impl->merge_txt.pose = u_pose_new(DIALOG_LEFT+6, DIALOG_TOP - pos, 1, 2);
    
    impl->merge_toggle = ro_single_new(r_texture_new_file(2, 1, "res/button_ok.png"));
    impl->merge_toggle.rect.pose = u_pose_new_aa(
            DIALOG_LEFT + DIALOG_WIDTH - 20, 
            DIALOG_TOP - pos - 4, 
            16, 16);
            
    pos += 30;
    
    impl->png_txt = ro_text_new_font55(16);
    ro_text_set_text(&impl->png_txt, "save .png:");
    
    ro_text_set_color(&impl->png_txt, DIALOG_TEXT_COLOR);
    impl->png_txt.pose = u_pose_new(DIALOG_LEFT + 8, DIALOG_TOP - pos - 2, 1, 2);

    impl->png_btn = ro_single_new(r_texture_new_file(2, 1, "res/button_save.png"));
    impl->png_btn.rect.pose = u_pose_new_aa(DIALOG_LEFT + DIALOG_WIDTH - 40, DIALOG_TOP - pos, 16, 16);
    
    impl->png_hd_btn = ro_single_new(r_texture_new_file(2, 1, "res/button_save_hd.png"));
    impl->png_hd_btn.rect.pose = u_pose_new_aa(DIALOG_LEFT + DIALOG_WIDTH - 20, DIALOG_TOP - pos, 16, 16);
    
    pos += 18;
    
    impl->gif_txt = ro_text_new_font55(16);
    ro_text_set_text(&impl->gif_txt, "save .gif:");
    
    ro_text_set_color(&impl->gif_txt, DIALOG_TEXT_COLOR);
    impl->gif_txt.pose = u_pose_new(DIALOG_LEFT + 8, DIALOG_TOP - pos - 2, 1, 2);

    impl->gif_btn = ro_single_new(r_texture_new_file(2, 1, "res/button_save.png"));
    impl->gif_btn.rect.pose = u_pose_new_aa(DIALOG_LEFT + DIALOG_WIDTH - 40, DIALOG_TOP - pos, 16, 16);
    
    impl->gif_hd_btn = ro_single_new(r_texture_new_file(2, 1, "res/button_save_hd.png"));
    impl->gif_hd_btn.rect.pose = u_pose_new_aa(DIALOG_LEFT + DIALOG_WIDTH - 20, DIALOG_TOP - pos, 16, 16);

    pos += 18;

    impl->project_txt = ro_text_new_font55(16);
    ro_text_set_text(&impl->project_txt, "Full Project:");
    ro_text_set_color(&impl->project_txt, DIALOG_TEXT_COLOR);
    impl->project_txt.pose = u_pose_new(DIALOG_LEFT + 8, DIALOG_TOP - pos - 2, 1, 2);
    impl->project_btn = ro_single_new(r_texture_new_file(2, 1, "res/button_project.png"));
    impl->project_btn.rect.pose = u_pose_new_aa(DIALOG_LEFT + DIALOG_WIDTH - 20, DIALOG_TOP - pos, 16, 16);

    pos += 18;

    impl->makapix_txt = ro_text_new_font55(20);
    ro_text_set_text(&impl->makapix_txt, "Makapix Club:");
    ro_text_set_color(&impl->makapix_txt, DIALOG_TEXT_COLOR);
    impl->makapix_txt.pose = u_pose_new(DIALOG_LEFT + 8, DIALOG_TOP - pos - 2, 1, 2);
    impl->makapix_btn = ro_single_new(r_texture_new_file(2, 1, "res/button_save.png"));
    impl->makapix_btn.rect.pose = u_pose_new_aa(DIALOG_LEFT + DIALOG_WIDTH - 20, DIALOG_TOP - pos, 16, 16);

    pos += 4;
    
    dialog.impl_height = pos;

    dialog_set_title("save", TITLE_COLOR);
    dialog_set_bg_color(BG_A_COLOR, BG_B_COLOR);
    dialog.kill = kill_fn;
    dialog.update = update;
    dialog.render = render;
    dialog.pointer_event = pointer_event;
    dialog.opt_on_ok_cb = on_action;
}

