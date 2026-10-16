#pragma once

#include <stddef.h>
#include <stdint.h>

// GPS navigation features (waypoints etc). OLED builds only. Same default as Applet.h.
#ifndef MISHMESH_GPS_NAV
#define MISHMESH_GPS_NAV 0
#endif

namespace mishmesh {

// Generic small-blob persistence, keyed by a short string. The framework knows only
// "applets can persist bytes by key" - never what any applet stores. Implemented by the
// adapter (UITask) over the device filesystem; a null AppletContext::storage means
// "no persistence available" and callers must tolerate load()==0 / save() no-op.
// Keys must be short (the adapter builds a "/mm/<key>" path in a fixed buffer - keep
// keys <= ~32 chars); blobs are small (the adapter caps them, currently 128 bytes).
struct AppletStorage {
  virtual ~AppletStorage() {}
  // Reads up to `cap` bytes for `key` into `dst`; returns bytes read, 0 if absent.
  virtual uint8_t load(const char* key, uint8_t* dst, uint8_t cap) = 0;
  // Persists `len` bytes for `key`; returns false on failure.
  virtual bool save(const char* key, const uint8_t* src, uint8_t len) = 0;
#if MISHMESH_GPS_NAV
  // Large-blob variants (no 128 B cap) for data that outgrows save()/load().
  // Default: unsupported (0 / false); callers must tolerate that.
  virtual size_t loadBlob(const char* key, uint8_t* dst, size_t cap) { (void)key; (void)dst; (void)cap; return 0; }
  virtual bool saveBlob(const char* key, const uint8_t* src, size_t len) { (void)key; (void)src; (void)len; return false; }
  // Append-only growth (trail recording): append to the end (creating the blob),
  // read a slice, query the size, delete. Default: unsupported.
  virtual bool appendBlob(const char* key, const uint8_t* src, size_t len) { (void)key; (void)src; (void)len; return false; }
  virtual size_t readBlobAt(const char* key, size_t offset, uint8_t* dst, size_t cap) { (void)key; (void)offset; (void)dst; (void)cap; return 0; }
  virtual size_t blobSize(const char* key) { (void)key; return 0; }
  virtual bool removeBlob(const char* key) { (void)key; return false; }
#endif
};

}  // namespace mishmesh
