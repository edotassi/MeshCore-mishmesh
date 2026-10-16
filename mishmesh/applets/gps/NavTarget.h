#pragma once

#include <stdint.h>

namespace mishmesh {

// The active navigation target. Outlives NavigateApplet (the user can leave the
// screen, even lock the device, and keep walking); cleared on arrival or when the
// user stops navigation. "Armed" means we have been farther than the arrival
// hysteresis away since the target was set, so starting while already standing on
// the target never fires an arrival.
class NavTarget {
public:
  void set(const char* name, int32_t lat1e6, int32_t lon1e6);
  void clear() { _active = false; _armed = false; }

  bool active() const { return _active; }
  bool armed() const { return _armed; }
  void arm() { _armed = true; }
  const char* name() const { return _name; }
  int32_t lat() const { return _lat; }
  int32_t lon() const { return _lon; }

private:
  bool    _active = false, _armed = false;
  char    _name[32] = {0};
  int32_t _lat = 0, _lon = 0;
};

NavTarget& navTarget();

}  // namespace mishmesh
