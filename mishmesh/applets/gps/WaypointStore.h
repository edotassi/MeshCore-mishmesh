#pragma once

#include <stddef.h>
#include <stdint.h>
#include <mishmesh/core/AppletStorage.h>

namespace mishmesh {

struct Waypoint {
  char     name[13];     // 12 chars + NUL
  int32_t  lat1e6;
  int32_t  lon1e6;
  uint32_t created;      // epoch seconds, 0 if the clock was unknown
};

// Small persistent list of saved positions. Every mutation is written straight
// through AppletStorage::saveBlob under the key "waypoints". Without storage (or
// with a storage that cannot saveBlob) it still works in RAM for the session.
class WaypointStore {
public:
  enum { CAPACITY = 16 };
  enum { NAME_LEN = 12 };

  // Loads the persisted list (a missing, short or foreign blob = empty).
  void begin(AppletStorage* storage);

  int  count() const { return _n; }
  bool get(int i, Waypoint& out) const;
  // Appends; returns the new index, or -1 when full. Name is truncated to NAME_LEN.
  int  add(const char* name, int32_t lat1e6, int32_t lon1e6, uint32_t created);
  bool rename(int i, const char* name);
  bool remove(int i);
  // "WP01".."WP99": the lowest number no waypoint uses (falls back to "WP99").
  void nextDefaultName(char* buf, size_t cap) const;

private:
  void save();

  AppletStorage* _st = nullptr;
  Waypoint       _wp[CAPACITY];
  int            _n = 0;
};

}  // namespace mishmesh
