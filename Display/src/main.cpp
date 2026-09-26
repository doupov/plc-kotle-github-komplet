// main.cpp
#include <Arduino.h>
#include <lvgl.h>
#include <M5Dial.h>
#include <EEPROM.h>

#include "ui.h"
#include "screens.h"

// ----------------- Modbus RTU (RS-485) -----------------
#include <ModbusRTU.h>

// ====== RS-485 větev (Port B: OUT=G2 -> TX, IN=G1 -> RX) ======
static constexpr int MODBUS_RX_PIN = 1;   // G1 (IN)
static constexpr int MODBUS_TX_PIN = 2;   // G2 (OUT)
static constexpr int DE_RE_PIN     = -1;  // -1 = auto směr
static constexpr uint32_t MODBUS_BAUD = 9600;
static constexpr uint8_t  MODBUS_ID   = 1;   // slave address

ModbusRTU mb;

// --------- Globální stav ----------
int  g_kotel_temp   = 90;   // kotel aktuální teplota
int  g_tank_top     = 70;   // nádrž - horní sonda
int  g_tank_mid     = 40;   // nádrž - střed
int  g_tank_down    = 30;   // nádrž - spodní
int  g_topeni_act   = 55;   // topení aktuální
int  g_topeni_set   = 60;   // topení žádaná (EEPROM)
int  g_boiler_act   = 48;   // boiler aktuální
int  g_boiler_set   = 55;   // boiler žádaná (EEPROM)
int  g_kotel_temp2   = 10;   // kotel aktuální teplota

bool g_kotel_pump_on  = true;
bool g_topeni_pump_on = false;
bool g_boiler_pump_on = false;

// --- GLOBÁLNÍ LIMITY EDITACE (požadavek) ---
int g_topeni_set_min  = 35;
int g_topeni_set_max  = 70;
int g_boiler_set_min  = 50;
int g_boiler_set_max  = 65;

// --- GLOBÁLNÍ PRAH PŘEHŘÁTÍ (požadavek) ---
int  g_overheat_threshold = 85;
static constexpr int OVERHEAT_EXIT_TEMP = 82; // Match PLC alarm hysteresis.
bool g_overheat_active    = false;  // vnitřní latch pro "rising-edge" detekci

// ====== Modbus mapování registrů ======
enum : uint16_t {
  HR_KOTEL_TEMP = 0,
  HR_TANK_TOP   = 1,
  HR_TANK_MID   = 2,
  HR_TANK_DOWN  = 3,
  HR_TOPENI_ACT = 4,
  HR_TOPENI_SET = 5,
  HR_BOILER_ACT = 6,
  HR_BOILER_SET = 7,
  HR_KOTEL_TEMP2 = 8,
};
enum : uint16_t {
  CO_KOTEL_PUMP  = 0,
  CO_TOPENI_PUMP = 1,
  CO_BOILER_PUMP = 2,
};

// --- externy z tvých generovaných souborů ---
extern objects_t objects;
extern void create_screens();
extern void tick_screen(int screen_index);

// --- parametry displeje M5Dial ---
static constexpr uint16_t SCR_W = 240;
static constexpr uint16_t SCR_H = 240;

// --- LVGL display handle ---
static lv_display_t* disp = nullptr;

// --- správa stránek ---
static constexpr int kScreenCount = 4;
static int current_screen = 0;
// mapování indexů: 0=kotel, 1=nadrz (TANK), 2=topeni, 3=boiler

// --- enkodér ---
static bool encoder_ready = false;
static int32_t last_enc = 0;
static uint32_t last_switch_ms = 0;
static constexpr uint32_t switch_deadtime_ms = 120; // ms

// --- Režim editace (tlačítko jen pro edit) ---
enum EditTarget : uint8_t {
  EDIT_NONE = 0,
  EDIT_TOPENI_SET,
  EDIT_BOILER_SET
};
static EditTarget edit_target = EDIT_NONE;

// --- EEPROM layout (jednoduchý) ---
static constexpr uint16_t EE_MAGIC        = 0xBEEF;
static constexpr int      EE_SIZE         = 8;     // 2+2+2=6, zarovnáno
static constexpr int      EE_ADDR_TOP_SET = 0;     // uint16_t
static constexpr int      EE_ADDR_BLR_SET = 2;     // uint16_t
static constexpr int      EE_ADDR_MAGIC   = 4;     // uint16_t

// --- volitelně: LVGL log do sériovky (zapne se přes -D LV_USE_LOG=1) ---
#if LV_USE_LOG
static void lvgl_log_cb(const char* buf) { Serial.print(buf); }
#endif

