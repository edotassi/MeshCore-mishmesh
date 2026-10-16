#pragma once

#include <stddef.h>
#include <stdint.h>
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/AppletStorage.h>
#include <mishmesh/core/ContactsService.h>

namespace mishmesh {

// What the adapter should announce (sound + wake + toast).
struct GpsAlert {
  enum Kind : uint8_t { Arrived, Nearby } kind;
  char text[32];
};

// Two background GPS checks, driven from the adapter's main loop and independent
// of which applet is on screen (or whether the display is asleep):
//  - arrival at the active NavTarget (<= 30 m, once armed by being > 60 m away);
//  - "Nearby alert": a Chat contact with a fresh advert position within the
//    configured radius (cooldown 1 h per contact, one alert per check).
// All state lives here; the caller only delivers the returned alert.
class GpsAlerts {
public:
  static const uint32_t ARRIVE_CHECK_MS = 1000;
  static const uint32_t NEARBY_CHECK_MS = 30000;
  static const uint32_t COOLDOWN_MS     = 3600000UL;   // per contact
  static const uint32_t STALE_SECS      = 7200;        // advert older than this: skip
  static constexpr float ARRIVE_M = 30.0f, ARM_M = 60.0f;
  enum { OPTIONS = 5, COOLDOWN_SLOTS = 16 };

  // Loads the persisted "Nearby alert" option (key "gps_near").
  void begin(AppletStorage* storage);
  uint8_t nearbyIndex() const { return _near; }
  void setNearbyIndex(uint8_t idx);                 // clamps; persists
  static const char* nearbyLabel(uint8_t idx);      // "Off", "100 m", ...
  static int nearbyRadiusM(uint8_t idx);            // 0 = off

  // At most one alert per call. `nowMs` is the monotonic loop clock.
  bool tick(uint32_t nowMs, AppServices& app, ContactsService& svc, GpsAlert& out);

  void resetForTest();

private:
  bool checkArrival(AppServices& app, GpsAlert& out);
  bool checkNearby(uint32_t nowMs, AppServices& app, ContactsService& svc, GpsAlert& out);
  bool inCooldown(const uint8_t* key, uint32_t nowMs) const;
  void startCooldown(const uint8_t* key, uint32_t nowMs);

  AppletStorage* _st = nullptr;
  uint8_t  _near = 0;
  uint32_t _nextArriveAt = 0, _nextNearAt = 0;
  struct Slot { bool used; uint8_t key[6]; uint32_t until; } _cool[COOLDOWN_SLOTS] = {};
};

GpsAlerts& gpsAlerts();

}  // namespace mishmesh
