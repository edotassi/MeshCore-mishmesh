#pragma once

#include <stdint.h>

namespace mishmesh {

struct AppletStorage;

// A fixed-size ring buffer of battery voltage samples (mV), bucketed by
// wall-clock time (UNIX seconds, not millis() which resets on reboot) so the
// history survives a restart via save()/load() through AppletStorage.
// 48 half-hour buckets = a rolling 24 hours. No heap.
//
// Unlike AirtimeHistory's cumulative-counter banking, voltage is a
// point-in-time gauge: tick() just records the latest reading into the
// current bucket. A gap (the device was off, or the clock wasn't synced yet)
// carries the last known reading forward into the skipped buckets rather
// than dropping to a misleading 0 - the pack only ever discharges, it
// doesn't vanish while the screen is off.
class BatteryHistory {
public:
  static const int      BUCKETS     = 48;
  static const uint32_t BUCKET_SECS = 1800;   // 30 minutes

  BatteryHistory() { reset(); }

  void reset();

  // Fold in the latest reading. nowEpoch is UNIX seconds; 0 means the clock
  // isn't synced yet, and the sample is skipped rather than corrupting the
  // bucket math (a synced clock always advances forward from here).
  void tick(uint32_t nowEpoch, uint16_t mv);

  int      bucketCount() const { return BUCKETS; }
  uint32_t bucketSecs()  const { return BUCKET_SECS; }
  // Chronological access: i=0 is the oldest retained bucket, i=BUCKETS-1 is
  // the current (still-filling) one. 0 = no sample yet (never a real
  // reading - a safe "empty" sentinel, since real voltages are always > 0).
  uint16_t bucket(int i) const;

  bool primed() const { return _primed; }
  // 0 if no real sample has landed yet (all buckets still empty).
  uint16_t minMv() const;
  uint16_t maxMv() const;

  // Persist/restore through the generic AppletStorage seam. Same-device
  // round trip only (raw native-endianness field copy, no cross-platform
  // format concern). load() returns false on an absent/short/corrupt blob
  // and leaves the history reset rather than partially applied.
  bool save(AppletStorage* storage, const char* key) const;
  bool load(AppletStorage* storage, const char* key);

private:
  static const int BLOB_BYTES = 4 + 1 + 1 + 2 + BUCKETS * 2;

  uint16_t _mv[BUCKETS];
  uint8_t  _head;          // index of the current (newest) bucket
  bool     _primed;
  uint32_t _bucketStart;   // nowEpoch at which _head's window began
  uint16_t _lastMv;        // last recorded reading; carried into skipped buckets
};

}  // namespace mishmesh
