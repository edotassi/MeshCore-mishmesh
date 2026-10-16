#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mishmesh {

// The plain-text location message: "<pin> lat,lon" (5 decimals), "(live)" appended
// by live shares. The pin is U+1F4CD as UTF-8.
#define MISHMESH_PIN_UTF8 "\xF0\x9F\x93\x8D"

// Writes the message. Returns the length (0 if cap too small).
size_t formatLocationText(char* out, size_t cap, int32_t lat1e6, int32_t lon1e6, bool live);

// Recognises a location message. Accepts ONLY text that starts with the pin, then
// optional spaces, then "lat,lon" (optional sign, 1-7 decimals, optional space after
// the comma); trailing text is allowed. |lat|<=90, |lon|<=180. No free-text sniffing.
bool parseLocationText(const char* text, int32_t& lat1e6, int32_t& lon1e6);

}  // namespace mishmesh
