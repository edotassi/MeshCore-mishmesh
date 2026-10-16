#pragma once

#include <stdint.h>
#include <stdio.h>
#include <mishmesh/core/ContactsService.h>   // ContactKind / ContactView
#include <mishmesh/text/Fonts.h>             // Icon
#include <mishmesh/core/Applet.h>               // MISHMESH_GPS_NAV
#if MISHMESH_GPS_NAV
#include <mishmesh/core/Geo.h>
#endif

namespace mishmesh {

// Human label for an ADV_TYPE_* / ContactKind value.
inline const char* contactTypeName(uint8_t t) {
  switch (t) {
    case (uint8_t)ContactKind::Chat:     return "User";
    case (uint8_t)ContactKind::Repeater: return "Repeater";
    case (uint8_t)ContactKind::Room:     return "Room";
    case (uint8_t)ContactKind::Sensor:   return "Sensor";
    default:                             return "Contact";
  }
}

// Per-row type glyph (User/Radio/Comment/Chip).
inline uint16_t kindIcon(ContactKind k) {
  switch (k) {
    case ContactKind::Chat:     return (uint16_t)Icon::User;
    case ContactKind::Repeater: return (uint16_t)Icon::Radio;
    case ContactKind::Room:     return (uint16_t)Icon::Comment;
    default:                    return (uint16_t)Icon::Chip;
  }
}

// Row label: repeaters get a 2-hex key prefix (they often share generic names);
// everything else is just the name.
inline const char* contactLabel(const ContactView& v, char* buf, int n) {
  if (v.type == (uint8_t)ContactKind::Repeater && v.pubKey) {
    snprintf(buf, n, "%02X %s", v.pubKey[0], v.name);
    return buf;
  }
  return v.name;
}

// Compact relative age, e.g. "now", "5m", "2h", "3d".
inline void contactFormatAge(uint32_t secs, char* buf, int n) {
  if (secs < 60)         snprintf(buf, n, "%us", secs);
  else if (secs < 3600)  snprintf(buf, n, "%um", secs / 60);
  else if (secs < 86400) snprintf(buf, n, "%uh", secs / 3600);
  else                   snprintf(buf, n, "%ud", secs / 86400);
}

#if MISHMESH_GPS_NAV
// Our own advert location, fetched once per render pass (refresh) and shared by all
// rows of a list so value() doesn't hit the service per row.
struct SelfPos {
  bool valid = false;
  int32_t lat = 0, lon = 0;
  void refresh(const ContactsService* svc) { valid = svc && svc->selfLocation(lat, lon); }
  // "850 m" / "1.2 km" to the contact, or false when either end has no location.
  bool distanceText(const ContactView& v, char* buf, int n) const {
    if (!valid || !v.hasLocation) return false;
    geoFormatDistance(buf, (size_t)n, geoDistanceKm(lat, lon, v.gpsLat, v.gpsLon));
    return true;
  }
};
#endif

}  // namespace mishmesh
