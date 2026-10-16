#include <mishmesh/applets/gps/LiveShare.h>
#include <mishmesh/applets/gps/LocationText.h>
#include <mishmesh/core/StrUtil.h>

namespace mishmesh {

static const uint32_t DUR_H[LiveShare::DURATIONS] = {1, 2, 4, 8};
static const uint32_t INT_MIN[LiveShare::INTERVALS] = {5, 10, 15, 30};
static const char* const DUR_L[LiveShare::DURATIONS] = {"1 h", "2 h", "4 h", "8 h"};
static const char* const INT_L[LiveShare::INTERVALS] = {"5 min", "10 min", "15 min", "30 min"};

uint32_t LiveShare::durationSecs(uint8_t i) { return DUR_H[i % DURATIONS] * 3600u; }
uint32_t LiveShare::intervalSecs(uint8_t i) { return INT_MIN[i % INTERVALS] * 60u; }
const char* LiveShare::durationLabel(uint8_t i) { return DUR_L[i % DURATIONS]; }
const char* LiveShare::intervalLabel(uint8_t i) { return INT_L[i % INTERVALS]; }

void LiveShare::setTarget(const ConvoKey& k, const char* name) {
  _key = k; _hasTarget = true;
  copyStr(_name, sizeof(_name), name ? name : "");
}

static bool due(uint32_t now, uint32_t at) { return (int32_t)(now - at) >= 0; }

bool LiveShare::start(uint32_t nowMs) {
  if (!_hasTarget) return false;
  _active = true;
  _endMs = nowMs + durationSecs(_dur) * 1000u;
  _nextAt = nowMs;          // first send as soon as there is a fix
  _nextTickAt = nowMs;
  return true;
}

uint32_t LiveShare::remainingSecs(uint32_t nowMs) const {
  if (!_active || due(nowMs, _endMs)) return 0;
  return (_endMs - nowMs) / 1000u;
}

LiveShare::Event LiveShare::tick(uint32_t nowMs, AppServices& app, MessagesService& svc) {
  if (!_active || !due(nowMs, _nextTickAt)) return Event::None;
  _nextTickAt = nowMs + 1000;
  if (due(nowMs, _endMs)) { _active = false; return Event::Ended; }
  if (!due(nowMs, _nextAt)) return Event::None;
  int32_t lat, lon;
  if (!app.gpsEnabled() || !app.gpsHasFix() || !app.gpsPosition1e6(lat, lon))
    return Event::None;     // skip: retry next second, never send stale
  char text[64];
  if (formatLocationText(text, sizeof(text), lat, lon, true) == 0) return Event::None;
  _nextAt = nowMs + intervalSecs(_int) * 1000u;
  return svc.sendText(_key, text) ? Event::Sent : Event::SendFailed;
}

void LiveShare::resetForTest() { *this = LiveShare(); }

LiveShare& liveShare() {
  static LiveShare s;
  return s;
}

}  // namespace mishmesh
