#include <mishmesh/applets/gps/WaypointsApplet.h>
#include <mishmesh/applets/gps/GpsAlerts.h>
#include <mishmesh/applets/gps/NavigateApplet.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/Geo.h>
#include <mishmesh/core/StrUtil.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {

enum { ACT_NAVIGATE, ACT_RENAME, ACT_DELETE, ACT_COUNT };
static const char* const ACTION_LABELS[ACT_COUNT] = {"Navigate", "Rename", "Delete"};
static const StaticListModel& actionModel() {
  static const StaticListModel m(ACTION_LABELS, ACT_COUNT);
  return m;
}

WaypointsApplet::WaypointsApplet() : Applet("Waypoints") { _renameBuf[0] = 0; }

const char* WaypointsApplet::Rows::label(int i) const {
  static Waypoint w;
  if (i == 0) return "+ Mark here";
  if (i == a->_orderN + 1) return "Nearby alert";
  if (i < 1 || i > a->_orderN || !a->_store.get(a->_order[i - 1], w)) return "";
  return w.name;
}

uint16_t WaypointsApplet::Rows::icon(int i) const {
  if (i == a->_orderN + 1) return (uint16_t)Icon::Bell;
  return (uint16_t)(i == 0 ? Icon::Plus : Icon::Gps);
}

const char* WaypointsApplet::Rows::value(int i) const {
  static Waypoint w;
  static char buf[12];
  if (i == a->_orderN + 1) return GpsAlerts::nearbyLabel(gpsAlerts().nearbyIndex());
  if (i < 1 || i > a->_orderN || !a->_havePos || !a->_store.get(a->_order[i - 1], w)) return nullptr;
  geoFormatDistance(buf, sizeof(buf), geoDistanceKm(a->_lat, a->_lon, w.lat1e6, w.lon1e6));
  return buf;
}

void WaypointsApplet::refreshPos() {
  _havePos = _app && _app->gpsSupported() && _app->gpsEnabled() && _app->gpsHasFix() &&
             _app->gpsPosition1e6(_lat, _lon);
}

// Nearest first with a fix, else newest first (the store appends chronologically).
void WaypointsApplet::rebuild() {
  _orderN = _store.count();
  for (int i = 0; i < _orderN; i++) _order[i] = (uint8_t)(_havePos ? i : _orderN - 1 - i);
  if (!_havePos) return;
  float d[WaypointStore::CAPACITY];
  for (int i = 0; i < _orderN; i++) {
    Waypoint w; _store.get(i, w);
    d[i] = geoDistanceKm(_lat, _lon, w.lat1e6, w.lon1e6);
  }
  for (int i = 1; i < _orderN; i++) {     // insertion sort, stable
    uint8_t k = _order[i]; int j = i;
    while (j > 0 && d[_order[j - 1]] > d[k]) { _order[j] = _order[j - 1]; j--; }
    _order[j] = k;
  }
}

void WaypointsApplet::onStart(AppletContext& ctx) {
  _app = ctx.app; _host = ctx.host;
  _store.begin(ctx.storage);
  _rows.a = this;
  _acting = false; _confirming = false; _sel = -1;
  refreshPos();
  rebuild();
  _list.setRowHeight(14);
  _list.setModel(&_rows);
  _list.resetSelection();
  _actions.setRowHeight(14);
  _actions.setModel(&actionModel());
  _actions.resetSelection();
}

void WaypointsApplet::onForeground() {   // back from the keypad / Navigate
  refreshPos();
  rebuild();
}

void WaypointsApplet::markHere() {
  if (!_host) return;
  if (!_app || !_app->gpsSupported()) { _host->postToast("GPS not available"); return; }
  if (!_app->gpsEnabled())            { _host->postToast("GPS is off"); return; }
  int32_t lat, lon;
  if (!_app->gpsHasFix() || !_app->gpsPosition1e6(lat, lon)) { _host->postToast("No GPS fix"); return; }
  char name[WaypointStore::NAME_LEN + 1];
  _store.nextDefaultName(name, sizeof(name));
  if (_store.add(name, lat, lon, _app->epochSeconds()) < 0) { _host->postToast("Waypoints full"); return; }
  char msg[24];
  snprintf(msg, sizeof(msg), "Saved %s", name);
  _host->postToast(msg);
  refreshPos();
  rebuild();
}