// --------- EEPROM pomocné ----------
static bool eeprom_ready = false;
static int saved_topeni_set = -1;
static int saved_boiler_set = -1;
static void eeprom_begin() {
  eeprom_ready = EEPROM.begin(EE_SIZE);
  if (!eeprom_ready) Serial.println("EEPROM init failed; using RAM defaults");
}
static uint16_t ee_read_u16(int addr) {
  uint16_t hi = EEPROM.read(addr);
  uint16_t lo = EEPROM.read(addr + 1);
  return (uint16_t)((hi << 8) | lo);
}
static void ee_write_u16(int addr, uint16_t v) {
  EEPROM.write(addr,     (uint8_t)(v >> 8));
  EEPROM.write(addr + 1, (uint8_t)(v & 0xFF));
}
static bool ee_commit() {
  if (!EEPROM.commit()) { Serial.println("EEPROM commit failed"); return false; }
  saved_topeni_set = g_topeni_set;
  saved_boiler_set = g_boiler_set;
  return true;
}

static inline int clamp_to_range(int v, int vmin, int vmax) {
  if (v < vmin) return vmin;
  if (v > vmax) return vmax;
  return v;
}

static void load_setpoints_from_eeprom() {
  if (!eeprom_ready) return;
  uint16_t magic = ee_read_u16(EE_ADDR_MAGIC);
  if (magic == EE_MAGIC) {
    uint16_t t = ee_read_u16(EE_ADDR_TOP_SET);
    uint16_t b = ee_read_u16(EE_ADDR_BLR_SET);
    // validace proti GLOBÁLNÍM limitům
    if (t >= g_topeni_set_min && t <= g_topeni_set_max) g_topeni_set = (int)t;
    if (b >= g_boiler_set_min && b <= g_boiler_set_max) g_boiler_set = (int)b;
    if (t == g_topeni_set && b == g_boiler_set) {
      saved_topeni_set = g_topeni_set;
      saved_boiler_set = g_boiler_set;
    } // Invalid data: defaults in RAM, repair on next genuine edit exit.
  } else {
    // poprvé: zapiš aktuální defaulty a magic
    ee_write_u16(EE_ADDR_TOP_SET, (uint16_t)g_topeni_set);
    ee_write_u16(EE_ADDR_BLR_SET, (uint16_t)g_boiler_set);
    ee_write_u16(EE_ADDR_MAGIC,   EE_MAGIC);
    ee_commit();
  }
}

static void save_setpoints_to_eeprom() {
  if (!eeprom_ready || (saved_topeni_set == g_topeni_set && saved_boiler_set == g_boiler_set)) return;
  ee_write_u16(EE_ADDR_TOP_SET, (uint16_t)g_topeni_set);
  ee_write_u16(EE_ADDR_BLR_SET, (uint16_t)g_boiler_set);
  ee_write_u16(EE_ADDR_MAGIC,   EE_MAGIC);
  ee_commit();
}

static void finish_edit() {
  if (edit_target == EDIT_NONE) return;
  edit_target = EDIT_NONE;
  g_topeni_set = clamp_to_range(g_topeni_set, g_topeni_set_min, g_topeni_set_max);
  g_boiler_set = clamp_to_range(g_boiler_set, g_boiler_set_min, g_boiler_set_max);
  save_setpoints_to_eeprom();
  mb.Hreg(HR_TOPENI_SET, g_topeni_set);
  mb.Hreg(HR_BOILER_SET, g_boiler_set);
}

// --- pomocné funkce ---
static inline bool is_valid(lv_obj_t* o) { return o && lv_obj_is_valid(o); }

static void ensure_default_screens() {
  if (!is_valid(objects.kotel))  objects.kotel  = lv_obj_create(NULL);
  if (!is_valid(objects.nadrz))  objects.nadrz  = lv_obj_create(NULL);
  if (!is_valid(objects.topeni)) objects.topeni = lv_obj_create(NULL);
  if (!is_valid(objects.boiler)) objects.boiler = lv_obj_create(NULL);

  auto style_bg = [](lv_obj_t* scr) {
    if (!is_valid(scr)) return;
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101010), 0);
  };
  style_bg(objects.kotel);
  style_bg(objects.nadrz);
  style_bg(objects.topeni);
  style_bg(objects.boiler);
}

static lv_obj_t* get_screen_by_index(int idx) {
  switch (idx) {
    case 0: return objects.kotel;
    case 1: return objects.nadrz;  // TANK
    case 2: return objects.topeni;
    case 3: return objects.boiler;
    default: return nullptr;
  }
}

