#include "styles.h"
#include "images.h"
#include "fonts.h"

#include "ui.h"
#include "screens.h"

//
// Style: Button
//

void init_style_button_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_radius(style, 10);
    lv_style_set_shadow_width(style, 0);
    lv_style_set_border_width(style, 0);
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][4]));
};

lv_style_t *get_style_button_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_button_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_button(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_button_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_button(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_button_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: Button primary
//

void init_style_button_primary_MAIN_DEFAULT(lv_style_t *style) {
    init_style_button_MAIN_DEFAULT(style);
    
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][7]));
    lv_style_set_border_color(style, lv_color_lighten(lv_color_hex(theme_colors[active_theme_index][7]), 26));
    lv_style_set_border_width(style, 1);
};

lv_style_t *get_style_button_primary_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_button_primary_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_button_primary(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_button_primary_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_button_primary(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_button_primary_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: Button secondary
//

void init_style_button_secondary_MAIN_DEFAULT(lv_style_t *style) {
    init_style_button_MAIN_DEFAULT(style);
    
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][8]));
    lv_style_set_border_width(style, 1);
    lv_style_set_border_color(style, lv_color_lighten(lv_color_hex(theme_colors[active_theme_index][8]), 26));
};

lv_style_t *get_style_button_secondary_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_button_secondary_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_button_secondary(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_button_secondary_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_button_secondary(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_button_secondary_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: Button danger
//

void init_style_button_danger_MAIN_CHECKED(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][9]));
    lv_style_set_bg_opa(style, 255);
    lv_style_set_text_color(style, lv_color_hex(0xffffff));
    lv_style_set_border_color(style, lv_color_lighten(lv_color_hex(theme_colors[active_theme_index][9]), 26));
    lv_style_set_border_width(style, 1);
};

lv_style_t *get_style_button_danger_MAIN_CHECKED() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_button_danger_MAIN_CHECKED(style);
    }
    return style;
};

void init_style_button_danger_MAIN_DEFAULT(lv_style_t *style) {
    init_style_button_MAIN_DEFAULT(style);
    
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][5]));
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][11]));
};

lv_style_t *get_style_button_danger_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_button_danger_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_button_danger(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_button_danger_MAIN_CHECKED(), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_add_style(obj, get_style_button_danger_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_button_danger(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_button_danger_MAIN_CHECKED(), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_remove_style(obj, get_style_button_danger_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
//
//

void add_style(lv_obj_t *obj, int32_t styleIndex) {
    typedef void (*AddStyleFunc)(lv_obj_t *obj);
    static const AddStyleFunc add_style_funcs[] = {
        add_style_button,
        add_style_button_primary,
        add_style_button_secondary,
        add_style_button_danger,
    };
    add_style_funcs[styleIndex](obj);
}

void remove_style(lv_obj_t *obj, int32_t styleIndex) {
    typedef void (*RemoveStyleFunc)(lv_obj_t *obj);
    static const RemoveStyleFunc remove_style_funcs[] = {
        remove_style_button,
        remove_style_button_primary,
        remove_style_button_secondary,
        remove_style_button_danger,
    };
    remove_style_funcs[styleIndex](obj);
}