#pragma once
#include "Arduino.h"
namespace textdatum_t { constexpr int top_left=0; }
struct ButtonStub {
 bool clicked = false;
 bool wasClicked() { const bool result=clicked; clicked=false; return result; }
};
struct M5Stub {
 ButtonStub BtnA, BtnB, BtnC;
 int config() {return 0;}
 void begin(int) {}
 void update() {assert(!hw_relay[2]);}
};
inline M5Stub M5;
