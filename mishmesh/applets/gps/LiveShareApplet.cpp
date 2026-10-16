#include <mishmesh/applets/gps/LiveShareApplet.h>
#include <mishmesh/applets/gps/LiveShare.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

enum { ROW_TARGET, ROW_DURATION, ROW_INTERVAL, ROW_START };

LiveShareApplet::LiveShareApplet() : Applet("Share loc") {}

const char* LiveShareApplet::Rows::label(int i) const {
  switch (i) {
    case ROW_TARGET:   return "Target";
    case ROW_DURATION: return "Duration";
    case ROW_INTERVAL: return "Interval";
    default:           return liveShare().active() ? "Stop sharing" : "Start sharing";
  }
}

const char* LiveShareApplet::Rows::value(int i) const {
  LiveShare& s = liveShare();
  switch (i) {
    case ROW_TARGET:   return s.hasTarget() ? s.targetName() : "None";
    case ROW_DURATION: return LiveShare::durationLabel(s.durationIdx());
    case ROW_INTERVAL: return LiveShare::intervalLabel(s.intervalIdx());
    default:           return nullptr;
  }
}

const char* LiveShareApplet::Convos::label(int i) const {
  static ConvoView v;
  return (svc && svc->getConvo(i, v)) ? v.name : "";
}
uint16_t LiveShareApplet::Convos::icon(int i) const {
  ConvoView v;
  return (svc && svc->getConvo(i, v)) ? (uint16_t)(v.isChannel ? Icon::Users : Icon::User) : 0;
}

void LiveShareApplet::onStart(AppletContext& ctx) {
  _app = ctx.app; _host = ctx.host; _svc = ctx.messages;
  _convos.svc = _svc;
  _picking = false;
  _list.setRowHeight(12);
  _list.setModel(&_rows);
  _list.resetSelection();
  _pick.setRowHeight(14);
  _pick.setModel(&_convos);
  _pick.setEmptyText("No conversations");
  _pick.resetSelection();
}

int LiveShareApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  _nowMs = c.now();
  if (_picking) {
    const Font* body = fontBody();
    c.drawText(body, 2, 1, "Share to...", DisplayDriver::LIGHT);
    int y = c.lineHeight(body) + 2;
    _pick.draw(c, 0, y, w, h - y);
    return _pick.needsAnimation() ? ListMenu::TICK_MS : 1000;
  }
  const Font* body = fontBody();
  LiveShare& s = liveShare();
  char line[40];
  if (s.active()) {
    uint32_t r = s.remainingSecs(_nowMs);
    snprintf(line, sizeof(line), "Sharing %uh%02um left", (unsigned)(r / 3600), (unsigned)((r % 3600) / 60));
  } else {
    snprintf(line, sizeof(line), "Off");
  }
  c.drawTextEllipsized(body, 2, 1, w - 4, line, DisplayDriver::LIGHT);
  int y = c.lineHeight(body) + 2;
  _list.draw(c, 0, y, w, h - y);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 1000;
}

bool LiveShareApplet::onInput(InputEvent ev) {
  LiveShare& s = liveShare();
  if (_picking) {
    if (ev == InputEvent::Back) { _picking = false; return true; }
    if (_pick.onInput(ev)) return true;
    if (ev == InputEvent::Select && _svc) {
      ConvoView v;
      if (_svc->getConvo(_pick.selected(), v)) s.setTarget(v.key, v.name);
      _picking = false;
    }
    return true;
  }
  if (_list.onInput(ev)) return true;
  if (ev != InputEvent::Select) return false;   // Back bubbles

  int row = _list.selected();
  if (row == ROW_START) {
    if (s.active()) { s.stop(); if (_host) _host->postToast("Sharing stopped"); return true; }
    if (!s.hasTarget()) { if (_host) _host->postToast("Pick a target"); return true; }
    if (!_app || !_app->gpsSupported()) { if (_host) _host->postToast("GPS not available"); return true; }
    if (!_app->gpsEnabled()) _app->setGpsEnabled(true);
    s.start(_nowMs);
    if (_host) _host->postToast("Sharing started");
    return true;
  }
  if (s.active()) { if (_host) _host->postToast("Stop sharing first"); return true; }
  if (row == ROW_TARGET) { _picking = true; _pick.resetSelection(); }
  else if (row == ROW_DURATION) s.cycleDuration();
  else if (row == ROW_INTERVAL) s.cycleInterval();
  return true;
}

LiveShareApplet& liveShareApplet() {
  static LiveShareApplet a;
  return a;
}

MISHMESH_REGISTER_APPLET_ICON(&liveShareApplet(), Placement::UtilityMenu, "Share loc", 6,
                              (uint16_t)Icon::Gps);

}  // namespace mishmesh
