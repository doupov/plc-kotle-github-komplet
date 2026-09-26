#include "control_safety.h"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace safety;
struct Sink {
  bool running = false;
  Direction dir = Direction::STOP;
  unsigned pulses = 0;
  void run(bool value) { if (value) { assert(!running); ++pulses; } running = value; }
  void direction(Direction value) { assert(!running); dir = value; }
};
struct Client {
  int count = 8, available_count = 8, cut = 8, pos = 0;
  uint16_t values[8] = {700,600,500,750,550,600,0,0};
  int requestFrom(int,int,int,int) { pos = 0; return count; }
  int available() { return pos >= cut ? 0 : available_count - pos; }
  long read() { return values[pos++]; }
};
int main() {
  const uint32_t start = 0xfffffff0UL;
  assert(!expired(0xffffffffUL, start, 32));
  assert(!expired(0, start, 32));
  assert(remaining(0, start, 32) == 16);
  assert(expired(0x10, start, 32));
  assert(remaining(0x11, start, 32) == 0);
  uint32_t last = start; int ticks = 0;
  for (uint32_t i = 1; i <= 128; ++i) if (expired(start+i, last, 32)) { last = start+i; ++ticks; }
  assert(ticks == 4);
  assert(demand(true, 50, 60, 2) == Direction::OPEN_HOT);
  assert(demand(true, 70, 60, 2) == Direction::CLOSE_HOT);
  for (float t : {58.f, 60.f, 62.f}) assert(demand(true,t,60,2) == Direction::STOP);
  assert(demand(false,50,60,2) == Direction::STOP);
  assert(demand(true,NAN,60,2) == Direction::STOP);
  Valve v; Sink out;
  v.update(start, Direction::OPEN_HOT, out); assert(!out.running);
  v.update(start+249, Direction::OPEN_HOT, out); assert(!out.running);
  v.update(start+250, Direction::OPEN_HOT, out); assert(out.running);
  v.update(start+1249, Direction::OPEN_HOT, out); assert(out.running);
  v.update(start+1250, Direction::OPEN_HOT, out); assert(!out.running);
  v.update(start+16249, Direction::CLOSE_HOT, out); assert(out.dir == Direction::OPEN_HOT);
  v.update(start+16250, Direction::CLOSE_HOT, out); assert(!out.running && out.dir == Direction::CLOSE_HOT);
  v.update(start+16500, Direction::CLOSE_HOT, out); assert(out.running);
  v.update(start+16501, Direction::OPEN_HOT, out); assert(!out.running && out.dir == Direction::CLOSE_HOT);
  // Alarm edge interrupts an existing pulse, then remains hot for ten minutes.
  v = Valve{}; out = Sink{};
  v.update(0, Direction::CLOSE_HOT, out); v.update(250, Direction::CLOSE_HOT, out);
  v.stop(300, out); assert(!out.running);
  bool alarm = false, prev = false; uint32_t on_since = 0, max_run = 0;
  for (uint32_t now = 300; now < 600300; now += 5) {
    alarm = updateAlarm(alarm, true, 90);
    const Direction wanted = demand(true, 40, ALARM_DUMP_SUPPLY_SETPOINT, 2);
    v.update(now, wanted, out);
    if (out.running && !prev) on_since = now;
    if (!out.running && prev) { const auto duration = elapsed(now,on_since); if (duration > max_run) max_run = duration; }
    if (out.running) assert(elapsed(now,on_since) <= VALVE_PULSE_MS);
    prev = out.running;
  }
  assert(out.pulses > 20 && max_run == VALVE_PULSE_MS);
  assert(!updateAlarm(false,true,85));
  assert(updateAlarm(false,true,86));
  assert(updateAlarm(true,true,83));
  assert(!updateAlarm(true,true,82));
  assert(updateAlarm(true,false,0));
  Freshness f; assert(!f.current(0)); f.success(start);
  assert(f.current(start+11999)); assert(!f.current(start+12000));
  assert(!f.current(start+1)); // cannot revive after an entire wrap
  f.success(42); assert(f.current(43));
  Client c; uint16_t dest[8] = {123};
  c.count = 0; assert(readRegisters(c,2,0,0,dest) == FrameResult::FAIL && dest[0] == 123);
  c.count = 7; assert(readRegisters(c,2,0,0,dest) == FrameResult::PARTIAL && dest[0] == 123);
  c.count = 8; c.cut = 5; assert(readRegisters(c,2,0,0,dest) == FrameResult::PARTIAL && dest[0] == 123);
  c.cut = 8; assert(readRegisters(c,2,0,0,dest) == FrameResult::OK && dest[0] == 700);
  float set = 60;
  for (float bad : {0.f,34.f,71.f,65535.f,float(NAN)}) assert(!acceptSetpoint(bad,35,70,set) && set == 60);
  assert(acceptSetpoint(35,35,70,set)); assert(acceptSetpoint(70,35,70,set));
  assert(!acceptSetpoint(49,50,65,set)); assert(acceptSetpoint(65,50,65,set));
  // Emergency uses its own budget; rollover and HOLD cannot rearm it.
  Valve emergency; Sink emergency_out;
  const uint32_t e0 = 0xffffff00UL;
  emergency.startEmergencyTravel(e0, emergency_out);
  assert(emergency_out.dir == Direction::OPEN_HOT && !emergency_out.running);
  emergency.updateEmergency(e0+249, emergency_out); assert(!emergency_out.running);
  emergency.updateEmergency(e0+250, emergency_out); assert(emergency_out.running);
  emergency.updateEmergency(e0+250+VALVE_EMERGENCY_TRAVEL_MS-1, emergency_out);
  assert(emergency_out.running);
  emergency.updateEmergency(e0+250+VALVE_EMERGENCY_TRAVEL_MS, emergency_out);
  assert(!emergency_out.running && emergency.state == Valve::State::EMERGENCY_HOLD);
  for(uint32_t i=0; i<3600000; i+=5) {
    emergency.startEmergencyTravel(e0+i, emergency_out);
    emergency.updateEmergency(e0+i, emergency_out);
    assert(!emergency_out.running);
  }
  assert(emergency_out.pulses == 1);
  emergency.recover(100, emergency_out);
  emergency.update(15099, Direction::CLOSE_HOT, emergency_out); assert(!emergency_out.running);
  emergency.update(15100, Direction::CLOSE_HOT, emergency_out);
  emergency.update(15350, Direction::CLOSE_HOT, emergency_out); assert(emergency_out.running);
  emergency.startEmergencyTravel(16000, emergency_out);
  assert(!emergency_out.running && emergency.emergency_run_ms == 0);
  // A poll pause neither resets nor consumes the motor budget.
  emergency.updateEmergency(16250, emergency_out);
  emergency.pauseEmergency(17250, emergency_out);
  assert(emergency.emergency_run_ms == 1000 && !emergency_out.running);
  emergency.updateEmergency(30000, emergency_out);
  emergency.updateEmergency(30250, emergency_out);
  emergency.service(30250+VALVE_EMERGENCY_TRAVEL_MS-1000, emergency_out);
  assert(!emergency_out.running && emergency.emergency_run_ms == VALVE_EMERGENCY_TRAVEL_MS);
  DisplayBackoff link;
  link.completeRead(e0,false); link.completeRead(e0,false); link.completeRead(e0,false);
  assert(link.offline && !link.due(e0+9999) && link.due(e0+10000));
  link.completeRead(100,false); assert(link.offline);
  link.completeRead(200,true); assert(!link.offline && link.failures==0);
  assert(!link.due(1199) && link.due(1200));
  printf("PASS emergency rollover, single travel + hour HOLD, polling pause budget, recovery, Display backoff rollover\n");
  printf("PASS timer rollover, remaining, periodic; valve direction/order/pulse/settling; frames, freshness, setpoints\n");
  printf("PASS 10-minute alarm: %u pulses, longest continuous RUN %u ms\n", out.pulses,max_run);
}
