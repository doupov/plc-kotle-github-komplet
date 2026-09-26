#pragma once
#include <cstdint>
#include <cmath>
#include <cassert>
#define F(x) x
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define SERIAL_8N1 0
#define TFT_BLACK 0
#define TFT_WHITE 1
inline uint32_t fake_now = 0;
inline bool hw_relay[4] = {};
inline int gpio = 0;
inline uint32_t run_on_ms = 0, max_run_ms = 0;
inline uint64_t total_run_ms = 0;
inline unsigned run_starts = 0;
inline uint32_t millis() { return fake_now; }
inline void delay(uint32_t ms) { fake_now += ms; }
inline void pinMode(int,int) {}
inline void digitalWrite(int,int value) { gpio=value; }
inline int digitalRead(int) { return gpio; }
struct SerialStub {
 void begin(int) {}
 template<class... T> void println(T...) {}
 template<class... T> void printf(T...) {}
};
inline SerialStub Serial, Serial2;
