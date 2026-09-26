// ===================== StamPLC Master: NT48A08 + M5Dial (Dial HR = INT °C) =====================

\#include \<Arduino.h>

\#include \<M5Unified.h>

\#include \<M5StamPLC.h>



// ---------- RS485 / Modbus ----------

\#include \<ArduinoModbus.h>

\#include \<ArduinoRS485.h>

\#include \<cmath>

\#include "control_safety.h"



static safety::Freshness nt48_freshness;

static safety::Valve valve;

static bool overheat_active = false;

static bool modbus_ready = false;

static uint32_t last_modbus_init_attempt_ms = 0;

static constexpr uint32_t MODBUS_INIT_RETRY_MS = 5000;

enum class SensorMode : uint8_t { NORMAL, SENSOR_FAIL, NT48_LOST };

static SensorMode sensor_mode = SensorMode::NORMAL;

static safety::DisplayBackoff display_link;

static uint8_t pending_temp = 7; // No old/stale temperature batch before a valid read.

static uint32_t lastPollNT48 = 0;

static bool nt48_poll_started = false;

static bool nt48_ever_received = false;

static uint32_t nt48_started_ms = 0;

static constexpr uint32_t POLL_NT48_MS = 3000;



// StamPLC zapojení RS485 transceiveru (DE/RE/TXEN)

RS485Class RS485(Serial2, 39, 0, 46, -1);

static const uint32_t MODBUS_BAUD = 9600;

static const uint8_t  DIAL_ID     = 1;   // M5Stack Dial (slave)

static const uint8_t  NT48_ID     = 2;   // NT48A08 (slave)



// ---------- PORT A – G1 = GPIO1 (čerpadlo boileru) ----------

\#ifndef PORTA_G1_GPIO

&#x20; \#define PORTA_G1_GPIO 1   // Stamp-S3: Port A G1 = GPIO 1 (SDA)

\#endif



// ====== Modbus mapování registrů (M5Dial) ======

enum : uint16_t {

&#x20; HR_KOTEL_TEMP = 0,

&#x20; HR_TANK_TOP   = 1,

&#x20; HR_TANK_MID   = 2,

&#x20; HR_TANK_DOWN  = 3,

&#x20; HR_TOPENI_ACT = 4,

&#x20; HR_TOPENI_SET = 5,

&#x20; HR_BOILER_ACT = 6,

&#x20; HR_BOILER_SET = 7,

&#x20; HR_KOTEL_TEMP2 = 8,

};

enum : uint16_t {

&#x20; CO_KOTEL_PUMP  = 0,  // signalizace → DIAL coil 0

&#x20; CO_TOPENI_PUMP = 1,  // signalizace → DIAL coil 1

&#x20; CO_BOILER_PUMP = 2,  // signalizace → DIAL coil 2

};



// Popisky pro UI

\#define RELAY1_CONTACT "NC"

\#define RELAY2_CONTACT "NC"

\#define RELAY3_CONTACT "NO (PORTA G1)"



// ---------- Počáteční stavy výstupů ----------

static bool relay_state[4] = { false, false, false, false }; // R1..R4



// Tlačítka A/B/C přepínají ruční režim jednotlivých čerpadel.

// Po startu jsou všechna čerpadla v automatickém režimu.

static bool pump_force_on[3] = { false, false, false };



// ---------- Globální teploty (°C) z NT48 ----------

static float t_nt_tank_top   = NAN; // CH01

static float t_nt_tank_mid   = NAN; // CH02

static float t_nt_tank_down  = NAN; // CH03

static float t_nt_topeni     = NAN; // CH04

static float t_nt_boiler     = NAN; // CH05

static float t_nt_kotel      = NAN; // CH06



// ---------- Hodnoty z Dialu (HR = INT °C) ----------

static float d_kotel_temp    = NAN;

static float d_tank_top      = NAN;

static float d_tank_mid      = NAN;

static float d_tank_down     = NAN;

static float d_topeni_act    = NAN;

// BEHAVIOR CHANGE: bounded Display defaults until the first valid panel read.

static float d_topeni_set    = 60;  // SET (Dial -> StamPLC)

