#include <mishmesh/applets/gps/GpsAlerts.h>
#include <mishmesh/applets/gps/NavTarget.h>
#include <mishmesh/core/Geo.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {

static const char* const NEAR_LABELS[GpsAlerts::OPTIONS] = {"Off", "100 m", "500 m", "1 km", "5 km"};
static const int NEAR_RADIUS_M[GpsAlerts::OPTIONS] = {0, 100, 500, 1000, 5000};
static const char* const NEAR_KEY = "gps_near";

const char* GpsAlerts::nearbyLabel(uint8_t idx) { return NEAR_LABELS[idx < OPTIONS ? idx : 0]; }
int GpsAlerts::nearbyRadiusM(uint8_t idx) { return NEAR_RADIUS_M[idx < OPTIONS ? idx : 0]; }

void GpsAlerts::begin(AppletStorage* storage) {
  _st = storage;
  _near = 0;
  uint8_t b = 0;
  if (_st && _st->load(NEAR_KEY, &b, 1) == 1 && b < OPTIONS) _near = b;
}

void GpsAlerts::setNearbyIndex(uint8_t idx) {
  _near = idx < OPTIONS ? idx : 0;
  if (_st) _st->save(NEAR_KEY, &_near, 1);
}

void GpsAlerts::resetForTest() {
  _st = nullptr; _near = 0; _nextArriveAt = _nextNearAt = 0;
  memset(_cool, 0, sizeof(_cool));
}

static bool due(uint32_t now, uint32_t at) { return (int32_t)(now - at) >= 0; }

bool GpsAlerts::tick(uint32_t nowMs, AppServices& app, ContactsService& svc, GpsAlert& out) {
  if (navTarget().active() && due(nowMs, _nextArriveAt)) {
    _nextArriveAt = nowMs + ARRIVE_CHECK_MS;
    if (checkArrival(app, out)) return true;
  }
  if (_near != 0 && due(nowMs, _nextNearAt)) {
    _nextNearAt = nowMs + NEARBY_CHECK_MS;
    if (checkNearby(nowMs, app, svc, out)) return true;
  }
  return false;
}

static bool liveFix(AppServices& app, int32_t& lat, int32_t& lon) {
  return app.gpsEnabled() && app.gpsHasFix() && app.gpsPosition1e6(lat, lon);
}

bool GpsAlerts::checkArrival(AppServices& app, GpsAlert& out) {
  int32_t lat, lon;
  if (!liveFix(app, lat, lon)) return false;
  NavTarget& t = navTarget();
  float m = geoDistanceKm(lat, lon, t.lat(), t.lon()) * 1000.0f;
  if (!t.armed()) {
    if (m > ARM_M) t.arm();
    return false;
  }
  if (m > ARRIVE_M) return false;
  out.kind = GpsAlert::Arrived;
  snprintf(out.text, sizeof(out.text), "Arrived: %s", t.name());
  out.text[27] = 0;   // fits the host toast
  t.clear();
  return true;
}

bool GpsAlerts::inCooldown(const uint8_t* key, uint32_t nowMs) const {
  for (const Slot& s : _cool)
    if (s.used && memcmp(s.key, key, 6) == 0) return (int32_t)(nowMs - s.until) < 0;
  return false;
}

void GpsAlerts::startCooldown(const uint8_t* key, uint32_t nowMs) {
  Slot* pick = nullptr;
  for (Slot& s : _cool) if (s.used && memcmp(s.key, key, 6) == 0) { pick = &s; break; }
  if (!pick) {
    for (Slot& s : _cool) if (!s.used) { pick = &s; break; }
  }
  if (!pick) {   // full: replace the entry that expires soonest
    pick = &_cool[0];
    for (Slot& s : _cool) if ((int32_t)(s.until - pick->until) < 0) pick = &s;
  }
  pick->used = true;
  memcpy(pick->key, key, 6);
  pick->until = nowMs + COOLDOWN_MS;
}

bool GpsAlerts::checkNearby(uint32_t nowMs, AppServices& app, ContactsService& svc, GpsAlert& out) {
  int32_t lat, lon;
  if (!liveFix(app, lat, lon)) return false;
  // "Fresh" = the contact's advert (their clock) is within STALE_SECS of ours.
  // With either clock unknown the position's age can't be told, so skip.
  uint32_t now = app.epochSeconds();
  if (now == 0) return false;
  float limitKm = nearbyRadiusM(_near) / 1000.0f;

  int n = svc.countByKind(ContactKind::Chat);
  float bestKm = 1e9f;
  uint8_t bestKey[6]; char bestName[32] = {0};
  bool found = false;
  for (int i = 0; i < n; i++) {
    ContactView v;
    if (!svc.getByKind(ContactKind::Chat, i, v) || !v.hasLocation || !v.pubKey) continue;
    if (v.lastAdvert == 0 || (now > v.lastAdvert && now - v.lastAdvert > STALE_SECS)) continue;
    float km = geoDistanceKm(lat, lon, v.gpsLat, v.gpsLon);
    if (km > limitKm || km >= bestKm) continue;
    if (inCooldown(v.pubKey, nowMs)) continue;
    bestKm = km; memcpy(bestKey, v.pubKey, 6);
    strncpy(bestName, v.name ? v.name : "", sizeof(bestName) - 1);
    found = true;
  }
  if (!found) return false;

  char dist[12];
  geoFormatDistance(dist, sizeof(dist), bestKm);
  // Toast holds 27 chars: "Nearby: " + name + " " + dist, shrinking the name to fit.
  int room = 27 - 8 - 1 - (int)strlen(dist);
  if ((int)strlen(bestName) > room) bestName[room > 0 ? room : 0] = 0;
  out.kind = GpsAlert::Nearby;
  snprintf(out.text, sizeof(out.text), "Nearby: %s %s", bestName, dist);
  startCooldown(bestKey, nowMs);
  return true;
}

GpsAlerts& gpsAlerts() {
  static GpsAlerts a;
  return a;
}

}  // namespace mishmesh
