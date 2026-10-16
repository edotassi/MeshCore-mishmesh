#pragma once

#include <stdint.h>
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/MessagesService.h>

namespace mishmesh {

// Periodic location sharing into one conversation, as plain "pin lat,lon (live)"
// text messages. Not persisted: a reboot ends it. Driven by tick() from the
// adapter's main loop (also while the screen is off). Never sends without a live
// fix, so a stale position is never shared.
class LiveShare {
public:
  enum class Event : uint8_t { None, Sent, SendFailed, Ended };
  enum { DURATIONS = 4, INTERVALS = 4 };

  void setTarget(const ConvoKey& k, const char* name);
  bool hasTarget() const { return _hasTarget; }
  const ConvoKey& target() const { return _key; }
  const char* targetName() const { return _name; }

  // Duration 1/2/4/8 h, interval 5/10/15/30 min (default 10). cycle* wraps.
  uint8_t durationIdx() const { return _dur; }
  uint8_t intervalIdx() const { return _int; }
  void cycleDuration() { _dur = (uint8_t)((_dur + 1) % DURATIONS); }
  void cycleInterval() { _int = (uint8_t)((_int + 1) % INTERVALS); }
  static uint32_t durationSecs(uint8_t idx);
  static uint32_t intervalSecs(uint8_t idx);
  static const char* durationLabel(uint8_t idx);
  static const char* intervalLabel(uint8_t idx);

  bool start(uint32_t nowMs);           // false without a target; first send on the next tick
  void stop() { _active = false; }
  bool active() const { return _active; }
  uint32_t remainingSecs(uint32_t nowMs) const;

  Event tick(uint32_t nowMs, AppServices& app, MessagesService& svc);

  void resetForTest();

private:
  bool     _hasTarget = false, _active = false;
  ConvoKey _key{};
  char     _name[32] = {0};
  uint8_t  _dur = 1, _int = 1;          // 2 h / 10 min defaults
  uint32_t _endMs = 0, _nextAt = 0, _nextTickAt = 0;
};

LiveShare& liveShare();

}  // namespace mishmesh