static float d_boiler_act    = NAN;

static float d_boiler_set    = 55;  // SET (Dial -> StamPLC)



// ---------- Pomocné ----------

static inline bool isPlausibleTemp(float c) {

&#x20; return isfinite(c) && (c > -100.0f) && (c < 300.0f);

}



// Konstanty řízení

static float kotel_stop  = 50;

static float kotel_start = 75;

static float kotel_delta = 2;

static float topeni_hyst = 2;

static float kotel_abs_on  = 75.0f;  // >= → ON

static float kotel_abs_off = 55.0f;  // <= → OFF



// ---------- Výstupy ----------

static inline void writeRelay12(uint8_t idx, bool on) {&#x20;

&#x20; // idx: 0..3 (R1..R4 na PLC)

&#x20; M5StamPLC.writePlcRelay(idx, on);

&#x20; relay_state[idx] = on;

}

static inline void gpioR3_init() { pinMode(PORTA_G1_GPIO, OUTPUT); digitalWrite(PORTA_G1_GPIO, LOW); }

static inline void gpioR3_write(bool on) { digitalWrite(PORTA_G1_GPIO, on ? HIGH : LOW); }



// Sole electrical direction mapping; preserves old plus=true => warmer wiring.

// VERIFY physically: relay HIGH must move toward OPEN_HOT on the installed servo.

struct ValveOutputs {

&#x20; void run(bool on) { writeRelay12(2, on); }

&#x20; void direction(safety::Direction dir) {

&#x20;   writeRelay12(3, dir == safety::Direction::OPEN_HOT);

&#x20; }

} valve_outputs;



// ---------- UI ----------

static void drawUI() {

&#x20; auto& d = M5StamPLC.Display;



&#x20; d.startWrite();

&#x20; d.fillScreen(TFT_BLACK);

&#x20; d.setTextColor(TFT_WHITE, TFT_BLACK);

&#x20; d.setTextDatum(textdatum_t::top_left);



&#x20; // Pouze režim tří čerpadel a velká teplota kotle.

&#x20; d.setTextSize(2);

&#x20; d.setCursor(6, 3);

&#x20; d.printf("KOTEL");

&#x20; d.setCursor(138, 3);

&#x20; d.printf("%s", pump_force_on[0] ? "FORCE ON" : "AUT");

&#x20; d.setCursor(6, 27);

&#x20; d.printf("TOPENI");

&#x20; d.setCursor(138, 27);

&#x20; d.printf("%s", pump_force_on[1] ? "FORCE ON" : "AUT");

&#x20; d.setCursor(6, 51);

&#x20; d.printf("BOILER");

&#x20; d.setCursor(138, 51);

&#x20; d.printf("%s", pump_force_on[2] ? "FORCE ON" : "AUT");



&#x20; d.setTextSize(4);

&#x20; d.setCursor(42, 91);

&#x20; if (isPlausibleTemp(t_nt_kotel)) d.printf("%5.1f C", t_nt_kotel);

&#x20; else                             d.printf("  --.- C");

&#x20; d.endWrite();

}



static void serviceLocalUI() {

&#x20; if (valve.busy()) return;

&#x20; M5.update();

&#x20; bool changed = false;

&#x20; if (M5.BtnA.wasClicked()) {

&#x20;   pump_force_on[0] = !pump_force_on[0];

&#x20;   changed = true;

&#x20; }

&#x20; if (M5.BtnB.wasClicked()) {

&#x20;   pump_force_on[1] = !pump_force_on[1];

&#x20;   changed = true;

&#x20; }

&#x20; if (M5.BtnC.wasClicked()) {

&#x20;   pump_force_on[2] = !pump_force_on[2];

&#x20;   changed = true;

&#x20; }

&#x20; static uint32_t last_ui_ms = 0;

&#x20; if (changed || safety::expired(millis(), last_ui_ms, 1000)) {

&#x20;   last_ui_ms = millis();

&#x20;   drawUI();

&#x20; }

}



// ---------- ČTENÍ NT48A08: Holding 0..7, /10 ----------

