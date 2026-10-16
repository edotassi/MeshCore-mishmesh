#include <mishmesh/applets/gps/LocationText.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace mishmesh {

size_t formatLocationText(char* out, size_t cap, int32_t lat1e6, int32_t lon1e6, bool live) {
  if (!out || cap == 0) return 0;
  // Integer formatting: exact, no float rounding, 5 decimals (round half away from 0).
  auto fmt = [](char* b, size_t n, int32_t v1e6) {
    int64_t a = v1e6 < 0 ? -(int64_t)v1e6 : v1e6;
    a = (a + 5) / 10;                         // to 1e5, rounded
    snprintf(b, n, "%s%lld.%05lld", v1e6 < 0 && a != 0 ? "-" : "",
             (long long)(a / 100000), (long long)(a % 100000));
  };
  char la[16], lo[16];
  fmt(la, sizeof(la), lat1e6);
  fmt(lo, sizeof(lo), lon1e6);
  int n = snprintf(out, cap, MISHMESH_PIN_UTF8 " %s,%s%s", la, lo, live ? " (live)" : "");
  if (n < 0 || (size_t)n >= cap) { out[0] = 0; return 0; }
  return (size_t)n;
}

// Parses [sign]digits[.1-7 digits] into 1e6 units; advances p. False if malformed.
static bool parseCoord(const char*& p, int64_t& out1e6) {
  bool neg = false;
  if (*p == '-') { neg = true; p++; } else if (*p == '+') { p++; }
  if (*p < '0' || *p > '9') return false;
  int64_t ip = 0; int digits = 0;
  while (*p >= '0' && *p <= '9') { ip = ip * 10 + (*p - '0'); p++; if (++digits > 3) return false; }
  int64_t frac = 0; int fd = 0;
  if (*p == '.') {
    p++;
    if (*p < '0' || *p > '9') return false;
    while (*p >= '0' && *p <= '9') {
      if (fd >= 7) return false;
      if (fd < 6) frac = frac * 10 + (*p - '0');   // 7th digit is accepted but below 1e-6
      fd++; p++;
    }
    for (int i = fd; i < 6; i++) frac *= 10;
  }
  int64_t v = ip * 1000000 + frac;
  out1e6 = neg ? -v : v;
  return true;
}

bool parseLocationText(const char* text, int32_t& lat1e6, int32_t& lon1e6) {
  if (!text) return false;
  const size_t pl = strlen(MISHMESH_PIN_UTF8);
  if (strncmp(text, MISHMESH_PIN_UTF8, pl) != 0) return false;
  const char* p = text + pl;
  while (*p == ' ') p++;
  int64_t la, lo;
  if (!parseCoord(p, la)) return false;
  if (*p != ',') return false;
  p++;
  if (*p == ' ') p++;
  if (!parseCoord(p, lo)) return false;
  // The number must end here: whitespace, "(" or end of text, not more digits/letters.
  if (*p && *p != ' ' && *p != '(' && *p != '\n') return false;
  if (la > 90000000 || la < -90000000 || lo > 180000000 || lo < -180000000) return false;
  lat1e6 = (int32_t)la; lon1e6 = (int32_t)lo;
  return true;
}

}  // namespace mishmesh
