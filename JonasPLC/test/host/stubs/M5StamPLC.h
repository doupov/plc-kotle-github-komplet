#pragma once
#include "Arduino.h"
struct DisplayStub {
 void startWrite() { assert(!hw_relay[2]); }
 void endWrite() {}
 template<class... T> void fillScreen(T...) {}
 template<class... T> void setTextColor(T...) {}
 template<class... T> void setTextDatum(T...) {}
 template<class... T> void setTextSize(T...) {}
 template<class... T> void setCursor(T...) {}
 template<class... T> void println(T...) {}
 template<class... T> void printf(T...) {}
 template<class... T> void setRotation(T...) {}
 template<class... T> void setBrightness(T...) {}
};
struct PLCStub {
 DisplayStub Display;
 void begin() {}
 void writePlcRelay(int i,bool value) {
   if (i == 3) assert(!hw_relay[2]);
   if (i == 2 && value && !hw_relay[i]) { run_on_ms = fake_now; ++run_starts; }
   if (i == 2 && !value && hw_relay[i]) {
     const uint32_t duration = fake_now - run_on_ms;
     if (duration > max_run_ms) max_run_ms = duration;
     total_run_ms += duration;
     assert(duration <= 60000);
   }
   hw_relay[i]=value;
 }
};
inline PLCStub M5StamPLC;
