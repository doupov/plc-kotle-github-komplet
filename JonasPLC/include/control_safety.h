#pragma once
#include <stdint.h>
#include <math.h>

namespace safety {
inline uint32_t elapsed(uint32_t now, uint32_t start) { return uint32_t(now - start); }
inline bool expired(uint32_t now, uint32_t start, uint32_t duration) {
  return elapsed(now, start) >= duration;
}
inline uint32_t remaining(uint32_t now, uint32_t start, uint32_t duration) {
  const uint32_t age = elapsed(now, start);
  return age >= duration ? 0 : duration - age;
}

// Installed actuator: ESBE ARA655, 60 s / 90 degrees (specified by user).
// 1 s motion is nominally 1/60 stroke (1.5 degrees), then 15 s to observe response.
// 250 ms for relay direction switching; verify relay/servo specifications on site.
// Tune these constants to the actual full-travel time and hydraulic lag.
constexpr uint32_t VALVE_PULSE_MS = 1000;
constexpr uint32_t VALVE_SETTLE_MS = 15000;
constexpr uint32_t VALVE_DIRECTION_DEADTIME_MS = 250;
// Four nominal 3 s NT48 polls; also tolerates a failed Display transaction batch.
constexpr uint32_t NT48_STALE_TIMEOUT_MS = 12000;
constexpr float OVERHEAT_ENTER_TEMP = 85.0f; // Preserve original strict > 85 activation.
constexpr float OVERHEAT_EXIT_TEMP = 82.0f;  // 3 C hysteresis; release at <= 82.
// TODO: Verify that 60 C radiator supply provides sufficient emergency heat-dump
// capacity for the actual boiler/system. Target unchanged.
constexpr float ALARM_DUMP_SUPPLY_SETPOINT = 60.0f;
static_assert(VALVE_PULSE_MS > 0 && VALVE_PULSE_MS < 0x80000000UL, "pulse interval");
static_assert(VALVE_SETTLE_MS > 0 && VALVE_SETTLE_MS < 0x80000000UL, "settle interval");
static_assert(VALVE_DIRECTION_DEADTIME_MS > 0 && VALVE_DIRECTION_DEADTIME_MS < 0x80000000UL, "deadtime interval");
static_assert(OVERHEAT_EXIT_TEMP < OVERHEAT_ENTER_TEMP, "alarm hysteresis");

enum class Direction : uint8_t { STOP, OPEN_HOT, CLOSE_HOT };
// ESBE ARA655: full travel 60 s / 90 degrees, as specified by the user.
// One nominal full-stroke ON-time budget. Requires working end stops.
// Total commanded motor ON time per fault episode; NT48 polling pauses are excluded.
constexpr uint32_t VALVE_EMERGENCY_TRAVEL_MS = 60000;
constexpr Direction VALVE_EMERGENCY_DIRECTION = Direction::OPEN_HOT; // R4 HIGH, unchanged.
static_assert(VALVE_EMERGENCY_DIRECTION != Direction::STOP, "emergency requires a travel direction");
static_assert(VALVE_EMERGENCY_TRAVEL_MS > 0 && VALVE_EMERGENCY_TRAVEL_MS < 0x80000000UL,
              "emergency travel interval");
inline Direction demand(bool enabled, float actual, float target, float hyst) {
  if (!enabled || !isfinite(actual) || !isfinite(target)) return Direction::STOP;
  if (actual < target - hyst) return Direction::OPEN_HOT;
  if (actual > target + hyst) return Direction::CLOSE_HOT;
  return Direction::STOP;
}

// Sink exposes run(bool), direction(Direction). No electrical polarity in control.
class Valve {
public:
  enum class State : uint8_t { IDLE, DEADTIME, PULSE, SETTLING,
    EMERGENCY_DEADTIME, EMERGENCY_TRAVEL, EMERGENCY_PAUSED, EMERGENCY_HOLD };
  State state = State::IDLE;
  Direction direction = Direction::STOP;
  uint32_t started = 0;
  bool emergency_latched = false;
  uint32_t emergency_run_ms = 0; // Sum of completed ON segments, never a countdown.
  bool busy() const {
    return state == State::PULSE || state == State::DEADTIME ||
           state == State::EMERGENCY_TRAVEL || state == State::EMERGENCY_DEADTIME;
  }
  template<class Sink> void pauseEmergency(uint32_t now, Sink& out) {
    if (state != State::EMERGENCY_TRAVEL && state != State::EMERGENCY_DEADTIME) return;
    out.run(false);
    if (state == State::EMERGENCY_TRAVEL) {
      const uint32_t segment = elapsed(now, started);
      const uint32_t budget = VALVE_EMERGENCY_TRAVEL_MS - emergency_run_ms;
      emergency_run_ms += segment < budget ? segment : budget;
    }
    state = emergency_run_ms >= VALVE_EMERGENCY_TRAVEL_MS ? State::EMERGENCY_HOLD
                                                        : State::EMERGENCY_PAUSED;
  }
  template<class Sink> void service(uint32_t now, Sink& out) {
    // Called FIRST in loop; only stop here, never start before processing sensors.
    if (state == State::PULSE && expired(now, started, VALVE_PULSE_MS)) stop(now, out);
    if (state == State::EMERGENCY_TRAVEL &&
        expired(now, started, VALVE_EMERGENCY_TRAVEL_MS - emergency_run_ms))
      pauseEmergency(now, out);
  }
  template<class Sink> void startEmergencyTravel(uint32_t now, Sink& out) {
    if (emergency_latched) return; // HOLD cannot restart itself, even after hours.
    out.run(false);
    emergency_latched = true;
    emergency_run_ms = 0;
    direction = VALVE_EMERGENCY_DIRECTION;
    out.direction(direction);
    started = now;
    state = State::EMERGENCY_DEADTIME;
  }
  template<class Sink> void updateEmergency(uint32_t now, Sink& out) {
    service(now, out);
    if (state == State::EMERGENCY_PAUSED) {
      out.run(false);
      out.direction(direction);
      started = now;
      state = State::EMERGENCY_DEADTIME;
    } else if (state == State::EMERGENCY_DEADTIME &&
               expired(now, started, VALVE_DIRECTION_DEADTIME_MS)) {
      started = now;
      state = State::EMERGENCY_TRAVEL;
      out.run(true);
    }
  }
  template<class Sink> void holdEmergency(uint32_t now, Sink& out) {
    pauseEmergency(now, out);
    out.run(false);
    state = State::EMERGENCY_HOLD; // Partial recovery stops travel, keeps episode latch.
  }
  template<class Sink> void recover(uint32_t now, Sink& out) {
    pauseEmergency(now, out);
    emergency_latched = false;
    stop(now, out); // STOP + settling before any normal reversal.
  }
  template<class Sink> void stop(uint32_t now, Sink& out) {
    out.run(false);
    if (state != State::SETTLING) started = now;
    state = State::SETTLING;
  }
  template<class Sink> void update(uint32_t now, Direction wanted, Sink& out) {
    if (emergency_latched) return;
    if (state == State::PULSE) {
      if (wanted != direction || expired(now, started, VALVE_PULSE_MS)) stop(now, out);
      return;
    }
    if (state == State::SETTLING) {
      if (!expired(now, started, VALVE_SETTLE_MS)) return;
      state = State::IDLE;
    }
    if (state == State::DEADTIME) {
      if (wanted != direction) { stop(now, out); return; }
      if (expired(now, started, VALVE_DIRECTION_DEADTIME_MS)) {
        started = now;
        state = State::PULSE;
        out.run(true);
      }
      return;
    }
    if (wanted == Direction::STOP) return;
    out.run(false);                 // Always before DIR, even the first movement.
    out.direction(wanted);
    direction = wanted;
    started = now;
    state = State::DEADTIME;
  }
};

constexpr uint32_t DISPLAY_POLL_MS = 1000;
constexpr uint32_t DISPLAY_OFFLINE_RETRY_MS = 10000;
struct DisplayBackoff {
  uint8_t failures = 0;
  bool offline = false;
  uint32_t last_read_ms = 0;
  bool due(uint32_t now) const {
    return expired(now, last_read_ms, offline ? DISPLAY_OFFLINE_RETRY_MS : DISPLAY_POLL_MS);
  }
  void failed(uint32_t now) {
    if (failures < 3) ++failures;
    if (failures >= 3 && !offline) { offline = true; last_read_ms = now; }
  }
  void completeRead(uint32_t now, bool ok) {
    last_read_ms = now; // Space retries from completion, including synchronous timeout.
    if (ok) { failures = 0; offline = false; } else failed(now);
  }
};

struct Freshness {
  bool valid = false;
  uint32_t last_ok = 0;
  void success(uint32_t now) { valid = true; last_ok = now; }
  bool current(uint32_t now) {
    // Latch expiry: a second millis wrap must never revive an old measurement.
    if (valid && expired(now, last_ok, NT48_STALE_TIMEOUT_MS)) valid = false;
    return valid;
  }
};
inline bool updateAlarm(bool active, bool valid, float tank) {
  if (!valid) return active; // Retain latch; failsafe has priority while data invalid.
  return active ? tank > OVERHEAT_EXIT_TEMP : tank > OVERHEAT_ENTER_TEMP;
}
inline bool acceptSetpoint(float value, float lo, float hi, float& saved) {
  if (!isfinite(value) || value < lo || value > hi) return false;
  saved = value;
  return true;
}
enum class FrameResult { OK, FAIL, PARTIAL };
template<class Client> FrameResult readRegisters(Client& client, int id, int type,
                                                int address, uint16_t (&dest)[8]) {
  const int count = client.requestFrom(id, type, address, 8);
  if (count <= 0) return FrameResult::FAIL;
  if (count != 8 || client.available() != 8) return FrameResult::PARTIAL;
  uint16_t staged[8];
  for (int i = 0; i < 8; ++i) {
    if (client.available() <= 0) return FrameResult::PARTIAL;
    const long value = client.read();
    if (value < 0 || value > 65535) return FrameResult::PARTIAL;
    staged[i] = uint16_t(value);
  }
  for (int i = 0; i < 8; ++i) dest[i] = staged[i];
  return FrameResult::OK;
}
} // namespace safety
