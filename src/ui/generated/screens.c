#include <string.h>

#include "screens.h"
#include "images.h"
#include "fonts.h"
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"

#include <string.h>

objects_t objects;

//
// Event handlers
//

lv_obj_t *tick_value_change_obj;

//
// Screens
//

void create_screen_boot() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.boot = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // redux_logo_img
            lv_obj_t *obj = lv_image_create(parent_obj);
            objects.redux_logo_img = obj;
            lv_obj_set_pos(obj, 96, 49);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_image_set_src(obj, &img_redux_logo);
            lv_obj_set_style_width(obj, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_height(obj, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // boot_message_lbl
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.boot_message_lbl = obj;
            lv_obj_set_pos(obj, 20, 190);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_width(obj, 280, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "Hold left encoder to skip auto-connect.");
        }
        {
            // boot_version_lbl
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.boot_version_lbl = obj;
            lv_obj_set_pos(obj, 60, 213);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_width(obj, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "v0.0.0");
        }
    }
    
    tick_screen_boot();
}

void tick_screen_boot() {
}

void create_screen_welcome() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.welcome = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // welcome_scan_btn
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.welcome_scan_btn = obj;
            lv_obj_set_pos(obj, 212, 207);
            lv_obj_set_size(obj, 98, 26);
            lv_obj_add_event_cb(obj, action_welcome_scan, LV_EVENT_CLICKED, (void *)0);
            add_style_button_primary(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Scan");
                }
            }
        }
        {
            // welcome_battery_lbl
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.welcome_battery_lbl = obj;
            lv_obj_set_pos(obj, 282, 4);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_22, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "");
        }
        {
            // welcome_title_lbl
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.welcome_title_lbl = obj;
            lv_obj_set_pos(obj, 0, 86);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_22, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_width(obj, 320, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "Welcome");
        }
        {
            // welcome_hint_lbl
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.welcome_hint_lbl = obj;
            lv_obj_set_pos(obj, 0, 123);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_width(obj, 320, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "Press Scan to connect");
        }
        {
            // welcome_settings_btn
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.welcome_settings_btn = obj;
            lv_obj_set_pos(obj, 10, 207);
            lv_obj_set_size(obj, 98, 26);
            lv_obj_add_event_cb(obj, action_welcome_settings, LV_EVENT_CLICKED, (void *)0);
            add_style_button_secondary(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Settings");
                }
            }
        }
    }
    
    tick_screen_welcome();
}

void tick_screen_welcome() {
}

