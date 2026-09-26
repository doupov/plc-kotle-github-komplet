#include "../../src/main.cpp"
#include <cstdio>
int main() {
  setup(); assert(!hw_relay[2]);
  assert(d_topeni_set == 60 && d_boiler_set == 55);
  assert(!pump_force_on[0] && !pump_force_on[1] && !pump_force_on[2]);
  M5.BtnA.clicked=true; M5.BtnB.clicked=true; M5.BtnC.clicked=true;
  serviceLocalUI();
  assert(pump_force_on[0] && pump_force_on[1] && pump_force_on[2]);
  M5.BtnA.clicked=true; M5.BtnB.clicked=true; M5.BtnC.clicked=true;
  serviceLocalUI();
  assert(!pump_force_on[0] && !pump_force_on[1] && !pump_force_on[2]);
  loop();
  ModbusRTUClient.dial[5]=0; ModbusRTUClient.dial[7]=65535;
  assert(readDial_Holding()); assert(d_topeni_set==60 && d_boiler_set==55);
  ModbusRTUClient.dial_count=7; ModbusRTUClient.dial[5]=40;
  assert(!readDial_Holding()); assert(d_topeni_set==60);
  ModbusRTUClient.dial_count=0; assert(!readDial_Holding()); assert(d_topeni_set==60);
  ModbusRTUClient.nt_count=7; float old=t_nt_tank_top;
  assert(!readNT48()); assert(t_nt_tank_top==old);
  ModbusRTUClient.nt_count=0; assert(!readNT48());
  fake_now+=safety::NT48_STALE_TIMEOUT_MS; loop();
  assert(!hw_relay[0] && !hw_relay[1] && gpio && !hw_relay[2]);
  // Recovery and sustained overheat, including offline Display and millis rollover.
  ModbusRTUClient.nt_count=8; ModbusRTUClient.nt[0]=900;
  fake_now=0xffff0000UL;
  const uint32_t begin=fake_now;
  bool was_running=false; uint32_t run_start=0, longest=0; unsigned pulses=0;
  while (uint32_t(fake_now-begin)<600000) {
    loop();
    const bool run=hw_relay[2];
    if(run && !was_running) {run_start=fake_now; ++pulses;}
    if(!run && was_running) {const uint32_t duration=fake_now-run_start; if(duration>longest)longest=duration;}
    if(run) assert(uint32_t(fake_now-run_start)<=safety::VALVE_PULSE_MS);
    was_running=run;
  }
  assert(overheat_active && pulses>20 && max_run_ms == safety::VALVE_PULSE_MS);
  assert(!hw_relay[0] && !hw_relay[1] && gpio);
  // Test opposite temperature error for another five minutes of alarm.
  ModbusRTUClient.nt[5]=800;
  const uint32_t hot_start=fake_now;
  while (uint32_t(fake_now-hot_start)<300000) loop();
  assert(!hw_relay[3] && max_run_ms == safety::VALVE_PULSE_MS);
  // Wait for STOP before explicitly invoking synchronous read functions.
  while(valve.busy()) loop();
  ModbusRTUClient.dial_count=8; ModbusRTUClient.dial[5]=45; ModbusRTUClient.dial[7]=60;
  assert(readDial_Holding()); assert(d_topeni_set==45 && d_boiler_set==60);
  ModbusRTUClient.nt[0]=820;
  fake_now+=3000; loop(); assert(!overheat_active);
  printf("PASS real setup/loop: boot, partial/fail/stale, pump failsafe, recovery, invalid setpoints, alarm exit\n");
  printf("PASS real loop 15-minute alarm, both directions, offline Display, rollover: max actual RUN %u ms (%u opening pulses in first 10 min)\n",max_run_ms,pulses);
}
