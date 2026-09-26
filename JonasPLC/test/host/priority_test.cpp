#include "../../src/main.cpp"
#include <cstdio>
static void runFor(uint32_t duration) {
  const uint32_t begin=fake_now;
  while(uint32_t(fake_now-begin)<duration) loop();
}
int main(int argc,char**) {
  if(argc>1) {
    fake_now=0xffffff00UL;
    ModbusRTUClient.begin_result=false;
    setup(); assert(!modbus_ready && !hw_relay[2]);
    runFor(6000);
    assert(!hw_relay[0] && !hw_relay[1] && gpio);
    assert(!hw_relay[2] && !valve.emergency_latched);
    assert(ModbusRTUClient.init_times.size()==2);
    assert(ModbusRTUClient.nt_reads==0 && ModbusRTUClient.dial_reads==0 && ModbusRTUClient.writes==0);
    runFor(3600000);
    assert(valve.emergency_latched && valve.state==safety::Valve::State::EMERGENCY_HOLD);
    assert(!hw_relay[2] && hw_relay[3]==HIGH);
    assert(total_run_ms==60000);
    for(size_t i=1;i<ModbusRTUClient.init_times.size();++i) {
      const uint32_t gap=ModbusRTUClient.init_times[i]-ModbusRTUClient.init_times[i-1];
      assert(gap>=5000 && gap<=5005);
    }
    const unsigned starts=run_starts;
    runFor(10000); assert(run_starts==starts);
    ModbusRTUClient.begin_result=true; ModbusRTUClient.nt_count=0;
    const uint32_t start=fake_now;
    while(!modbus_ready && uint32_t(fake_now-start)<6000) loop();
    assert(modbus_ready && ModbusRTUClient.nt_reads==1);
    assert(!nt48_freshness.valid && valve.emergency_latched && !hw_relay[2]);
    assert(!hw_relay[0] && !hw_relay[1] && gpio);
    ModbusRTUClient.nt_count=7; runFor(4000);
    assert(valve.emergency_latched); // Complete frame required, not merely begin success.
    ModbusRTUClient.nt_count=8;
    runFor(4000);
    assert(!valve.emergency_latched && sensor_mode==SensorMode::NORMAL && !hw_relay[2]);
    assert(valve.state==safety::Valve::State::SETTLING);
    runFor(20000); assert(run_starts>starts);
    printf("PASS init fail: loop/pumps, retry 5 s across rollover, hour fault with 60000 ms ON, complete-frame recovery\n");
    return 0;
  }
  ModbusRTUClient.dial_count=0;
  ModbusRTUClient.nt[0]=900;
  ModbusRTUClient.nt[5]=550;
  ModbusRTUClient.nt[4]=uint16_t(int16_t(-2731));
  ModbusRTUClient.nt[1]=uint16_t(int16_t(-2731));
  ModbusRTUClient.nt[3]=uint16_t(int16_t(-2731));
  setup(); runFor(25000);
  assert(overheat_active && sensor_mode==SensorMode::SENSOR_FAIL);
  assert(!isPlausibleTemp(t_nt_boiler) && !isPlausibleTemp(t_nt_tank_mid) && !isPlausibleTemp(t_nt_kotel));
  assert(!hw_relay[0] && !hw_relay[1] && gpio);
  assert(run_starts>0 && hw_relay[3]==HIGH && max_run_ms==1000);

  // A due Display request must not win over an eligible valve start.
  valve.stop(fake_now,valve_outputs);
  valve.state=safety::Valve::State::IDLE;
  lastPollNT48=fake_now;
  display_link.last_read_ms=fake_now-safety::DISPLAY_OFFLINE_RETRY_MS;
  const auto reads=ModbusRTUClient.dial_reads;
  loop();
  assert(valve.state==safety::Valve::State::DEADTIME);
  assert(ModbusRTUClient.dial_reads==reads);
  runFor(300); assert(hw_relay[2] && hw_relay[3]==HIGH);
  runFor(1000); assert(!hw_relay[2]);

  // Missing supply feedback: alarm still ON, generic fault STOP, no guessed demand.
  ModbusRTUClient.nt[5]=uint16_t(int16_t(-2731));
  runFor(5000);
  assert(overheat_active && !isPlausibleTemp(t_nt_topeni));
  assert(!hw_relay[2] && !valve.emergency_latched);
  const unsigned stopped_starts=run_starts;
  runFor(20000); assert(run_starts==stopped_starts);

  // An old full-loss latch can recover into heat dump with unrelated sensors bad.
  ModbusRTUClient.nt_count=0; runFor(20000);
  assert(valve.emergency_latched);
  ModbusRTUClient.nt_count=8; ModbusRTUClient.nt[5]=550;
  const uint32_t restore=fake_now;
  while(valve.emergency_latched && uint32_t(fake_now-restore)<6000) loop();
  assert(!valve.emergency_latched && !hw_relay[2]);
  assert(overheat_active && sensor_mode==SensorMode::SENSOR_FAIL);
  assert(valve.state==safety::Valve::State::SETTLING);
  const unsigned recovered_starts=run_starts;
  runFor(14000); assert(run_starts==recovered_starts);
  runFor(6000); assert(run_starts>recovered_starts && hw_relay[3]==HIGH);
  ModbusRTUClient.nt[5]=700; runFor(25000); assert(hw_relay[3]==LOW);
  // Normal branch has the same actuator-before-Display priority.
  ModbusRTUClient.nt[0]=700; ModbusRTUClient.nt[1]=600;
  ModbusRTUClient.nt[3]=800; ModbusRTUClient.nt[4]=500; ModbusRTUClient.nt[5]=400;
  runFor(5000); assert(sensor_mode==SensorMode::NORMAL && !overheat_active);
  valve.stop(fake_now,valve_outputs); valve.state=safety::Valve::State::IDLE;
  lastPollNT48=fake_now; display_link.last_read_ms=fake_now-safety::DISPLAY_OFFLINE_RETRY_MS;
  const auto normal_reads=ModbusRTUClient.dial_reads;
  loop(); assert(valve.state==safety::Valve::State::DEADTIME);
  assert(ModbusRTUClient.dial_reads==normal_reads);
  printf("PASS heat dump precedes unrelated faults, missing feedback STOP, emergency recovery, HIGH/LOW mapping, actuator before offline Display\n");
}