void create_screen_scan() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.scan = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // scan_device_list
            lv_obj_t *obj = lv_list_create(parent_obj);
            objects.scan_device_list = obj;
            lv_obj_set_pos(obj, 0, 34);
            lv_obj_set_size(obj, 320, 166);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // scan_status_label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.scan_status_label = obj;
            lv_obj_set_pos(obj, 10, 8);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_width(obj, 250, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Scanning...");
        }
        {
            // scan_connect_btn
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.scan_connect_btn = obj;
            lv_obj_set_pos(obj, 212, 207);
            lv_obj_set_size(obj, 98, 26);
            lv_obj_add_event_cb(obj, action_scan_connect, LV_EVENT_CLICKED, (void *)0);
            lv_obj_add_state(obj, LV_STATE_DISABLED);
            add_style_button_primary(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Connect");
                }
            }
        }
        {
            // scan_cancel_btn
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.scan_cancel_btn = obj;
            lv_obj_set_pos(obj, 10, 207);
            lv_obj_set_size(obj, 98, 26);
            lv_obj_add_event_cb(obj, action_scan_cancel, LV_EVENT_CLICKED, (void *)0);
            add_style_button_secondary(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Cancel");
                }
            }
        }
        {
            // scan_spinner
            lv_obj_t *obj = lv_spinner_create(parent_obj);
            objects.scan_spinner = obj;
            lv_obj_set_pos(obj, 292, 7);
            lv_obj_set_size(obj, 18, 18);
            lv_obj_set_style_arc_width(obj, 2, LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_color(obj, lv_color_hex(theme_colors[active_theme_index][6]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_color(obj, lv_color_hex(theme_colors[active_theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_opa(obj, 76, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
    
    tick_screen_scan();
}

void tick_screen_scan() {
}

void create_screen_connect() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.connect = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // connect_cancel_btn
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.connect_cancel_btn = obj;
            lv_obj_set_pos(obj, 10, 207);
            lv_obj_set_size(obj, 98, 26);
            lv_obj_add_event_cb(obj, action_connect_cancel, LV_EVENT_CLICKED, (void *)0);
            add_style_button_secondary(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Cancel");
                }
            }
        }
        {
            // connect_device_name_lbl
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.connect_device_name_lbl = obj;
            lv_obj_set_pos(obj, 10, 130);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_width(obj, 300, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "Device Name");
        }
        {
            // connect_device_address_lbl
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.connect_device_address_lbl = obj;
            lv_obj_set_pos(obj, 10, 148);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_width(obj, 300, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "BLE Address");
        }
        {
            // connect_status_lbl
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.connect_status_lbl = obj;
            lv_obj_set_pos(obj, 10, 100);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_width(obj, 300, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "Connecting...");
        }
        {
            // connect_spinner
            lv_obj_t *obj = lv_spinner_create(parent_obj);
            objects.connect_spinner = obj;
            lv_obj_set_pos(obj, 140, 44);
            lv_obj_set_size(obj, 40, 40);
            lv_obj_set_style_arc_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_color(obj, lv_color_hex(theme_colors[active_theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_opa(obj, 76, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_width(obj, 4, LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_color(obj, lv_color_hex(theme_colors[active_theme_index][6]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        }
    }
    
    tick_screen_connect();
}

void tick_screen_connect() {
}

void create_screen_ossm_control() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.ossm_control = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // ossm_control_speed_value_lbl
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.ossm_control_speed_value_lbl = obj;
            lv_obj_set_pos(obj, 30, 81);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_chivo_mono_60, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_width(obj, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "100");
        }
        {
            // ossm_control_sensation_value_lbl
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.ossm_control_sensation_value_lbl = obj;
            lv_obj_set_pos(obj, 170, 81);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_chivo_mono_60, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_width(obj, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "100");
        }
        {
            // ossm_control_motion_range_slider
            lv_obj_t *obj = lv_slider_create(parent_obj);
            objects.ossm_control_motion_range_slider = obj;
            lv_obj_set_pos(obj, 53, 173);
            lv_obj_set_size(obj, 214, 8);
            lv_slider_set_mode(obj, LV_SLIDER_MODE_RANGE);
            lv_slider_set_value(obj, 80, LV_ANIM_OFF);
            lv_slider_set_start_value(obj, 50, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][6]), LV_PART_KNOB | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][6]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][3]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // ossm_control_speed_lbl
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.ossm_control_speed_lbl = obj;
            lv_obj_set_pos(obj, 30, 53);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_width(obj, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "SPEED");
        }
        {
            // ossm_control_sensation_lbl
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.ossm_control_sensation_lbl = obj;
            lv_obj_set_pos(obj, 170, 53);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_width(obj, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "SENSATION");
        }
        {
            // ossm_control_stop_btn
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.ossm_control_stop_btn = obj;
            lv_obj_set_pos(obj, 111, 207);
            lv_obj_set_size(obj, 98, 26);
            lv_obj_add_event_cb(obj, action_stop, LV_EVENT_PRESSED, (void *)0);
            add_style_button_danger(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Stop");
                }
            }
        }
        {
            // ossm_control_pattern_lbl
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.ossm_control_pattern_lbl = obj;
            lv_obj_set_pos(obj, 40, 8);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_width(obj, 240, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "Robostroke");
        }
        {
            // ossm_control_patterns_btn
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.ossm_control_patterns_btn = obj;
            lv_obj_set_pos(obj, 212, 207);
            lv_obj_set_size(obj, 98, 26);
            lv_obj_add_event_cb(obj, action_ossm_control_patterns, LV_EVENT_CLICKED, (void *)0);
            add_style_button_secondary(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Patterns");
                }
            }
        }
        {
            // ossm_control_battery_lbl
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.ossm_control_battery_lbl = obj;
            lv_obj_set_pos(obj, 282, 4);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_22, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "");
        }
        {
            // ossm_control_motion_range_lbl
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.ossm_control_motion_range_lbl = obj;
            lv_obj_set_pos(obj, 60, 145);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_width(obj, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "STROKE / DEPTH");
        }
        {
            // ossm_control_settings_btn
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.ossm_control_settings_btn = obj;
            lv_obj_set_pos(obj, 10, 207);
            lv_obj_set_size(obj, 98, 26);
            lv_obj_add_event_cb(obj, action_ossm_control_settings, LV_EVENT_CLICKED, (void *)0);
            add_style_button_secondary(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Settings");
                }
            }
        }
    }
    
    tick_screen_ossm_control();
}