static bool readNT48() {

&#x20; uint16_t raw[8];

&#x20; const auto result = safety::readRegisters(ModbusRTUClient, NT48_ID, HOLDING_REGISTERS, 0, raw);

&#x20; if (result != safety::FrameResult::OK) {

&#x20;   Serial.println(result == safety::FrameResult::FAIL ? "[NT48] FAIL/timeout/frame error" : "[NT48] PARTIAL");

&#x20;   return false; // Keep historical values; their freshness expires independently.

&#x20; }

&#x20; nt48_freshness.success(millis());

&#x20; nt48_ever_received = true;

&#x20; pending_temp = 0;



&#x20; auto f = []\(int16_t v)->float{

&#x20;   float x = v / 10.0f;

&#x20;   if (!isPlausibleTemp(x) || fabsf(x + 273.1f) < 0.05f) return NAN;

&#x20;   return x;

&#x20; };



&#x20; t_nt_tank_top  = f(static_cast\<int16_t>(raw[0]));

&#x20; t_nt_tank_mid  = f(static_cast\<int16_t>(raw[1]));

&#x20; t_nt_tank_down = f(static_cast\<int16_t>(raw[2]));

&#x20; t_nt_kotel    = f(static_cast\<int16_t>(raw[3]));

&#x20; t_nt_boiler    = f(static_cast\<int16_t>(raw[4]));

&#x20; t_nt_topeni     = f(static_cast\<int16_t>(raw[5]));



&#x20; Serial.println(F("--- NT48 (ID=2): temps (/10) ---"));

&#x20; Serial.printf("tank_top/mid/down: %.1f / %.1f / %.1f\n", t_nt_tank_top, t_nt_tank_mid, t_nt_tank_down);

&#x20; Serial.printf("topeni/boiler/kotel: %.1f / %.1f / %.1f\n", t_nt_topeni, t_nt_boiler, t_nt_kotel);

&#x20; return true;

}



// ---------- NT48 → Dial (HR = INT °C) ----------

static inline int16_t round_to_i16(float c) {

&#x20; long v = lroundf(c);                   // na celé °C

&#x20; if (v >  32767) v =  32767;

&#x20; if (v < -32768) v = -32768;

&#x20; return (int16_t)v;

}



static bool pushTempsToDial() {

&#x20; // pošleme naměřené teploty z NT48 do Dial HR jako INT °C (žádné ×10)

&#x20; struct { uint16_t addr; float val; const char\* name; } wr[] = {

&#x20;   { HR_KOTEL_TEMP,  t_nt_kotel,     "HR_KOTEL_TEMP"  },

&#x20;   { HR_TANK_TOP,    t_nt_tank_top,  "HR_TANK_TOP"    },

&#x20;   { HR_TANK_MID,    t_nt_tank_mid,  "HR_TANK_MID"    },

&#x20;   { HR_TANK_DOWN,   t_nt_tank_down, "HR_TANK_DOWN"   },

&#x20;   { HR_TOPENI_ACT,  t_nt_topeni,    "HR_TOPENI_ACT"  },

&#x20;   { HR_BOILER_ACT,  t_nt_boiler,    "HR_BOILER_ACT"  },

&#x20;   { HR_KOTEL_TEMP2, t_nt_kotel,     "HR_KOTEL_TEMP2" },

&#x20; };



&#x20; while (pending_temp < 7) {

&#x20;   const auto& w = wr[pending_temp++];

&#x20;   if (!isPlausibleTemp(w\.val)) continue;

&#x20;   const int16_t v = round_to_i16(w\.val);

&#x20;   if (!ModbusRTUClient.holdingRegisterWrite(DIAL_ID, w\.addr, (uint16_t)v)) {

&#x20;     Serial.printf("[DIAL] FAIL write %s @%u\n", w\.name, w\.addr);

&#x20;     display_link.failed(millis());

&#x20;     pending_temp = 7;

&#x20;   }

&#x20;   return true; // Only ONE synchronous transaction before returning to safety control.

&#x20; }

&#x20; return false;

}



// ---------- ČTENÍ DIAL: HR (INT °C) ----------