// --- LVGL flush s bezpečným RGB565 byte-swapem ---
static void my_disp_flush(lv_display_t * d, const lv_area_t * a, uint8_t * px_map) {
  const int32_t w = (a->x2 - a->x1 + 1);
  const int32_t h = (a->y2 - a->y1 + 1);

  lv_draw_sw_rgb565_swap(reinterpret_cast<lv_color_t*>(px_map), static_cast<uint32_t>(w * h));

  M5.Display.startWrite();
  M5.Display.setAddrWindow(a->x1, a->y1, w, h);
  M5.Display.pushPixels(reinterpret_cast<uint16_t const*>(px_map), w * h);
  M5.Display.endWrite();

  lv_display_flush_ready(d);
}

static void show_placeholder(const char* msg) {
  lv_obj_clean(lv_screen_active());
  lv_obj_t* l = lv_label_create(lv_screen_active());
  lv_label_set_text(l, msg);
  lv_obj_center(l);
}

// Bez animací. Pokud není cíl validní, zobrazí placeholder.
static void load_screen(int idx) {
  idx = (idx % kScreenCount + kScreenCount) % kScreenCount;
  current_screen = idx;

  lv_obj_t* scr = get_screen_by_index(current_screen);
  if (!is_valid(scr)) {
    show_placeholder("Screen not ready");
    return;
  }
  lv_screen_load(scr);
}

// --------- Pomocné funkce pro LED a ARC ----------
static inline int clamp01_100(int v) {
  if (v < 0)   return 0;
  if (v > 100) return 100;
  return v;
}

static void set_led_by_bool(lv_obj_t* led, bool on) {
  if (!is_valid(led)) return;
  lv_led_set_color(led, on ? lv_palette_main(LV_PALETTE_GREEN)
                           : lv_palette_main(LV_PALETTE_RED));
  lv_led_set_brightness(led, 255);
}

