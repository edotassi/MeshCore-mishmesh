#include <mishmesh/applets/gps/WaypointStore.h>
#include <mishmesh/core/StrUtil.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {

// Blob: 'W' 'P' version count, then count records of
// name[13] + lat(4 LE) + lon(4 LE) + created(4 LE) = 25 bytes.
static const uint8_t MAGIC0 = 'W', MAGIC1 = 'P', VERSION = 1;
static const size_t HDR = 4, REC = 25;
static const char* const KEY = "waypoints";

static void put32(uint8_t* p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static uint32_t get32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void WaypointStore::begin(AppletStorage* storage) {
  _st = storage;
  _n = 0;
#if MISHMESH_GPS_NAV
  if (!_st) return;
  uint8_t buf[HDR + CAPACITY * REC];
  size_t got = _st->loadBlob(KEY, buf, sizeof(buf));
  if (got < HDR || buf[0] != MAGIC0 || buf[1] != MAGIC1 || buf[2] != VERSION) return;
  int n = buf[3];
  if (n > CAPACITY || got != HDR + (size_t)n * REC) return;
  for (int i = 0; i < n; i++) {
    const uint8_t* r = buf + HDR + i * REC;
    memcpy(_wp[i].name, r, NAME_LEN);
    _wp[i].name[NAME_LEN] = 0;
    _wp[i].lat1e6 = (int32_t)get32(r + 13);
    _wp[i].lon1e6 = (int32_t)get32(r + 17);
    _wp[i].created = get32(r + 21);
  }
  _n = n;
#endif
}

void WaypointStore::save() {
#if MISHMESH_GPS_NAV
  if (!_st) return;
  uint8_t buf[HDR + CAPACITY * REC];
  buf[0] = MAGIC0; buf[1] = MAGIC1; buf[2] = VERSION; buf[3] = (uint8_t)_n;
  for (int i = 0; i < _n; i++) {
    uint8_t* r = buf + HDR + i * REC;
    memset(r, 0, 13);
    memcpy(r, _wp[i].name, strlen(_wp[i].name));
    put32(r + 13, (uint32_t)_wp[i].lat1e6);
    put32(r + 17, (uint32_t)_wp[i].lon1e6);
    put32(r + 21, _wp[i].created);
  }
  _st->saveBlob(KEY, buf, HDR + (size_t)_n * REC);
#endif
}

bool WaypointStore::get(int i, Waypoint& out) const {
  if (i < 0 || i >= _n) return false;
  out = _wp[i];
  return true;
}

int WaypointStore::add(const char* name, int32_t lat1e6, int32_t lon1e6, uint32_t created) {
  if (_n >= CAPACITY) return -1;
  Waypoint& w = _wp[_n];
  copyStr(w.name, sizeof(w.name), name ? name : "");
  w.lat1e6 = lat1e6; w.lon1e6 = lon1e6; w.created = created;
  _n++;
  save();
  return _n - 1;
}

bool WaypointStore::rename(int i, const char* name) {
  if (i < 0 || i >= _n || !name || !name[0]) return false;
  copyStr(_wp[i].name, sizeof(_wp[i].name), name);
  save();
  return true;
}

bool WaypointStore::remove(int i) {
  if (i < 0 || i >= _n) return false;
  for (int k = i; k < _n - 1; k++) _wp[k] = _wp[k + 1];
  _n--;
  save();
  return true;
}

void WaypointStore::nextDefaultName(char* buf, size_t cap) const {
  int n = 1;
  for (; n < 99; n++) {
    char cand[8];
    snprintf(cand, sizeof(cand), "WP%02d", n);
    bool used = false;
    for (int i = 0; i < _n; i++) if (strcmp(_wp[i].name, cand) == 0) { used = true; break; }
    if (!used) break;
  }
  snprintf(buf, cap, "WP%02d", n);
}

}  // namespace mishmesh
