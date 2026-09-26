#include "../../src/main.cpp"
#include <cstdio>
static void runFor(uint32_t duration) {
  const uint32_t begin = fake_now;
  while (uint32_t(fake_now-begin)<duration) loop();
}
int main(int argc, char**) {
  if (argc > 1) {
    // Separate process: boot with no NT48 must not move before the stale window.
    ModbusRTUClient.nt_count=0; ModbusRTUClient.dial_count=0;
    setup(); runFor(6000);
    assert(sensor_mode==SensorMode::NT48_LOST && !valve.emergency_latched && !hw_relay[2]);
    runFor(9000); assert(valve.emergency_latched);
    ModbusRTUClient.nt_count=8;
    runFor(5000); assert(!valve.emergency_latched && !hw_relay[2]);
    // Complete fresh frames with every control sensor invalid are also total loss.
    for(auto& value : ModbusRTUClient.nt) value=uint16_t(int16_t(-2731));
    runFor(5000);
    assert(sensor_mode==SensorMode::SENSOR_FAIL && valve.emergency_latched);
    assert(hw_relay[3]==HIGH && !hw_relay[0] && !hw_relay[1] && gpio);
    printf("PASS boot loss: 12 s grace, recovery; fresh frame with all sensors invalid enters emergency\n");
    return 0;
  }
  // A. Actual production electrical mapping, not a duplicate mapping in the mock.
  valve_outputs.run(false);
  const auto cold=safety::demand(true,40,60,2);
  assert(cold==safety::Direction::OPEN_HOT);
  valve_outputs.direction(cold); assert(hw_relay[3]==HIGH);
  const auto hot=safety::demand(true,70,60,2);
  assert(hot==safety::Direction::CLOSE_HOT);
  valve_outputs.direction(hot); assert(hw_relay[3]==LOW);

  // F/H: offline from boot, actual 1 s failed transactions, rollover in live loop.
  fake_now=0xffffff00UL;
  ModbusRTUClient.dial_count=0;
  setup(); assert(!hw_relay[2] && d_topeni_set==60 && d_boiler_set==55);
  const uint32_t offline_start=fake_now;
  while(uint32_t(fake_now-offline_start)<600000) {
    loop();
    assert(sensor_mode==SensorMode::NORMAL && nt48_freshness.current(fake_now));
    assert(!valve.emergency_latched);
  }
  assert(display_link.offline && display_link.failures==3);
  assert(ModbusRTUClient.nt_reads>100 && ModbusRTUClient.max_nt_gap<6000);
  assert(d_topeni_set==60 && d_boiler_set==55 && run_starts>20);
  assert(max_run_ms==1000);
  const unsigned writes_offline=ModbusRTUClient.dial_writes;
  const size_t reads_before=ModbusRTUClient.dial_read_times.size();
  runFor(30000);
  assert(ModbusRTUClient.dial_writes==writes_offline);
  assert(ModbusRTUClient.dial_read_times.size()>=reads_before+2);
  for(size_t i=reads_before+1;i<ModbusRTUClient.dial_read_times.size();++i)
    assert(uint32_t(ModbusRTUClient.dial_read_times[i]-ModbusRTUClient.dial_read_times[i-1])>=10000);

  // G: first COMPLETE frame re-enables 1 s poll and accepts new setpoints.
  ModbusRTUClient.dial_count=7; runFor(12000); assert(display_link.offline);
  ModbusRTUClient.dial_count=8;
  ModbusRTUClient.dial[5]=45; ModbusRTUClient.dial[7]=60;
  runFor(12000);
  assert(!display_link.offline && display_link.failures==0);
  assert(d_topeni_set==45 && d_boiler_set==60);
  const auto recovered_reads=ModbusRTUClient.dial_reads;
  runFor(5000); assert(ModbusRTUClient.dial_reads>=recovered_reads+2);
  ModbusRTUClient.dial_count=0;
  runFor(20000);
  assert(display_link.offline && d_topeni_set==45 && d_boiler_set==60);
  assert(sensor_mode==SensorMode::NORMAL);

  // B: fresh tank-top 90 + failed boiler: alarm still enters; partial fault STOP.
  ModbusRTUClient.nt[0]=900; ModbusRTUClient.nt[4]=uint16_t(int16_t(-2731));
  runFor(5000);
  assert(overheat_active && sensor_mode==SensorMode::SENSOR_FAIL);
  assert(!valve.emergency_latched && !hw_relay[2]);
  assert(!hw_relay[0] && !hw_relay[1] && gpio);
  ModbusRTUClient.nt[0]=820; runFor(5000); assert(!overheat_active);
  ModbusRTUClient.nt[0]=700; ModbusRTUClient.nt[4]=500;
  ModbusRTUClient.nt[5]=450; runFor(5000);
  assert(sensor_mode==SensorMode::NORMAL && !hw_relay[2]);

  // C/D: full communication loss; single total 60 s travel in a one-hour fault.
  ModbusRTUClient.nt_count=0;
  const uint64_t before_loss_run=total_run_ms;
  unsigned episode_edges=0; bool was_emergency=false;
  const uint32_t loss_start=fake_now;
  while(uint32_t(fake_now-loss_start)<3600000) {
    loop();
    if(valve.emergency_latched && !was_emergency) ++episode_edges;
    was_emergency=valve.emergency_latched;
    if(sensor_mode==SensorMode::NT48_LOST) {
      assert(!hw_relay[0] && !hw_relay[1] && gpio);
      assert(hw_relay[3]==HIGH);
    }
  }
  assert(episode_edges==1 && sensor_mode==SensorMode::NT48_LOST);
  assert(valve.state==safety::Valve::State::EMERGENCY_HOLD && !hw_relay[2]);
  assert(total_run_ms-before_loss_run==safety::VALVE_EMERGENCY_TRAVEL_MS);
  const auto held_starts=run_starts; runFor(60000); assert(run_starts==held_starts);

  // E: fresh partial recovery must NOT rearm the episode; valid recovery does.
  ModbusRTUClient.nt_count=8; ModbusRTUClient.nt[4]=uint16_t(int16_t(-2731));
  runFor(5000); assert(valve.emergency_latched && !hw_relay[2]);
  ModbusRTUClient.nt_count=0; runFor(20000); assert(run_starts==held_starts);
  ModbusRTUClient.nt_count=8; ModbusRTUClient.nt[4]=500; ModbusRTUClient.nt[5]=700;
  const uint32_t recovery_start=fake_now;
  while(valve.emergency_latched && uint32_t(fake_now-recovery_start)<10000) loop();
  assert(!valve.emergency_latched && sensor_mode==SensorMode::NORMAL && !hw_relay[2]);
  assert(valve.state==safety::Valve::State::SETTLING);
  runFor(14000); assert(!hw_relay[2] && hw_relay[3]==HIGH);
  runFor(6000); assert(hw_relay[3]==LOW && run_starts>held_starts);
  // New full loss after genuine recovery is allowed exactly one NEW travel.
  ModbusRTUClient.nt_count=0; runFor(20000);
  assert(valve.emergency_latched && sensor_mode==SensorMode::NT48_LOST);
  // Recover while emergency is still moving: no synchronous I/O while RUN ON.
  ModbusRTUClient.nt_count=8; runFor(5000);
  assert(!valve.emergency_latched && !hw_relay[2]);
  printf("PASS A-H: preserved HIGH/LOW direction, independent tank alarm, hour fault: one 60000 ms emergency budget, HOLD, recovery\n");
  printf("PASS offline Display: 10 min + rollover, no stale NT48, max initial poll gap <6000 ms, complete-frame recovery, last SET retained\n");
}