static void set_arc_value_and_style(lv_obj_t* arc, int v) {
  if (!is_valid(arc)) return;

  v = clamp01_100(v);
  lv_arc_set_range(arc, 0, 100);
  lv_arc_set_value(arc, v);

  lv_obj_set_style_arc_color(arc, lv_color_hex(0x333333), LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, 10, LV_PART_MAIN);

  lv_color_t col;
  if (v >= 80) {
    col = lv_palette_main(LV_PALETTE_RED);
  } else if (v >= 50) {
    col = lv_palette_main(LV_PALETTE_ORANGE);
  } else {
    col = lv_palette_main(LV_PALETTE_BLUE);
  }
  lv_obj_set_style_arc_color(arc, col, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(arc, 14, LV_PART_INDICATOR);
}

// --------- Aplikace override hodnot do UI ----------
static void apply_ui_overrides() {
  // --- KOTEL ---
  if (is_valid(objects.kotel_temp)) lv_label_set_text_fmt(objects.kotel_temp, "%d", g_kotel_temp2);
  if (is_valid(objects.kotel_arc))  set_arc_value_and_style(objects.kotel_arc, g_kotel_temp2);
  if (is_valid(objects.kotel_pump)) set_led_by_bool(objects.kotel_pump, g_kotel_pump_on);

  // --- NÁDRŽ ---
  if (is_valid(objects.tank_top))  lv_label_set_text_fmt(objects.tank_top,  "%d", g_tank_top);
  if (is_valid(objects.tank_mid))  lv_label_set_text_fmt(objects.tank_mid,  "%d", g_tank_mid);
  if (is_valid(objects.tank_down)) lv_label_set_text_fmt(objects.tank_down, "%d", g_tank_down);
  if (is_valid(objects.tank_arc))  set_arc_value_and_style(objects.tank_arc, g_tank_top);

  // Červený panel (overheat) při > g_overheat_threshold
  if (is_valid(objects.obj0)) {
    if (g_overheat_active) lv_obj_clear_flag(objects.obj0, LV_OBJ_FLAG_HIDDEN);
    else                                   lv_obj_add_flag(objects.obj0,   LV_OBJ_FLAG_HIDDEN);
  }

  // --- TOPENÍ ---
  if (is_valid(objects.topeni_act)) lv_label_set_text_fmt(objects.topeni_act, "%d", g_topeni_act);
  if (is_valid(objects.topeni_set)) {
    lv_label_set_text_fmt(objects.topeni_set, "%d", g_topeni_set);
    lv_color_t c = (edit_target == EDIT_TOPENI_SET) ? lv_palette_main(LV_PALETTE_RED) : lv_color_white();
    lv_obj_set_style_text_color(objects.topeni_set, c, LV_PART_MAIN | LV_STATE_DEFAULT);
  }
  if (is_valid(objects.topeni_arc))  set_arc_value_and_style(objects.topeni_arc, g_topeni_act);
  if (is_valid(objects.topeni_pump)) set_led_by_bool(objects.topeni_pump, g_topeni_pump_on);

  // --- BOILER ---
  if (is_valid(objects.boiler_act)) lv_label_set_text_fmt(objects.boiler_act, "%d", g_boiler_act);
  if (is_valid(objects.boiler_set)) {
    lv_label_set_text_fmt(objects.boiler_set, "%d", g_boiler_set);
    lv_color_t c = (edit_target == EDIT_BOILER_SET) ? lv_palette_main(LV_PALETTE_RED) : lv_color_white();
    lv_obj_set_style_text_color(objects.boiler_set, c, LV_PART_MAIN | LV_STATE_DEFAULT);
  }
  if (is_valid(objects.boiler_arc)) set_arc_value_and_style(objects.boiler_arc, g_boiler_act);

  // Pozn.: v objektové struktuře je "boiler_pumo"
  if (is_valid(objects.boiler_pumo)) set_led_by_bool(objects.boiler_pumo, g_boiler_pump_on);

  // Boiler EL text skryt
  if (is_valid(objects.boiler_el)) {
    lv_obj_add_flag(objects.boiler_el, LV_OBJ_FLAG_HIDDEN);
  }
}

// ====== Modbus: inicializace a synchronizace ======
static void modbus_init() {
  Serial1.begin(MODBUS_BAUD, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);
  if (DE_RE_PIN >= 0) mb.begin(&Serial1, DE_RE_PIN);
  else                mb.begin(&Serial1);

  mb.setBaudrate(MODBUS_BAUD);
  mb.slave(MODBUS_ID);

  // Holding registry
  mb.addHreg(HR_KOTEL_TEMP, g_kotel_temp);
  mb.addHreg(HR_TANK_TOP,   g_tank_top);
  mb.addHreg(HR_TANK_MID,   g_tank_mid);
  mb.addHreg(HR_TANK_DOWN,  g_tank_down);
  mb.addHreg(HR_TOPENI_ACT, g_topeni_act);
  mb.addHreg(HR_TOPENI_SET, g_topeni_set);
  mb.addHreg(HR_BOILER_ACT, g_boiler_act);
  mb.addHreg(HR_BOILER_SET, g_boiler_set);
  mb.addHreg(HR_KOTEL_TEMP2, g_kotel_temp2);

  // Coils
  mb.addCoil(CO_KOTEL_PUMP,  g_kotel_pump_on);
  mb.addCoil(CO_TOPENI_PUMP, g_topeni_pump_on);
  mb.addCoil(CO_BOILER_PUMP, g_boiler_pump_on);
  
}

// Stáhne změny z masteru (vč. tank_top) a vyhodnotí přehřátí
static inline void modbus_pull_into_globals_and_check_overheat() {
  g_kotel_pump_on = mb.Coil(CO_KOTEL_PUMP);
  g_topeni_pump_on = mb.Coil(CO_TOPENI_PUMP);
  g_boiler_pump_on = mb.Coil(CO_BOILER_PUMP);

  // Holding regs -> int proměnné (vyjma setpointů, které v editaci držíme lokálně)
  g_kotel_temp  = (int16_t)mb.Hreg(HR_KOTEL_TEMP);
  int new_tank_top = (int16_t)mb.Hreg(HR_TANK_TOP);
  g_tank_mid    = (int16_t)mb.Hreg(HR_TANK_MID);
  g_tank_down   = (int16_t)mb.Hreg(HR_TANK_DOWN);
  g_topeni_act  = (int16_t)mb.Hreg(HR_TOPENI_ACT);
  g_boiler_act  = (int16_t)mb.Hreg(HR_BOILER_ACT);
  g_kotel_temp2  = (int16_t)mb.Hreg(HR_KOTEL_TEMP2);

  // Detekce přehřátí (rising-edge): přepni na TANK screen při překročení prahu
  bool now_hot = g_overheat_active ? new_tank_top > OVERHEAT_EXIT_TEMP
                                   : new_tank_top > g_overheat_threshold;
  if (now_hot && !g_overheat_active) {
    // došlo ke KŘÍŽENÍ PRAHU nahoru -> přepnout na Nádrž (index 1)
    finish_edit(); // Avoid invisible editing after the alarm changes screens.
    load_screen(1);
  }
  g_overheat_active = now_hot;   // latch
  g_tank_top = new_tank_top;     // po vyhodnocení ulož
}

static inline void modbus_push_setpoints() {
  mb.Hreg(HR_TOPENI_SET, g_topeni_set);
  mb.Hreg(HR_BOILER_SET, g_boiler_set);
}

// ====== Režim editace: pomocné ======
static inline bool is_screen_topeni() { return current_screen == 2; }
static inline bool is_screen_boiler() { return current_screen == 3; }

static void toggle_edit_current_screen() {
  if (edit_target != EDIT_NONE) {
    finish_edit();
  } else if (is_screen_topeni()) {
    edit_target = EDIT_TOPENI_SET;
  } else if (is_screen_boiler()) {
    edit_target = EDIT_BOILER_SET;
  }
}

void setup() {
  // --- start M5Dial ---
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5Dial.begin(cfg);

  M5.Display.setBrightness(140);
  M5.Display.setRotation(0);
  M5.Display.setSwapBytes(false);

  // --- EEPROM ---
  eeprom_begin();
  load_setpoints_from_eeprom();

  // --- LVGL init ---
  lv_init();
#if LV_USE_LOG
  lv_log_register_print_cb(lvgl_log_cb);
#endif

  // --- LVGL display objekt ---
  disp = lv_display_create(SCR_W, SCR_H);
  lv_display_set_default(disp);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_render_mode(disp, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, my_disp_flush);

  lv_draw_buf_t* dbuf = lv_draw_buf_create(SCR_W, 80, LV_COLOR_FORMAT_RGB565, 0);
  lv_display_set_draw_buffers(disp, dbuf, nullptr);

  lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x101010), 0);

  // --- vytvoření tvých obrazovek ---
  create_screens();
  ensure_default_screens();
  load_screen(0);

  // Enkodér
  M5Dial.Encoder.begin();

  // --- Modbus RS-485 init ---
  modbus_init();
  // promítnout setpointy do registrů
  modbus_push_setpoints();
}

