#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void action_ossm_control_stop(lv_event_t * e);
extern void action_welcome_scan(lv_event_t * e);
extern void action_scan_cancel(lv_event_t * e);
extern void action_scan_connect(lv_event_t * e);
extern void action_ossm_patterns_cancel(lv_event_t * e);
extern void action_ossm_patterns_select(lv_event_t * e);
extern void action_ossm_control_patterns(lv_event_t * e);
extern void action_welcome_settings(lv_event_t * e);
extern void action_ossm_control_settings(lv_event_t * e);
extern void action_settings_back(lv_event_t * e);
extern void action_settings_stop(lv_event_t * e);
extern void action_settings_select(lv_event_t * e);
extern void action_connect_cancel(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/