static inline float hrToC(uint16_t w) {

&#x20; // Dial ukládá HR jako INT °C → jen přecast na float pro tisk/UI

&#x20; return (float)((int16_t)w);

}



static bool readDial_Holding() {

&#x20; uint16_t hr[8];

&#x20; const auto result = safety::readRegisters(ModbusRTUClient, DIAL_ID, HOLDING_REGISTERS, HR_KOTEL_TEMP, hr);

&#x20; if (result != safety::FrameResult::OK) {

&#x20;   Serial.println(result == safety::FrameResult::FAIL ? "[DIAL] FAIL/timeout/frame error" : "[DIAL] PARTIAL");

&#x20;   display_link.completeRead(millis(), false);

&#x20;   return false;

&#x20; }



&#x20; display_link.completeRead(millis(), true);

&#x20; d_kotel_temp = hrToC(hr[0]);

&#x20; d_tank_top   = hrToC(hr[1]);

&#x20; d_tank_mid   = hrToC(hr[2]);

&#x20; d_tank_down  = hrToC(hr[3]);

&#x20; d_topeni_act = hrToC(hr[4]);

&#x20; if (!safety::acceptSetpoint(hrToC(hr[5]), 35, 70, d_topeni_set))

&#x20;   Serial.println("[DIAL] rejected heating setpoint");

&#x20; d_boiler_act = hrToC(hr[6]);

&#x20; if (!safety::acceptSetpoint(hrToC(hr[7]), 50, 65, d_boiler_set))

&#x20;   Serial.println("[DIAL] rejected boiler setpoint");



&#x20; Serial.println(F("--- DIAL (ID=1): HR INT °C ---"));

&#x20; Serial.printf("kotel: %.0f C | tank T/M/D: %.0f / %.0f / %.0f C\n",

&#x20;               d_kotel_temp, d_tank_top, d_tank_mid, d_tank_down);

&#x20; Serial.printf("topeni ACT/SET: %.0f / %.0f C | boiler ACT/SET: %.0f / %.0f C\n",

&#x20;               d_topeni_act, d_topeni_set, d_boiler_act, d_boiler_set);

&#x20; return true;

}



// ---------- ZÁPIS COILŮ NA DIAL (pouze signalizace!) ----------

static bool writeDial_Coil(uint16_t addr, bool on) {

&#x20; if (!ModbusRTUClient.coilWrite(DIAL_ID, addr, on ? 0xFF : 0x00)) {

&#x20;   Serial.printf("[DIAL] coilWrite addr %u FAIL\n", addr);

&#x20;   display_link.failed(millis());

&#x20;   return false;

&#x20; }

&#x20; return true;

}

static bool signalPumpsToDial(bool kotel_on, bool topeni_on, bool boiler_on) {

&#x20; if (valve.busy()) return false; // No blocking Modbus while a pulse/deadtime is active.

&#x20; static bool sent[3] = {};

&#x20; static bool known[3] = {};

&#x20; static bool attempted[3] = {};

&#x20; static uint32_t last_attempt[3] = {};

&#x20; static uint32_t last_ok[3] = {};

&#x20; const bool desired[3] = {kotel_on, topeni_on, boiler_on};

&#x20; const uint32_t now = millis();

&#x20; for (uint16_t i = 0; i < 3; ++i) {

&#x20;   const bool due = !known[i] || sent[i] != desired[i] || safety::expired(now, last_ok[i], 10000);

&#x20;   if (!due || (attempted[i] && !safety::expired(now, last_attempt[i], 1000))) continue;

&#x20;   attempted[i] = true;

&#x20;   last_attempt[i] = now;

&#x20;   if (writeDial_Coil(i, desired[i])) {

&#x20;     known[i] = true;

&#x20;     sent[i] = desired[i];

&#x20;     last_ok[i] = millis();

&#x20;   } else {

&#x20;     known[i] = false; // Retry failed writes even if desired state returns to old value.

&#x20;   }

&#x20;   return true; // At most one transaction per loop; retries limited to 1 s per coil.

&#x20; }

&#x20; return false;

}



// Display is last priority. All paths share backoff, including temperature/coils.

// Offline: read-only probe every 10 s; only a complete read restores ONLINE.