void tick_screen_ossm_control() {
}

void create_screen_ossm_patterns() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.ossm_patterns = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // ossm_patterns_list
            lv_obj_t *obj = lv_list_create(parent_obj);
            objects.ossm_patterns_list = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, 320, 200);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // ossm_patterns_cancel_btn
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.ossm_patterns_cancel_btn = obj;
            lv_obj_set_pos(obj, 10, 207);
            lv_obj_set_size(obj, 98, 26);
            lv_obj_add_event_cb(obj, action_ossm_patterns_cancel, LV_EVENT_CLICKED, (void *)0);
            add_style_button_secondary(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Cancel");
                }
            }
        }
        {
            // ossm_patterns_select_btn
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.ossm_patterns_select_btn = obj;
            lv_obj_set_pos(obj, 212, 207);
            lv_obj_set_size(obj, 98, 26);
            lv_obj_add_event_cb(obj, action_ossm_patterns_select, LV_EVENT_CLICKED, (void *)0);
            lv_obj_add_state(obj, LV_STATE_DISABLED);
            add_style_button_primary(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Select");
                }
            }
        }
        {
            // ossm_patterns_stop_btn
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.ossm_patterns_stop_btn = obj;
            lv_obj_set_pos(obj, 111, 207);
            lv_obj_set_size(obj, 98, 26);
            lv_obj_add_event_cb(obj, action_stop, LV_EVENT_PRESSED, (void *)0);
            add_style_button_danger(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Stop");
                }
            }
        }
    }
    
    tick_screen_ossm_patterns();
}

void tick_screen_ossm_patterns() {
}

void create_screen_settings() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.settings = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // settings_list
            lv_obj_t *obj = lv_list_create(parent_obj);
            objects.settings_list = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, 320, 200);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // settings_options_scrim
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.settings_options_scrim = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, 320, 240);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // settings_options
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.settings_options = obj;
                    lv_obj_set_pos(obj, 0, -20);
                    lv_obj_set_size(obj, 263, 26);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][11]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_clip_corner(obj, true, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][10]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
            }
        }
        {
            // settings_back_btn
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.settings_back_btn = obj;
            lv_obj_set_pos(obj, 10, 207);
            lv_obj_set_size(obj, 98, 26);
            lv_obj_add_event_cb(obj, action_settings_back, LV_EVENT_CLICKED, (void *)0);
            add_style_button_secondary(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Back");
                }
            }
        }
        {
            // settings_select_btn
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.settings_select_btn = obj;
            lv_obj_set_pos(obj, 212, 207);
            lv_obj_set_size(obj, 98, 26);
            lv_obj_add_event_cb(obj, action_settings_select, LV_EVENT_CLICKED, (void *)0);
            lv_obj_add_state(obj, LV_STATE_DISABLED);
            add_style_button_primary(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Select");
                }
            }
        }
        {
            // settings_stop_btn
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.settings_stop_btn = obj;
            lv_obj_set_pos(obj, 111, 207);
            lv_obj_set_size(obj, 98, 26);
            lv_obj_add_event_cb(obj, action_stop, LV_EVENT_PRESSED, (void *)0);
            add_style_button_danger(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Stop");
                }
            }
        }
    }
    
    tick_screen_settings();
}

void tick_screen_settings() {
}

void create_screen_diagnostics() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.diagnostics = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // diagnostics_panel
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.diagnostics_panel = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, 320, 200);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // diagnostics_back_btn
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.diagnostics_back_btn = obj;
            lv_obj_set_pos(obj, 10, 207);
            lv_obj_set_size(obj, 98, 26);
            lv_obj_add_event_cb(obj, action_diagnostics_back, LV_EVENT_CLICKED, (void *)0);
            add_style_button_secondary(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Back");
                }
            }
        }
        {
            // diagnostics_action_btn
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.diagnostics_action_btn = obj;
            lv_obj_set_pos(obj, 212, 207);
            lv_obj_set_size(obj, 98, 26);
            lv_obj_add_event_cb(obj, action_diagnostics_action, LV_EVENT_CLICKED, (void *)0);
            add_style_button_primary(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Log");
                }
            }
        }
        {
            // diagnostics_stop_btn
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.diagnostics_stop_btn = obj;
            lv_obj_set_pos(obj, 111, 207);
            lv_obj_set_size(obj, 98, 26);
            lv_obj_add_event_cb(obj, action_stop, LV_EVENT_PRESSED, (void *)0);
            add_style_button_danger(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Stop");
                }
            }
        }
    }
    
    tick_screen_diagnostics();
}

