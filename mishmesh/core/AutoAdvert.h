#pragma once

#include <stdint.h>

namespace mishmesh {

// How often a node re-sends its own self-advert unattended, flood-routed so
// the update actually reaches contacts who aren't a direct neighbour. This is
// the whole point of a standalone companion: a phone-paired node can be
// re-advertised on demand from the app, but one living in a pocket needs to
// keep the mesh (and anyone sharing location with it) up to date on its own.
// Options in stepper order, mirroring WakeHome.h's shape.
static const int AUTO_ADVERT_COUNT = 6;

inline uint32_t autoAdvertMinutes(int idx) {
  static const uint32_t MIN[AUTO_ADVERT_COUNT] = { 0, 15, 30, 60, 120, 360 };
  return (idx >= 0 && idx < AUTO_ADVERT_COUNT) ? MIN[idx] : 0;
}

inline const char* autoAdvertLabel(int idx) {
  static const char* const L[AUTO_ADVERT_COUNT] =
      { "Off", "15 min", "30 min", "1 hour", "2 hours", "6 hours" };
  return (idx >= 0 && idx < AUTO_ADVERT_COUNT) ? L[idx] : "Off";
}

// NodePrefs stores index+1 so a zeroed/legacy prefs byte resolves to Off
// rather than to the first real interval. Encode on write, decode on read.
inline int autoAdvertStoredToIndex(uint8_t stored) {
  if (stored == 0) return 0;   // unset/legacy -> Off
  int idx = (int)stored - 1;
  return (idx >= 0 && idx < AUTO_ADVERT_COUNT) ? idx : 0;
}

inline uint8_t autoAdvertIndexToStored(int idx) {
  if (idx < 0 || idx >= AUTO_ADVERT_COUNT) idx = 0;
  return (uint8_t)(idx + 1);
}

}  // namespace mishmesh