static bool serviceDisplay(bool kotel_on, bool topeni_on, bool boiler_on) {

&#x20; if (!modbus_ready || valve.busy() || (valve.emergency_latched &&

&#x20;     valve.state != safety::Valve::State::EMERGENCY_HOLD)) return false;

&#x20; const uint32_t now = millis();

&#x20; if (!nt48_poll_started || safety::expired(now, lastPollNT48, POLL_NT48_MS)) return false;

&#x20; if (display_link.due(now)) { readDial_Holding(); return true; }

&#x20; if (display_link.offline) return false;

&#x20; if (signalPumpsToDial(kotel_on, topeni_on, boiler_on)) return true;

&#x20; if (nt48_freshness.current(millis())) return pushTempsToDial();

&#x20; return false;

}



static bool tryInitModbus(uint32_t now) {

&#x20; last_modbus_init_attempt_ms = now;

&#x20; modbus_ready = ModbusRTUClient.begin(MODBUS_BAUD, SERIAL_8N1);

&#x20; if (modbus_ready) {

&#x20;   nt48_poll_started = false; // Acquisition, not begin(), determines sensor health.

&#x20;   Serial.println(F("[MODBUS] initialized"));

&#x20; } else {

&#x20;   Serial.println(F("[MODBUS] init failed; safety mode active"));

&#x20; }

&#x20; return modbus_ready;

}



// ===================== SETUP / LOOP =====================

void setup() {

&#x20; Serial.begin(115200);

&#x20; delay(150);



&#x20; // M5 + PLC

&#x20; auto cfg = M5.config();

&#x20; M5.begin(cfg);

&#x20; M5StamPLC.begin();

&#x20; // First output write after relay hardware is available; never rely on RAM cache.

&#x20; M5StamPLC.writePlcRelay(2, false);

&#x20; relay_state[2] = false;



&#x20; // Displej

&#x20; M5StamPLC.Display.setRotation(1);

&#x20; M5StamPLC.Display.setBrightness(200);



&#x20; // PORTA G1 (čerpadlo boiler)

&#x20; gpioR3_init();



&#x20; nt48_started_ms = millis();

&#x20; tryInitModbus(nt48_started_ms); // Failure must still enter loop and pump failsafe.

&#x20; // First loop polls NT48 and applies control before any Display transaction.

}