void tick_screen_diagnostics() {
}

typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_boot,
    tick_screen_welcome,
    tick_screen_scan,
    tick_screen_connect,
    tick_screen_ossm_control,
    tick_screen_ossm_patterns,
    tick_screen_settings,
    tick_screen_diagnostics,
};
void tick_screen(int screen_index) {
    if (screen_index >= 0 && screen_index < 8) {
        tick_screen_funcs[screen_index]();
    }
}
void tick_screen_by_id(enum ScreensEnum screenId) {
    tick_screen(screenId - 1);
}

//
// Fonts
//

ext_font_desc_t fonts[] = {
    { "Chivo Mono 60", &ui_font_chivo_mono_60 },
    { "AMI EGA 8x8", &ui_font_ami_ega_8x8 },
    { "DOS-V JPN12", &ui_font_dos_v_jpn12 },
#if LV_FONT_MONTSERRAT_8
    { "MONTSERRAT_8", &lv_font_montserrat_8 },
#endif
#if LV_FONT_MONTSERRAT_10
    { "MONTSERRAT_10", &lv_font_montserrat_10 },
#endif
#if LV_FONT_MONTSERRAT_12
    { "MONTSERRAT_12", &lv_font_montserrat_12 },
#endif
#if LV_FONT_MONTSERRAT_14
    { "MONTSERRAT_14", &lv_font_montserrat_14 },
#endif
#if LV_FONT_MONTSERRAT_16
    { "MONTSERRAT_16", &lv_font_montserrat_16 },
#endif
#if LV_FONT_MONTSERRAT_18
    { "MONTSERRAT_18", &lv_font_montserrat_18 },
#endif
#if LV_FONT_MONTSERRAT_20
    { "MONTSERRAT_20", &lv_font_montserrat_20 },
#endif
#if LV_FONT_MONTSERRAT_22
    { "MONTSERRAT_22", &lv_font_montserrat_22 },
#endif
#if LV_FONT_MONTSERRAT_24
    { "MONTSERRAT_24", &lv_font_montserrat_24 },
#endif
#if LV_FONT_MONTSERRAT_26
    { "MONTSERRAT_26", &lv_font_montserrat_26 },
#endif
#if LV_FONT_MONTSERRAT_28
    { "MONTSERRAT_28", &lv_font_montserrat_28 },
#endif
#if LV_FONT_MONTSERRAT_30
    { "MONTSERRAT_30", &lv_font_montserrat_30 },
#endif
#if LV_FONT_MONTSERRAT_32
    { "MONTSERRAT_32", &lv_font_montserrat_32 },
#endif
#if LV_FONT_MONTSERRAT_34
    { "MONTSERRAT_34", &lv_font_montserrat_34 },
#endif
#if LV_FONT_MONTSERRAT_36
    { "MONTSERRAT_36", &lv_font_montserrat_36 },
#endif
#if LV_FONT_MONTSERRAT_38
    { "MONTSERRAT_38", &lv_font_montserrat_38 },
#endif
#if LV_FONT_MONTSERRAT_40
    { "MONTSERRAT_40", &lv_font_montserrat_40 },
#endif
#if LV_FONT_MONTSERRAT_42
    { "MONTSERRAT_42", &lv_font_montserrat_42 },
#endif
#if LV_FONT_MONTSERRAT_44
    { "MONTSERRAT_44", &lv_font_montserrat_44 },
#endif
#if LV_FONT_MONTSERRAT_46
    { "MONTSERRAT_46", &lv_font_montserrat_46 },
#endif
#if LV_FONT_MONTSERRAT_48
    { "MONTSERRAT_48", &lv_font_montserrat_48 },
#endif
};

//
// Color themes
//

