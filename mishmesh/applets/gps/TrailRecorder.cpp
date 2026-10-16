#include <mishmesh/applets/gps/TrailRecorder.h>
#include <mishmesh/core/Geo.h>
#include <string.h>

namespace mishmesh {

const char* const TrailRecorder::POINTS_KEY = "trail";
const char* const TrailRecorder::META_KEY = "trail_meta";

// Meta: 'T' 'R' version flags(bit0 recording) u32 count f32 lengthM u32 first u32 last.
static const uint8_t M0 = 'T', M1 = 'R', MVER = 1;
static const size_t META_LEN = 20;

static void put32(uint8_t* p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static uint32_t get32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static bool due(uint32_t now, uint32_t at) { return (int32_t)(now - at) >= 0; }

void TrailRecorder::begin(AppletStorage* storage) {
  _st = storage;
  _recording = false; _needSeg = true; _count = 0; _lenKm = 0; _first = _last = 0;
  _n = 0; _haveLast = false; _nextTickAt = 0;
  if (!_st) return;
  size_t blob = _st->blobSize(POINTS_KEY);
  uint8_t m[META_LEN];
  if (_st->load(META_KEY, m, (uint8_t)META_LEN) == META_LEN && m[0] == M0 && m[1] == M1 && m[2] == MVER) {
    _recording = (m[3] & 1) != 0;
    _count = get32(m + 4);
    float lenM; uint32_t bits = get32(m + 8); memcpy(&lenM, &bits, 4);
    _lenKm = lenM > 0 ? lenM / 1000.0f : 0;
    _first = get32(m + 12); _last = get32(m + 16);
  }
  // Power loss between the append and the meta save (or a vanished file): trust the file.
  uint32_t fileCount = (uint32_t)(blob / REC_BYTES);
  if (fileCount != _count) {
    _count = fileCount;
    if (_count == 0) { _lenKm = 0; _first = _last = 0; }
  }
  if (_count > CAP) _count = CAP;
}

void TrailRecorder::saveMeta() {
  if (!_st) return;
  uint8_t m[META_LEN];
  m[0] = M0; m[1] = M1; m[2] = MVER; m[3] = _recording ? 1 : 0;
  put32(m + 4, _count);
  float lenM = _lenKm * 1000.0f; uint32_t bits; memcpy(&bits, &lenM, 4);
  put32(m + 8, bits);
  put32(m + 12, _first); put32(m + 16, _last);
  _st->save(META_KEY, m, (uint8_t)META_LEN);
}

void TrailRecorder::flush() {
  if (_n > 0 && _st) {
    uint8_t buf[BATCH * REC_BYTES];
    for (int i = 0; i < _n; i++) {
      uint8_t* r = buf + i * REC_BYTES;
      put32(r, (uint32_t)_buf[i].lat); put32(r + 4, (uint32_t)_buf[i].lon);
      put32(r + 8, _buf[i].time);
      r[12] = (uint8_t)(uint16_t)_buf[i].alt; r[13] = (uint8_t)((uint16_t)_buf[i].alt >> 8);
      r[14] = _buf[i].flags; r[15] = 0;
    }
    if (!_st->appendBlob(POINTS_KEY, buf, (size_t)_n * REC_BYTES)) return;   // keep the buffer, retry later
    _count += _n;
    _n = 0;
  } else if (_n > 0) {
    _n = 0;   // no storage at all: nothing to keep
  }
  saveMeta();
}

bool TrailRecorder::start() {
  if (count() >= CAP) return false;
  _recording = true; _needSeg = true;
  saveMeta();
  return true;
}

void TrailRecorder::stop() {
  _recording = false;
  flush();
}

void TrailRecorder::clear() {
  _n = 0;                       // buffered points are discarded with the rest
  if (_st) _st->removeBlob(POINTS_KEY);
  _count = 0; _lenKm = 0; _first = _last = 0;
  _haveLast = false; _needSeg = true;
  saveMeta();
}

TrailRecorder::State TrailRecorder::state(const AppServices& app) const {
  if (count() >= CAP) return State::Full;
  if (!_recording) return State::Stopped;
  if (!app.gpsEnabled()) return State::PausedGpsOff;
  if (!app.gpsHasFix()) return State::PausedNoFix;
  return State::Recording;
}

void TrailRecorder::addPoint(uint32_t nowMs, int32_t lat, int32_t lon, AppServices& app, bool segStart) {
  if (_n >= BATCH) return;   // buffer stuck (append failing): drop rather than grow
  Pt& p = _buf[_n];
  p.lat = lat; p.lon = lon; p.time = app.epochSeconds();
  float a = app.gpsAltitudeM();
  a = a > 32767.0f ? 32767.0f : (a < -32768.0f ? -32768.0f : a);
  p.alt = (int16_t)a;
  p.flags = segStart ? 1 : 0;
  if (_n == 0) _oldestBufMs = nowMs;
  _n++;
  if (!segStart && _haveLast) _lenKm += geoDistanceKm(_lastLat, _lastLon, lat, lon);
  _haveLast = true; _lastLat = lat; _lastLon = lon; _lastPointMs = nowMs;
  if (p.time != 0) { if (_first == 0) _first = p.time; _last = p.time; }
  _needSeg = false;
}

bool TrailRecorder::tick(uint32_t nowMs, AppServices& app) {
  if (!due(nowMs, _nextTickAt)) return false;
  _nextTickAt = nowMs + TICK_MS;

  if (_n > 0 && (_n >= BATCH || (int32_t)(nowMs - _oldestBufMs) >= (int32_t)FLUSH_MS)) {
    flush();
    if (_n > 0) _oldestBufMs = nowMs;   // append failed: back off a full interval
  }
  if (!_recording) return false;

  if (!app.gpsEnabled()) { _needSeg = true; return false; }   // paused; resume = new segment
  int32_t lat, lon;
  if (!app.gpsHasFix() || !app.gpsPosition1e6(lat, lon)) {
    if ((int32_t)(nowMs - _lastFixMs) >= (int32_t)HEARTBEAT_MS) _needSeg = true;   // long gap
    return false;
  }
  uint32_t sinceFix = nowMs - _lastFixMs;
  _lastFixMs = nowMs;
  if (sinceFix >= HEARTBEAT_MS && _haveLast) _needSeg = true;   // came back after a long gap

  bool take = _needSeg || !_haveLast ||
              geoDistanceKm(_lastLat, _lastLon, lat, lon) * 1000.0f >= (float)MIN_MOVE_M ||
              (int32_t)(nowMs - _lastPointMs) >= (int32_t)HEARTBEAT_MS;
  if (!take) return false;

  addPoint(nowMs, lat, lon, app, _needSeg || !_haveLast);
  if (_n >= BATCH) flush();

  if (count() >= CAP) {          // cap reached: stop, persist, announce once
    _recording = false;
    flush();
    return true;
  }
  return false;
}

TrailRecorder& trailRecorder() {
  static TrailRecorder t;
  return t;
}

}  // namespace mishmesh