void loop() {

&#x20; uint32_t now = millis();



&#x20; // 1. Service motor deadline before any potentially blocking operation.

&#x20; valve.service(now, valve_outputs);



&#x20; // Initialization may block too: pause the SAME emergency episode before retry.

&#x20; if (!modbus_ready && safety::expired(now, last_modbus_init_attempt_ms, MODBUS_INIT_RETRY_MS)) {

&#x20;   if (valve.emergency_latched) valve.pauseEmergency(now, valve_outputs);

&#x20;   if (!valve.busy()) {

&#x20;     valve_outputs.run(false);

&#x20;     tryInitModbus(now);

&#x20;   }

&#x20; }

&#x20; now = millis();



&#x20; // 2. NT48 has priority. Emergency motion pauses for the synchronous read;

&#x20; // its single episode keeps the accumulated ON-time budget (no 1 s/15 s cycling).

&#x20; if (modbus_ready && (!nt48_poll_started || safety::expired(now, lastPollNT48, POLL_NT48_MS))) {

&#x20;   if (valve.emergency_latched) valve.pauseEmergency(now, valve_outputs);

&#x20;   if (!valve.busy()) {

&#x20;     valve_outputs.run(false);

&#x20;     readNT48();

&#x20;     lastPollNT48 = millis();

&#x20;     nt48_poll_started = true;

&#x20;   }

&#x20; }

&#x20; now = millis();



&#x20; // ====== AUTO ŘÍZENÍ – logika s hysterezí a failsafe ======



&#x20; // 1) Validace měření (failsafe ON pokud něco chybí)

&#x20; const bool fresh = modbus_ready && nt48_freshness.current(now);

&#x20; bool valid_kotel   = isPlausibleTemp(t_nt_kotel);

&#x20; bool valid_tankT   = isPlausibleTemp(t_nt_tank_top);

&#x20; bool valid_tankM   = isPlausibleTemp(t_nt_tank_mid);

&#x20; bool valid_topeni  = isPlausibleTemp(t_nt_topeni);

&#x20; bool valid_boiler  = isPlausibleTemp(t_nt_boiler);



&#x20; static bool kotel_on  = true;   // paměť pro hysterezi

&#x20; static bool topeni_on = true;

&#x20; static bool boiler_on = true;



&#x20; bool any_invalid = !fresh || !(valid_kotel && valid_tankT && valid_tankM && valid_topeni && valid_boiler);



&#x20; // 2) ALARM přehřátí (NT48 senzor!)

&#x20; const bool was_alarm = overheat_active;

&#x20; const bool tank_alarm_valid = fresh && valid_tankT;

&#x20; overheat_active = safety::updateAlarm(overheat_active, tank_alarm_valid, t_nt_tank_top);

&#x20; const bool alarm = overheat_active;

&#x20; const bool heat_dump_feedback_valid = fresh && valid_tankT && valid_topeni;

&#x20; if (alarm && !was_alarm && !valve.emergency_latched) valve.stop(now, valve_outputs);

&#x20; sensor_mode = !fresh ? SensorMode::NT48_LOST :

&#x20;               any_invalid ? SensorMode::SENSOR_FAIL : SensorMode::NORMAL;

&#x20; // At boot, allow the same 12 s acquisition window before blind emergency motion.

&#x20; const bool long_communication_loss = !fresh && (nt48_ever_received ||

&#x20;   safety::expired(now, nt48_started_ms, safety::NT48_STALE_TIMEOUT_MS));

&#x20; const bool complete_sensor_loss = long_communication_loss || (fresh &&

&#x20;   !(valid_kotel || valid_tankT || valid_tankM || valid_topeni || valid_boiler));



&#x20; // A. Confirmed overheat with usable supply feedback outranks unrelated faults.

&#x20; if (alarm && heat_dump_feedback_valid) {

&#x20;   kotel_on = topeni_on = boiler_on = true;

&#x20;   if (valve.emergency_latched) valve.recover(now, valve_outputs);

&#x20;   if (!valve.busy()) {

&#x20;     writeRelay12(0, !kotel_on);

&#x20;     writeRelay12(1, !topeni_on);

&#x20;   }

&#x20;   gpioR3_write(boiler_on);

&#x20;   const auto wanted = safety::demand(true, t_nt_topeni,

&#x20;                                     safety::ALARM_DUMP_SUPPLY_SETPOINT, topeni_hyst);

&#x20;   valve.update(millis(), wanted, valve_outputs);

&#x20;   if (!valve.busy()) valve_outputs.run(false);

&#x20;   serviceLocalUI();

&#x20;   serviceDisplay(kotel_on, topeni_on, boiler_on);

&#x20;   delay(5);

&#x20;   return;

&#x20; }



&#x20; // 3. Control/fault actions before Display. Individual sensor errors retain STOP;

&#x20; // complete loss starts a single bounded emergency travel, only rearmed by full recovery.

&#x20; if (any_invalid) {

&#x20;   kotel_on = topeni_on = boiler_on = true;

&#x20;   if (!valve.busy()) {

&#x20;     writeRelay12(0, !kotel_on);

&#x20;     writeRelay12(1, !topeni_on);

&#x20;   }

&#x20;   gpioR3_write(boiler_on);

&#x20;   if (complete_sensor_loss) {

&#x20;     if (!valve.emergency_latched) {

&#x20;       valve.stop(now, valve_outputs);

&#x20;       writeRelay12(0, !kotel_on);

&#x20;       writeRelay12(1, !topeni_on);

&#x20;       valve.startEmergencyTravel(millis(), valve_outputs);

&#x20;     }

&#x20;     valve.updateEmergency(millis(), valve_outputs);

&#x20;   } else {

&#x20;     if (valve.emergency_latched) valve.holdEmergency(now, valve_outputs);

&#x20;     else valve.stop(now, valve_outputs);

&#x20;     writeRelay12(0, !kotel_on);

&#x20;     writeRelay12(1, !topeni_on);

&#x20;   }

&#x20;   if (!valve.busy()) valve_outputs.run(false);

&#x20;   serviceLocalUI();

&#x20;   serviceDisplay(kotel_on, topeni_on, boiler_on);

&#x20;   delay(5);

&#x20;   return;

&#x20; }

&#x20; if (valve.emergency_latched) valve.recover(now, valve_outputs);



&#x20; // 4) KOTEL (primární okruh) – absolutní prahy + původní hystereze



&#x20; // Tvrdé prahy mají prioritu

&#x20; if (t_nt_kotel >= kotel_abs_on) {

&#x20;   kotel_on = true;

&#x20; } else if (t_nt_kotel <= kotel_abs_off) {

&#x20;   kotel_on = false;

&#x20; } else {

&#x20;   // V pásmu 55–75 °C platí původní logika s hysterezí a delta vůči nádrži

&#x20;   if (!kotel_on) {

&#x20;     // OFF -> ON: kotel dost teplý a teplejší než horní část nádrže o kotel_delta

&#x20;     if ((t_nt_kotel >= kotel_start) && (t_nt_kotel >= t_nt_tank_top + kotel_delta)) {

&#x20;       kotel_on = true;

&#x20;     }

&#x20;   } else {

&#x20;     // ON -> OFF: kotel vychladl pod stop nebo je chladnější než nádrž

&#x20;     if ((t_nt_kotel <= kotel_stop) || (t_nt_kotel <= t_nt_tank_top - kotel_delta)) {

&#x20;       kotel_on = false;

&#x20;     }

&#x20;   }

&#x20; }



&#x20; // 5) TOPENÍ (radiátory) – hystereze na horní části nádrže

&#x20; if (!topeni_on) {

&#x20;   if (t_nt_tank_top > (d_topeni_set + topeni_hyst)) {

&#x20;     topeni_on = true;

&#x20;   }

&#x20; } else {

&#x20;   if (t_nt_tank_top < (d_topeni_set - topeni_hyst)) {

&#x20;     topeni_on = false;

&#x20;   }

&#x20; }



&#x20; // 6) BOILER (TUV) – hystereze a zdroj tepla z nádrže (střed)

&#x20; if (!boiler_on) {

&#x20;   if ((t_nt_boiler < (d_boiler_set - topeni_hyst)) && (t_nt_tank_mid > t_nt_boiler)) {

&#x20;     boiler_on = true;

&#x20;   }

&#x20; } else {

&#x20;   if ((t_nt_boiler >= (d_boiler_set + topeni_hyst)) || (t_nt_tank_mid <= t_nt_boiler)) {

&#x20;     boiler_on = false;

&#x20;   }

&#x20; }



&#x20; // Ruční volba je pouze FORCE ON; další stisk vrátí dané čerpadlo do AUT.

&#x20; // Bezpečnostní větve výše zůstávají nadřazené a při poruše čerpadla zapínají.

&#x20; if (pump_force_on[0]) kotel_on = true;

&#x20; if (pump_force_on[1]) topeni_on = true;

&#x20; if (pump_force_on[2]) boiler_on = true;



&#x20; const auto wanted = safety::demand(topeni_on, t_nt_topeni, d_topeni_set, topeni_hyst);



&#x20; // 9) Zápis výstupů (NC mapování pro KOTEL/TOPENI)

&#x20; if (!valve.busy()) { // Avoid repeated I2C writes during the timed movement.

&#x20;   writeRelay12(0, !kotel_on);   // R1 KOTEL (NC)

&#x20;   writeRelay12(1, !topeni_on);  // R2 TOPENI (NC)

&#x20; }

&#x20; gpioR3_write(boiler_on);      // PORTA G1 (Boiler pump)



&#x20; // Actuator decision FIRST; busy() gates local UI and synchronous Display I/O.

&#x20; valve.update(millis(), wanted, valve_outputs);

&#x20; if (!valve.busy()) valve_outputs.run(false);

&#x20; serviceLocalUI();

&#x20; serviceDisplay(kotel_on, topeni_on, boiler_on);

&#x20; delay(5);

}
