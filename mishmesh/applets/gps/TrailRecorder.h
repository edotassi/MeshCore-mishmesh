#pragma once

#include <stddef.h>
#include <stdint.h>
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/AppletStorage.h>

namespace mishmesh {

// Append-only GPS track. Points go to the blob "trail" in 16 B records (batched, so
// the flash sees one small append per 8 points or per minute, never a rewrite);
// "trail_meta" holds the summary and the recording flag. When the cap is reached
// recording stops until the trail is cleared.
//
// Record (little-endian): i32 lat1e6, i32 lon1e6, u32 epoch secs (0 = clock
// unknown), i16 altitude m, u8 flags (bit0 = segment start), u8 reserved.
class TrailRecorder {
public:
  enum { CAP = 4096, BATCH = 8, REC_BYTES = 16 };   // 4096 * 16 B = 64 KiB
  static const uint32_t MIN_MOVE_M   = 10;
  static const uint32_t HEARTBEAT_MS = 5UL * 60 * 1000;
  static const uint32_t FLUSH_MS     = 60UL * 1000;
  static const uint32_t TICK_MS      = 1000;
  enum class State : uint8_t { Stopped, Recording, PausedNoFix, PausedGpsOff, Full };

  // Loads the meta; the point count is repaired from the blob size if they disagree.
  // A trail that was recording resumes (first new point = segment start).
  void begin(AppletStorage* storage);

  bool start();            // false when full
  void stop();             // flushes
  void clear();            // flushes, deletes the points, zeroes the summary

  bool recording() const { return _recording; }
  State state(const AppServices& app) const;
  uint32_t count() const { return _count + _n; }
  float lengthKm() const { return _lenKm; }
  uint32_t firstTime() const { return _first; }
  uint32_t lastTime() const { return _last; }

  // Call from the main loop. Returns true exactly once when the cap is reached.
  bool tick(uint32_t nowMs, AppServices& app);

  void flush();            // append the RAM buffer + save meta

  static const char* const POINTS_KEY;
  static const char* const META_KEY;

private:
  struct Pt { int32_t lat, lon; uint32_t time; int16_t alt; uint8_t flags; };
  void addPoint(uint32_t nowMs, int32_t lat, int32_t lon, AppServices& app, bool segStart);
  void saveMeta();

  AppletStorage* _st = nullptr;
  bool     _recording = false;
  bool     _needSeg = true;       // next point starts a segment
  uint32_t _count = 0;            // points on flash
  float    _lenKm = 0;
  uint32_t _first = 0, _last = 0;
  Pt       _buf[BATCH];
  int      _n = 0;
  uint32_t _oldestBufMs = 0;
  bool     _haveLast = false;
  int32_t  _lastLat = 0, _lastLon = 0;
  uint32_t _lastPointMs = 0, _lastFixMs = 0, _nextTickAt = 0;
};

TrailRecorder& trailRecorder();

}  // namespace mishmesh
