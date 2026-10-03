#include <mishmesh/applets/GpsApplet.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

// 8-point compass abbreviation for a heading in degrees (0-359).
static const char* compassPoint(int deg) {
  static const char* const PTS[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int i = (((deg + 22) / 45) % 8 + 8) % 8;
  return PTS[i];
}

GpsApplet::GpsApplet() : Applet("GPS") {}

void GpsApplet::onStart(AppletContext& ctx) {
  _app = ctx.app;
}

int GpsApplet::onRender(Canvas& c) {
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

  // Status line: fix state + satellite count.
  char status[24];
  bool fix = _app->gpsHasFix();
  snprintf(status, sizeof(status), "%s  Sats %d", fix ? "3D fix" : "Searching...",
           _app->gpsSatellites());
  c.drawText(fontCaption(), 2, 1, status, DisplayDriver::LIGHT);

  int y = 2 + c.lineHeight(fontCaption()) + 2;

  if (!fix) {
    c.drawTextCentered(body, 0, y, w, h - y, "Waiting for a fix...", DisplayDriver::LIGHT);
    return 1000;
  }

  // Hero: speed, big - the number a moving tracker cares about most.
  float kmh = _app->gpsSpeedKmh();
  char spd[16];
  snprintf(spd, sizeof(spd), "%d", (int)(kmh + 0.5f));
  c.drawText(fontNum(), 3, y, spd, DisplayDriver::LIGHT);
  int nw = c.textWidth(fontNum(), spd);
  int nh = c.fontHeight(fontNum());
  c.drawText(body, 3 + nw + 2, y + nh - c.fontHeight(body), "km/h", DisplayDriver::LIGHT);
  y += nh + 2;

  // Heading + altitude rows, label left / value right.
  int heading = _app->gpsHeadingDeg();
  char hdg[20];
  snprintf(hdg, sizeof(hdg), "%03d %s", heading, compassPoint(heading));
  c.drawText(body, 3, y, "Heading", DisplayDriver::LIGHT);
  c.drawText(body, w - 3, y, hdg, DisplayDriver::LIGHT, TextAlign::Right);
  y += rowH;

  char alt[16];
  snprintf(alt, sizeof(alt), "%d m", (int)(_app->gpsAltitudeM() + (_app->gpsAltitudeM() < 0 ? -0.5f : 0.5f)));
  c.drawText(body, 3, y, "Altitude", DisplayDriver::LIGHT);
  c.drawText(body, w - 3, y, alt, DisplayDriver::LIGHT, TextAlign::Right);
  y += rowH;

  // Coordinates: body-tier type like the rows above it (up to ~20 chars with
  // two 5dp negatives, so ellipsize rather than overflow on a narrow panel).
  char coord[24];
  snprintf(coord, sizeof(coord), "%.5f, %.5f", (double)_app->gpsLatitude(),
           (double)_app->gpsLongitude());
  c.drawTextEllipsized(body, 3, y, w - 6, coord, DisplayDriver::LIGHT);

  return 1000;
}

bool GpsApplet::onInput(InputEvent ev) {
  if (ev == InputEvent::Select && _app && _app->gpsSupported()) {
    _app->setGpsEnabled(!_app->gpsEnabled());
    return true;
  }
  return false;   // Back bubbles -> host pops
}

GpsApplet& gpsApplet() {
  static GpsApplet a;
  return a;
}

MISHMESH_REGISTER_APPLET_ICON(&gpsApplet(), Placement::UtilityMenu, "GPS", 1,
                              (uint16_t)Icon::Gps);

}  // namespace mishmesh
