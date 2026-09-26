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
lv_obj_t *tick_value_change_obj;
uint32_t active_theme_index = 0;

void create_screen_kotel() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.kotel = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 240, 240);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 78, 24);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Kotel");
        }
        {
            // kotel_temp
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.kotel_temp = obj;
            lv_obj_set_pos(obj, 90, 120);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "99");
        }
        {
            // kotel_arc
            lv_obj_t *obj = lv_arc_create(parent_obj);
            objects.kotel_arc = obj;
            lv_obj_set_pos(obj, 45, 71);
            lv_obj_set_size(obj, 150, 150);
            lv_arc_set_range(obj, 50, 100);
            lv_arc_set_value(obj, 75);
        }
        {
            // kotel_pump
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.kotel_pump = obj;
            lv_obj_set_pos(obj, 179, 55);
            lv_obj_set_size(obj, 32, 32);
            lv_led_set_color(obj, lv_color_hex(0xff0000ff));
            lv_led_set_brightness(obj, 255);
        }
    }
    
    tick_screen_kotel();
}

void tick_screen_kotel() {
}

void create_screen_nadrz() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.nadrz = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 240, 240);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.obj0 = obj;
            lv_obj_set_pos(obj, -29, 20);
            lv_obj_set_size(obj, 300, 200);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xffff0000), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 72, 24);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Nadrz");
        }
        {
            // tank_top
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.tank_top = obj;
            lv_obj_set_pos(obj, 90, 106);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "99");
        }
        {
            // tank_arc
            lv_obj_t *obj = lv_arc_create(parent_obj);
            objects.tank_arc = obj;
            lv_obj_set_pos(obj, 45, 71);
            lv_obj_set_size(obj, 150, 150);
            lv_arc_set_value(obj, 25);
        }
        {
            // tank_mid
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.tank_mid = obj;
            lv_obj_set_pos(obj, 100, 158);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "99");
        }
        {
            // tank_down
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.tank_down = obj;
            lv_obj_set_pos(obj, 100, 193);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "99");
        }
    }
    
    tick_screen_nadrz();
}

void tick_screen_nadrz() {
}

void create_screen_topeni() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.topeni = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 240, 240);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 72, 24);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Topeni");
        }
        {
            // topeni_act
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.topeni_act = obj;
            lv_obj_set_pos(obj, 90, 120);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "99");
        }
        {
            // topeni_arc
            lv_obj_t *obj = lv_arc_create(parent_obj);
            objects.topeni_arc = obj;
            lv_obj_set_pos(obj, 45, 71);
            lv_obj_set_size(obj, 150, 150);
            lv_arc_set_value(obj, 25);
        }
        {
            // topeni_set
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.topeni_set = obj;
            lv_obj_set_pos(obj, 100, 195);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "99");
        }
        {
            // topeni_pump
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.topeni_pump = obj;
            lv_obj_set_pos(obj, 179, 55);
            lv_obj_set_size(obj, 32, 32);
            lv_led_set_color(obj, lv_color_hex(0xff0000ff));
            lv_led_set_brightness(obj, 255);
        }
    }
    
    tick_screen_topeni();
}

void tick_screen_topeni() {
}

void create_screen_boiler() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.boiler = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 240, 240);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 72, 24);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Boiler");
        }
        {
            // boiler_act
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.boiler_act = obj;
            lv_obj_set_pos(obj, 90, 120);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "99");
        }
        {
            // boiler_arc
            lv_obj_t *obj = lv_arc_create(parent_obj);
            objects.boiler_arc = obj;
            lv_obj_set_pos(obj, 45, 71);
            lv_obj_set_size(obj, 150, 150);
            lv_arc_set_value(obj, 25);
        }
        {
            // boiler_set
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.boiler_set = obj;
            lv_obj_set_pos(obj, 100, 193);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "99");
        }
        {
            // boiler_pumo
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.boiler_pumo = obj;
            lv_obj_set_pos(obj, 179, 55);
            lv_obj_set_size(obj, 32, 32);
            lv_led_set_color(obj, lv_color_hex(0xff0000ff));
            lv_led_set_brightness(obj, 255);
        }
        {
            // boiler_el
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.boiler_el = obj;
            lv_obj_set_pos(obj, 87, 158);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffff0000), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "+EL");
        }
    }
    
    tick_screen_boiler();
}

void tick_screen_boiler() {
}



typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_kotel,
    tick_screen_nadrz,
    tick_screen_topeni,
    tick_screen_boiler,
};
void tick_screen(int screen_index) {
    tick_screen_funcs[screen_index]();
}
void tick_screen_by_id(enum ScreensEnum screenId) {
    tick_screen_funcs[screenId - 1]();
}

void create_screens() {
    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    
    create_screen_kotel();
    create_screen_nadrz();
    create_screen_topeni();
    create_screen_boiler();
}
