#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *kotel;
    lv_obj_t *nadrz;
    lv_obj_t *topeni;
    lv_obj_t *boiler;
    lv_obj_t *kotel_temp;
    lv_obj_t *kotel_arc;
    lv_obj_t *kotel_pump;
    lv_obj_t *obj0;
    lv_obj_t *tank_top;
    lv_obj_t *tank_arc;
    lv_obj_t *tank_mid;
    lv_obj_t *tank_down;
    lv_obj_t *topeni_act;
    lv_obj_t *topeni_arc;
    lv_obj_t *topeni_set;
    lv_obj_t *topeni_pump;
    lv_obj_t *boiler_act;
    lv_obj_t *boiler_arc;
    lv_obj_t *boiler_set;
    lv_obj_t *boiler_pumo;
    lv_obj_t *boiler_el;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_KOTEL = 1,
    SCREEN_ID_NADRZ = 2,
    SCREEN_ID_TOPENI = 3,
    SCREEN_ID_BOILER = 4,
};

void create_screen_kotel();
void tick_screen_kotel();

void create_screen_nadrz();
void tick_screen_nadrz();

void create_screen_topeni();
void tick_screen_topeni();

void create_screen_boiler();
void tick_screen_boiler();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/