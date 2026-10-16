#pragma once

#include <stdint.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>

namespace mishmesh {

// Great-circle distance in km between two points given as degrees * 1e6 (the
// int32 form MeshCore stores GPS coordinates in). Pure math, host-testable.
inline float geoDistanceKm(int32_t lat1e6, int32_t lon1e6, int32_t lat2e6, int32_t lon2e6) {
  const double R = 6371.0;            // mean earth radius, km
  const double D2R = 3.14159265358979323846 / 180.0;
  double lat1 = (lat1e6 / 1e6) * D2R;
  double lat2 = (lat2e6 / 1e6) * D2R;
  double dlat = ((lat2e6 - lat1e6) / 1e6) * D2R;
  double dlon = ((lon2e6 - lon1e6) / 1e6) * D2R;
  double a = sin(dlat / 2) * sin(dlat / 2) +
             cos(lat1) * cos(lat2) * sin(dlon / 2) * sin(dlon / 2);
  return (float)(2.0 * R * asin(sqrt(a < 0 ? 0 : (a > 1 ? 1 : a))));
}

// Initial great-circle bearing from point 1 to point 2, in degrees clockwise from
// true north, [0, 360). Same 1e6 int32 inputs as geoDistanceKm. Coincident points
// give 0.
inline float geoBearingDeg(int32_t lat1e6, int32_t lon1e6, int32_t lat2e6, int32_t lon2e6) {
  const double D2R = 3.14159265358979323846 / 180.0;
  double lat1 = (lat1e6 / 1e6) * D2R;
  double lat2 = (lat2e6 / 1e6) * D2R;
  double dlon = ((lon2e6 - lon1e6) / 1e6) * D2R;
  double y = sin(dlon) * cos(lat2);
  double x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dlon);
  double deg = atan2(y, x) / D2R;
  if (deg < 0) deg += 360.0;
  if (deg >= 360.0) deg -= 360.0;
  return (float)deg;
}

// 8-point compass abbreviation for a heading in degrees; any int, wraps.
inline const char* geoCompassPoint(int deg) {
  static const char* const PTS[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int d = ((deg % 360) + 360) % 360;
  return PTS[((d + 22) / 45) % 8];
}

// GPS course over ground is noise below walking pace; treat it as unknown there.
static const float GEO_MIN_HEADING_KMH = 3.0f;

// Short human distance: "850 m" (10 m steps) under 1 km, "1.2 km" under 10 km,
// "12 km" beyond. Negative input is treated as 0.
inline void geoFormatDistance(char* out, size_t cap, float km) {
  if (cap == 0) return;
  if (!(km > 0)) km = 0;
  int m10 = (int)(km * 100.0f + 0.5f);   // tens of metres
  if (m10 < 100) {
    snprintf(out, cap, "%d m", m10 * 10);
  } else if (km < 9.95f) {
    snprintf(out, cap, "%.1f km", (double)km);
  } else {
    snprintf(out, cap, "%d km", (int)(km + 0.5f));
  }
}

// Angle to draw a "go this way" arrow at, clockwise from straight up the screen:
// the target bearing relative to the direction of travel (bearing - heading).
// Normalised to [0, 360) for any finite inputs.
inline float geoRelativeAngle(float bearingDeg, float headingDeg) {
  float a = fmodf(bearingDeg - headingDeg, 360.0f);
  if (a < 0) a += 360.0f;
  if (a >= 360.0f) a = 0.0f;   // fmodf of a tiny negative can round up to 360
  return a;
}

// Minutes to cover `km` at `kmh`, rounded to nearest, at least 1 for any remaining
// distance. -1 when speed is below the walking-pace floor (ETA would be nonsense).
inline int geoEtaMinutes(float km, float kmh) {
  if (!(kmh >= GEO_MIN_HEADING_KMH)) return -1;
  if (!(km > 0)) return 0;
  int m = (int)(km / kmh * 60.0f + 0.5f);
  return m < 1 ? 1 : m;
}

// "12 min" under an hour, "1:05 h" up to 99 h, ">99 h" beyond; "--" for unknown (<0).
inline void geoFormatEta(char* out, size_t cap, int minutes) {
  if (cap == 0) return;
  if (minutes < 0)            snprintf(out, cap, "--");
  else if (minutes < 60)      snprintf(out, cap, "%d min", minutes);
  else if (minutes < 99 * 60) snprintf(out, cap, "%d:%02d h", minutes / 60, minutes % 60);
  else                        snprintf(out, cap, ">99 h");
}

}  // namespace mishmesh
