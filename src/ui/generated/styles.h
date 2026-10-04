#ifndef EEZ_LVGL_UI_STYLES_H
#define EEZ_LVGL_UI_STYLES_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Style: Button
lv_style_t *get_style_button_MAIN_DEFAULT();
void add_style_button(lv_obj_t *obj);
void remove_style_button(lv_obj_t *obj);

// Style: Button primary
lv_style_t *get_style_button_primary_MAIN_DEFAULT();
void add_style_button_primary(lv_obj_t *obj);
void remove_style_button_primary(lv_obj_t *obj);

// Style: Button secondary
lv_style_t *get_style_button_secondary_MAIN_DEFAULT();
void add_style_button_secondary(lv_obj_t *obj);
void remove_style_button_secondary(lv_obj_t *obj);

// Style: Button danger
lv_style_t *get_style_button_danger_MAIN_CHECKED();
lv_style_t *get_style_button_danger_MAIN_DEFAULT();
void add_style_button_danger(lv_obj_t *obj);
void remove_style_button_danger(lv_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_STYLES_H*/