#include <mishmesh/core/BatteryHistory.h>
#include <mishmesh/core/AppletStorage.h>
#include <string.h>

namespace mishmesh {

void BatteryHistory::reset() {
  memset(_mv, 0, sizeof(_mv));
  _head = 0;
  _primed = false;
  _bucketStart = 0;
  _lastMv = 0;
}

void BatteryHistory::tick(uint32_t nowEpoch, uint16_t mv) {
  if (nowEpoch == 0) return;   // clock not synced yet

  if (!_primed) {
    _primed = true;
    _bucketStart = nowEpoch;
    _head = 0;
  } else if (nowEpoch >= _bucketStart + BUCKET_SECS) {
    uint32_t elapsed = nowEpoch - _bucketStart;
    int steps = (int)(elapsed / BUCKET_SECS);
    if (steps > BUCKETS) steps = BUCKETS;   // a longer gap just floods the whole ring
    for (int i = 0; i < steps; i++) {
      _head = (uint8_t)((_head + 1) % BUCKETS);
      _mv[_head] = _lastMv;   // carry forward through the gap
    }
    _bucketStart += (uint32_t)steps * BUCKET_SECS;
  }
  // A clock that jumped backward just overwrites the current bucket below -
  // no special case needed.

  _mv[_head] = mv;
  _lastMv = mv;
}

uint16_t BatteryHistory::bucket(int i) const {
  return _mv[(_head + 1 + i) % BUCKETS];
}

uint16_t BatteryHistory::minMv() const {
  uint16_t m = 0;
  for (int i = 0; i < BUCKETS; i++)
    if (_mv[i] != 0 && (m == 0 || _mv[i] < m)) m = _mv[i];
  return m;
}

uint16_t BatteryHistory::maxMv() const {
  uint16_t m = 0;
  for (int i = 0; i < BUCKETS; i++)
    if (_mv[i] > m) m = _mv[i];
  return m;
}

bool BatteryHistory::save(AppletStorage* storage, const char* key) const {
  if (!storage) return false;
  uint8_t buf[BLOB_BYTES];
  memcpy(buf + 0, &_bucketStart, 4);
  buf[4] = _head;
  buf[5] = _primed ? 1 : 0;
  memcpy(buf + 6, &_lastMv, 2);
  memcpy(buf + 8, _mv, sizeof(_mv));
  return storage->save(key, buf, sizeof(buf));
}

bool BatteryHistory::load(AppletStorage* storage, const char* key) {
  if (!storage) return false;
  uint8_t buf[BLOB_BYTES];
  if (storage->load(key, buf, sizeof(buf)) != sizeof(buf)) return false;   // absent/short/old layout
  uint8_t head = buf[4];
  if (head >= BUCKETS) return false;   // corrupt

  memcpy(&_bucketStart, buf + 0, 4);
  _head = head;
  _primed = buf[5] != 0;
  memcpy(&_lastMv, buf + 6, 2);
  memcpy(_mv, buf + 8, sizeof(_mv));
  return true;
}

}  // namespace mishmesh
