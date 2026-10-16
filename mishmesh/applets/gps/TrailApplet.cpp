#include <mishmesh/applets/gps/TrailApplet.h>
#include <mishmesh/applets/gps/TrailRecorder.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/Geo.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

TrailApplet::TrailApplet() : Applet("Trail") {}

const char* TrailApplet::Rows::label(int i) const {
  if (i == 0) return trailRecorder().recording() ? "Stop recording" : "Start recording";
  return "Clear trail";
}

void TrailApplet::onStart(AppletContext& ctx) {
  _app = ctx.app; _host = ctx.host;
  _confirming = false;
  _list.setRowHeight(12);
  _list.setModel(&_rows);
  _list.resetSelection();
}

static const char* stateName(TrailRecorder::State s) {
  switch (s) {
    case TrailRecorder::State::Recording:    return "Recording";
    case TrailRecorder::State::PausedNoFix:  return "Paused (no fix)";
    case TrailRecorder::State::PausedGpsOff: return "Paused (GPS off)";
    case TrailRecorder::State::Full:         return "Full";
    default:                                 return "Stopped";
  }
}

int TrailApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  const Font* body = fontBody();
  int rowH = c.lineHeight(body);
  TrailRecorder& t = trailRecorder();

  snprintf(_stateLine, sizeof(_stateLine), "%s",
           stateName(_app ? t.state(*_app) : TrailRecorder::State::Stopped));

  int y = 1;
  c.drawTextEllipsized(body, 2, y, w - 4, _stateLine, DisplayDriver::LIGHT); y += rowH;

  char line[32];
  snprintf(line, sizeof(line), "%u / %u pts", (unsigned)t.count(), (unsigned)TrailRecorder::CAP);
  c.drawText(body, 2, y, line, DisplayDriver::LIGHT); y += rowH;

  char dist[12];
  geoFormatDistance(dist, sizeof(dist), t.lengthKm());
  char dur[12] = "";
  if (t.firstTime() != 0 && t.lastTime() >= t.firstTime()) {
    uint32_t mins = (t.lastTime() - t.firstTime()) / 60;
    if (mins >= 60) snprintf(dur, sizeof(dur), "%uh%02um", (unsigned)(mins / 60), (unsigned)(mins % 60));
    else            snprintf(dur, sizeof(dur), "%um", (unsigned)mins);
  }
  snprintf(line, sizeof(line), dur[0] ? "%s  %s" : "%s", dist, dur);
  c.drawText(body, 2, y, line, DisplayDriver::LIGHT); y += rowH + 1;

  _list.draw(c, 0, y, w, h - y);
  if (_confirming) _confirm.draw(c, 0, 0, w, h);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 1000;
}

bool TrailApplet::onInput(InputEvent ev) {
  TrailRecorder& t = trailRecorder();
  if (_confirming) {
    if (_confirm.onInput(ev)) {
      ConfirmResult r = _confirm.result();
      if (r != ConfirmResult::None) {
        _confirming = false;
        if (r == ConfirmResult::Confirmed) {
          t.clear();
          if (_host) _host->postToast("Trail cleared");
        }
      }
    }
    return true;
  }
  if (_list.onInput(ev)) return true;
  if (ev != InputEvent::Select) return false;   // Back bubbles

  if (_list.selected() == 1) {
    _confirm.configure("Clear trail?", 0);   // opens on Cancel: destructive
    _confirming = true;
    return true;
  }
  if (t.recording()) {
    t.stop();
    if (_host) _host->postToast("Recording stopped");
    return true;
  }
  if (!_app || !_app->gpsSupported()) { if (_host) _host->postToast("GPS not available"); return true; }
  if (t.count() >= TrailRecorder::CAP) {
    if (_host) _host->postToast("Trail full - clear it first");
    return true;
  }
  if (!_app->gpsEnabled()) {
    _app->setGpsEnabled(true);
    if (_host) _host->postToast("GPS on");
  } else if (_host) {
    _host->postToast("Recording started");
  }
  t.start();
  return true;
}

TrailApplet& trailApplet() {
  static TrailApplet a;
  return a;
}

MISHMESH_REGISTER_APPLET_ICON(&trailApplet(), Placement::UtilityMenu, "Trail", 5,
                              (uint16_t)Icon::Gps);

}  // namespace mishmesh
