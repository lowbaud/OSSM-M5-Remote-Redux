#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_BOOT = 1,
    SCREEN_ID_WELCOME = 2,
    SCREEN_ID_SCAN = 3,
    SCREEN_ID_CONNECT = 4,
    SCREEN_ID_OSSM_CONTROL = 5,
    SCREEN_ID_OSSM_PATTERNS = 6,
    SCREEN_ID_SETTINGS = 7,
    _SCREEN_ID_LAST = 7
};

typedef struct _objects_t {
    lv_obj_t *boot;
    lv_obj_t *welcome;
    lv_obj_t *scan;
    lv_obj_t *connect;
    lv_obj_t *ossm_control;
    lv_obj_t *ossm_patterns;
    lv_obj_t *settings;
    lv_obj_t *redux_logo_img;
    lv_obj_t *boot_message_lbl;
    lv_obj_t *boot_version_lbl;
    lv_obj_t *welcome_scan_btn;
    lv_obj_t *welcome_battery_lbl;
    lv_obj_t *welcome_settings_btn;
    lv_obj_t *scan_device_list;
    lv_obj_t *scan_status_label;
    lv_obj_t *scan_connect_btn;
    lv_obj_t *scan_cancel_btn;
    lv_obj_t *connect_cancel_btn;
    lv_obj_t *connect_device_name_lbl;
    lv_obj_t *connect_device_address_lbl;
    lv_obj_t *connect_status_lbl;
    lv_obj_t *connect_spinner;
    lv_obj_t *ossm_control_speed_value_lbl;
    lv_obj_t *ossm_control_sensation_value_lbl;
    lv_obj_t *ossm_control_motion_range_slider;
    lv_obj_t *obj0;
    lv_obj_t *obj1;
    lv_obj_t *oss_control_stop_btn;
    lv_obj_t *ossm_control_pattern_lbl;
    lv_obj_t *ossm_control_patterns_btn;
    lv_obj_t *ossm_control_battery_lbl;
    lv_obj_t *obj2;
    lv_obj_t *ossm_control_settings_btn;
    lv_obj_t *ossm_patterns_list;
    lv_obj_t *ossm_patterns_cancel_btn;
    lv_obj_t *ossm_patterns_select_btn;
    lv_obj_t *oss_patterns_stop_btn;
    lv_obj_t *settings_list;
    lv_obj_t *settings_back_btn;
    lv_obj_t *settings_select_btn;
    lv_obj_t *settings_stop_btn;
    lv_obj_t *settings_options;
} objects_t;

extern objects_t objects;

void create_screen_boot();
void tick_screen_boot();

void create_screen_welcome();
void tick_screen_welcome();

void create_screen_scan();
void tick_screen_scan();

void create_screen_connect();
void tick_screen_connect();

void create_screen_ossm_control();
void tick_screen_ossm_control();

void create_screen_ossm_patterns();
void tick_screen_ossm_patterns();

void create_screen_settings();
void tick_screen_settings();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/