void loop() {
  M5Dial.update();

  // === Modbus: obsluha + pull + kontrola přehřátí ===
  mb.task();
  modbus_pull_into_globals_and_check_overheat();

  apply_ui_overrides();    // Update hodnot na display

  // Přesný LVGL tick podle millis()
  static uint32_t last = 0;
  uint32_t now = millis();
  lv_tick_inc(now - last);
  last = now;

  // Obsluha LVGL časovačů
  lv_timer_handler();

  // Lehký yield
  delay(5);

  // --- Enkodér: povolit po prvním update, teprve pak první read ---
  if (!encoder_ready) {
    last_enc = M5Dial.Encoder.read();
    encoder_ready = true;
  }

  if (encoder_ready) {
    int32_t enc_now = M5Dial.Encoder.read();
    uint32_t diff = uint32_t(enc_now) - uint32_t(last_enc);
    const int encoder_direction = diff < 0x80000000UL ? +1 : -1;

    if (diff != 0) {
      uint32_t t = millis();

      if (edit_target == EDIT_NONE) {
        // --- Navigace mezi obrazovkami (jako dřív) ---
        if (t - last_switch_ms > switch_deadtime_ms) {
          int dir = encoder_direction;
          load_screen(current_screen + dir);
          last_switch_ms = t;
        }
      } else {
        // --- Jsme v režimu editace: enkodér upravuje setpoint ---
        int step = encoder_direction;
        if (edit_target == EDIT_TOPENI_SET) {
          g_topeni_set += step;
          g_topeni_set = clamp_to_range(g_topeni_set, g_topeni_set_min, g_topeni_set_max);
          mb.Hreg(HR_TOPENI_SET, g_topeni_set); // okamžitě do Modbus
        } else if (edit_target == EDIT_BOILER_SET) {
          g_boiler_set += step;
          g_boiler_set = clamp_to_range(g_boiler_set, g_boiler_set_min, g_boiler_set_max);
          mb.Hreg(HR_BOILER_SET, g_boiler_set);
        }
      }
      last_enc = enc_now;
    }
  }

  // --- Tlačítko: jen pro editaci ---
  if (M5Dial.BtnA.wasPressed()) {
    toggle_edit_current_screen();
  }

  // --- uživatelský "tick" pro aktuální stránku ---
  if (is_valid(get_screen_by_index(current_screen))) {
    tick_screen(current_screen);
  }
}