uint32_t active_theme_index = 0;
void change_color_theme(uint32_t theme_index) {
    active_theme_index = theme_index;
    
    {
        lv_obj_set_style_bg_color(objects.boot, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.boot_message_lbl, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.boot_version_lbl, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    {
        lv_obj_set_style_bg_color(objects.welcome, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.welcome_title_lbl, lv_color_hex(theme_colors[theme_index][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.welcome_hint_lbl, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    {
        lv_obj_set_style_bg_color(objects.scan, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.scan_status_label, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_color(objects.scan_spinner, lv_color_hex(theme_colors[theme_index][6]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_color(objects.scan_spinner, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    {
        lv_obj_set_style_bg_color(objects.connect, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.connect_device_name_lbl, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.connect_device_address_lbl, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.connect_status_lbl, lv_color_hex(theme_colors[theme_index][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_color(objects.connect_spinner, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_color(objects.connect_spinner, lv_color_hex(theme_colors[theme_index][6]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }
    {
        lv_obj_set_style_bg_color(objects.ossm_control, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.ossm_control_speed_value_lbl, lv_color_hex(theme_colors[theme_index][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.ossm_control_sensation_value_lbl, lv_color_hex(theme_colors[theme_index][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.ossm_control_motion_range_slider, lv_color_hex(theme_colors[theme_index][6]), LV_PART_KNOB | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.ossm_control_motion_range_slider, lv_color_hex(theme_colors[theme_index][6]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.ossm_control_motion_range_slider, lv_color_hex(theme_colors[theme_index][3]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.ossm_control_speed_lbl, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.ossm_control_sensation_lbl, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.ossm_control_pattern_lbl, lv_color_hex(theme_colors[theme_index][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.ossm_control_motion_range_lbl, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    {
        lv_obj_set_style_bg_color(objects.ossm_patterns, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    {
        lv_obj_set_style_bg_color(objects.settings, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.settings_options, lv_color_hex(theme_colors[theme_index][11]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.settings_options, lv_color_hex(theme_colors[theme_index][10]), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    {
        lv_obj_set_style_bg_color(objects.diagnostics, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    lv_style_set_text_color(get_style_button_MAIN_DEFAULT(), lv_color_hex(theme_colors[theme_index][4]));
    lv_style_set_text_color(get_style_button_primary_MAIN_DEFAULT(), lv_color_hex(theme_colors[theme_index][4]));
    lv_style_set_text_color(get_style_button_secondary_MAIN_DEFAULT(), lv_color_hex(theme_colors[theme_index][4]));
    lv_style_set_bg_color(get_style_button_primary_MAIN_DEFAULT(), lv_color_hex(theme_colors[theme_index][7]));
    lv_style_set_border_color(get_style_button_primary_MAIN_DEFAULT(), lv_color_lighten(lv_color_hex(theme_colors[theme_index][7]), 26));
    lv_style_set_bg_color(get_style_button_secondary_MAIN_DEFAULT(), lv_color_hex(theme_colors[theme_index][8]));
    lv_style_set_border_color(get_style_button_secondary_MAIN_DEFAULT(), lv_color_lighten(lv_color_hex(theme_colors[theme_index][8]), 26));
    lv_style_set_bg_color(get_style_button_danger_MAIN_CHECKED(), lv_color_hex(theme_colors[theme_index][9]));
    lv_style_set_border_color(get_style_button_danger_MAIN_CHECKED(), lv_color_lighten(lv_color_hex(theme_colors[theme_index][9]), 26));
    lv_style_set_text_color(get_style_button_danger_MAIN_DEFAULT(), lv_color_hex(theme_colors[theme_index][5]));
    lv_style_set_bg_color(get_style_button_danger_MAIN_DEFAULT(), lv_color_hex(theme_colors[theme_index][11]));
    lv_obj_invalidate(objects.boot);
    lv_obj_invalidate(objects.welcome);
    lv_obj_invalidate(objects.scan);
    lv_obj_invalidate(objects.connect);
    lv_obj_invalidate(objects.ossm_control);
    lv_obj_invalidate(objects.ossm_patterns);
    lv_obj_invalidate(objects.settings);
    lv_obj_invalidate(objects.diagnostics);
}
uint32_t theme_colors[1][12] = {
    { 0xff121a22, 0xff1f2e3e, 0xff1e232b, 0xff2c323a, 0xffd8dce2, 0xff7a838f, 0xff1eb4f0, 0xff0e8fc4, 0xff2f3d4d, 0xffdc3545, 0xff4a5260, 0xff1c232d },
};

//
//
//

void create_screens() {

// Set default LVGL theme
    lv_display_t *dispp = lv_display_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);
    lv_display_set_theme(dispp, theme);
    
    // Initialize screens
    // Create screens
    create_screen_boot();
    create_screen_welcome();
    create_screen_scan();
    create_screen_connect();
    create_screen_ossm_control();
    create_screen_ossm_patterns();
    create_screen_settings();
    create_screen_diagnostics();
}