void WaypointsApplet::onRenameDone(void* ctx, const char* text) {
  WaypointsApplet* a = static_cast<WaypointsApplet*>(ctx);
  if (text && text[0]) a->_store.rename(a->_sel, text);   // empty -> keep the old name
}

int WaypointsApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  bool hadPos = _havePos;
  refreshPos();
  if (hadPos != _havePos && !_acting && !_confirming) rebuild();   // fix gained/lost: reorder

  if (_acting) {
    Waypoint wp;
    char title[32] = "";
    if (_store.get(_sel, wp)) copyStr(title, sizeof(title), wp.name);
    const Font* body = fontBody();
    int rowH = c.lineHeight(body);
    c.drawTextEllipsized(body, 2, 1, w - 4, title, DisplayDriver::LIGHT);
    _actions.draw(c, 0, rowH + 2, w, h - rowH - 2);
    if (_confirming) _confirm.draw(c, 0, 0, w, h);
    return (_actions.needsAnimation()) ? ListMenu::TICK_MS : 1000;
  }
  _list.draw(c, 0, 0, w, h);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 1000;
}

bool WaypointsApplet::onInput(InputEvent ev) {
  if (_confirming) {
    if (_confirm.onInput(ev)) {
      ConfirmResult r = _confirm.result();
      if (r != ConfirmResult::None) {
        _confirming = false;
        if (r == ConfirmResult::Confirmed) {
          _store.remove(_sel);
          _acting = false; _sel = -1;
          refreshPos(); rebuild();
          _list.resetSelection();
          if (_host) _host->postToast("Waypoint deleted");
        }
      }
    }
    return true;
  }

  if (_acting) {
    if (ev == InputEvent::Back) { _acting = false; return true; }
    if (_actions.onInput(ev)) return true;
    if (ev == InputEvent::Select) {
      Waypoint wp;
      if (!_store.get(_sel, wp)) { _acting = false; return true; }
      switch (_actions.selected()) {
        case ACT_NAVIGATE:
          navigateApplet().setTarget(wp.name, wp.lat1e6, wp.lon1e6);
          if (_host) _host->push(&navigateApplet());
          return true;
        case ACT_RENAME:
          copyStr(_renameBuf, sizeof(_renameBuf), wp.name);
          keypadApplet().configure(_renameBuf, WaypointStore::NAME_LEN, "Rename",
                                   &WaypointsApplet::onRenameDone, this);
          if (_host) _host->push(&keypadApplet());
          return true;
        case ACT_DELETE:
          _confirm.configure("Delete waypoint?", 0);   // open on Cancel: destructive
          _confirming = true;
          return true;
      }
      return true;
    }
    return false;
  }

  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    int s = _list.selected();
    if (s == 0) { markHere(); return true; }
    if (s == _orderN + 1) {   // cycle Off / 100 m / 500 m / 1 km / 5 km
      gpsAlerts().setNearbyIndex((gpsAlerts().nearbyIndex() + 1) % GpsAlerts::OPTIONS);
      return true;
    }
    if (s >= 1 && s <= _orderN) {
      _sel = _order[s - 1];
      _acting = true;
      _actions.resetSelection();
    }
    return true;
  }
  return false;   // Back bubbles -> host pops
}

WaypointsApplet& waypointsApplet() {
  static WaypointsApplet a;
  return a;
}

// Order 4: last in the Utility menu. GPS is 1 and Airtime/Noise Floor hold 2/3; a
// tie at 1 would sort by static-init order, and renumbering them would touch the
// e-ink build's data.
MISHMESH_REGISTER_APPLET_ICON(&waypointsApplet(), Placement::UtilityMenu, "Waypoints", 4,
                              (uint16_t)Icon::Gps);

}  // namespace mishmesh
