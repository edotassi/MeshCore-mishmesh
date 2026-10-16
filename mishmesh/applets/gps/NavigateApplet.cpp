#include <mishmesh/applets/gps/NavigateApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/Geo.h>
#include <mishmesh/core/StrUtil.h>
#include <mishmesh/text/Fonts.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {

NavigateApplet::NavigateApplet() : Applet("Navigate") { _name[0] = 0; }

void NavigateApplet::setTarget(const char* name, int32_t lat1e6, int32_t lon1e6) {
  copyStr(_name, sizeof(_name), name ? name : "");
  _lat = lat1e6; _lon = lon1e6;
  navTarget().set(_name, lat1e6, lon1e6);   // keeps running after this screen closes
}

void NavigateApplet::onStart(AppletContext& ctx) { _app = ctx.app; _host = ctx.host; }

// Filled triangle, one fillRect per scanline (the panel has no polygon op).
static void fillTri(Canvas& c, float x0, float y0, float x1, float y1, float x2, float y2) {
  const float px[3] = {x0, x1, x2}, py[3] = {y0, y1, y2};
  int ymin = (int)floorf(fminf(y0, fminf(y1, y2)));
  int ymax = (int)ceilf(fmaxf(y0, fmaxf(y1, y2)));
  for (int y = ymin; y <= ymax; y++) {
    float sy = y + 0.5f, lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < 3; i++) {
      int j = (i + 1) % 3;
      float ya = py[i], yb = py[j];
      if ((sy < ya) == (sy < yb)) continue;   // edge doesn't straddle this scanline
      float x = px[i] + (sy - ya) / (yb - ya) * (px[j] - px[i]);
      if (x < lo) lo = x;
      if (x > hi) hi = x;
    }
    if (hi < lo) continue;
    int xa = (int)floorf(lo), xb = (int)ceilf(hi);
    c.fillRect(xa, y, xb - xa < 1 ? 1 : xb - xa, 1, DisplayDriver::LIGHT);
  }
}

// Arrow head ("kite" with a notched tail) of radius r, pointing `deg` clockwise from up.
static void drawArrow(Canvas& c, int cx, int cy, int r, float deg) {
  float t = deg * 3.14159265f / 180.0f;
  float dx = sinf(t), dy = -cosf(t);   // forward (screen coords)
  float rx = -dy, ry = dx;             // right-hand perpendicular
  float R = (float)r;
  float tipX = cx + dx * R,           tipY = cy + dy * R;
  float lX = cx - dx * R * 0.7f - rx * R * 0.6f, lY = cy - dy * R * 0.7f - ry * R * 0.6f;
  float rX = cx - dx * R * 0.7f + rx * R * 0.6f, rY = cy - dy * R * 0.7f + ry * R * 0.6f;
  float nX = cx - dx * R * 0.25f,     nY = cy - dy * R * 0.25f;
  fillTri(c, tipX, tipY, lX, lY, nX, nY);
  fillTri(c, tipX, tipY, nX, nY, rX, rY);
}

int NavigateApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  const Font* body = fontBody();
  int rowH = c.lineHeight(body);

  if (!_app || !_app->gpsSupported()) {
    int y0 = h / 2 - rowH;
    c.drawText(body, w / 2, y0, "GPS not available", DisplayDriver::LIGHT, TextAlign::Center);
    c.drawText(body, w / 2, y0 + rowH, "on this device", DisplayDriver::LIGHT, TextAlign::Center);
    return 1000;
  }

  if (!_app->gpsEnabled()) {
    c.drawTextCentered(body, 0, 0, w, h, "GPS is off", DisplayDriver::LIGHT);
    c.drawText(fontCaption(), w / 2, h - c.lineHeight(fontCaption()), "Select: turn on",
               DisplayDriver::LIGHT, TextAlign::Center);
    return 1000;
  }

  c.drawTextEllipsized(body, 2, 1, w - 4, _name, DisplayDriver::LIGHT);

  if (!navTarget().active()) {   // arrived (cleared by GpsAlerts) or stopped elsewhere
    c.drawTextCentered(body, 0, rowH, w, h - rowH, "Arrived", DisplayDriver::LIGHT);
    return 1000;
  }

  int32_t lat, lon;
  if (!_app->gpsHasFix() || !_app->gpsPosition1e6(lat, lon)) {
    c.drawTextCentered(body, 0, rowH, w, h - rowH, "Waiting for fix...", DisplayDriver::LIGHT);
    return 1000;
  }

  float km = geoDistanceKm(lat, lon, _lat, _lon);
  float bearing = geoBearingDeg(lat, lon, _lat, _lon);
  float kmh = _app->gpsSpeedKmh();
  bool moving = kmh >= GEO_MIN_HEADING_KMH;

  // Moving: relative to travel direction (up = straight ahead). Stationary: north up.
  float angle = moving ? geoRelativeAngle(bearing, (float)_app->gpsHeadingDeg()) : bearing;

  int midTop = rowH + 2, midBot = h - rowH - 2;
  int r = (midBot - midTop) / 2;
  if (r > w / 4) r = w / 4;
  drawArrow(c, w - r - 6, (midTop + midBot) / 2, r, angle);

  // Distance: the number in the big digit font, the unit beneath in body type
  // (fontNum has no letters).
  char dist[16];
  geoFormatDistance(dist, sizeof(dist), km);
  char* sp = strchr(dist, ' ');
  const char* unit = "";
  if (sp) { *sp = 0; unit = sp + 1; }
  int nh = c.fontHeight(fontNum());
  int y = midTop + ((midBot - midTop) - (nh + rowH)) / 2;
  c.drawText(fontNum(), 3, y, dist, DisplayDriver::LIGHT);
  c.drawText(body, 3, y + nh, unit, DisplayDriver::LIGHT);
  c.drawText(fontCaption(), 3, y + nh + rowH, "Sel: stop", DisplayDriver::LIGHT);

  char cap[24];
  if (moving) {
    char eta[16];
    geoFormatEta(eta, sizeof(eta), geoEtaMinutes(km, kmh));
    snprintf(cap, sizeof(cap), "ETA %s", eta);
  } else {
    int b = (int)(bearing + 0.5f) % 360;
    snprintf(cap, sizeof(cap), "%03d %s | N up", b, geoCompassPoint(b));
  }
  c.drawText(body, 2, h - rowH, cap, DisplayDriver::LIGHT);
  return 1000;
}

bool NavigateApplet::onInput(InputEvent ev) {
  if (ev == InputEvent::Select && _app && _app->gpsSupported()) {
    if (!_app->gpsEnabled()) { _app->setGpsEnabled(true); return true; }
    navTarget().clear();   // GPS on: Select = stop navigation
    if (_host) { _host->postToast("Navigation stopped"); _host->pop(); }
    return true;
  }
  return false;   // Back bubbles -> host pops
}

NavigateApplet& navigateApplet() {
  static NavigateApplet a;
  return a;
}

}  // namespace mishmesh
