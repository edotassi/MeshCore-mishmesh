#include <mishmesh/applets/GamesApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

void GamesApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _count = 0;
  for (const AppletRegistration* r = registeredApplets(); r; r = r->next) {
    if (r->placement != Placement::GamesMenu || _count >= MAX_ENTRIES) continue;
    int i = _count++;
    while (i > 0 && _entries[i - 1]->order > r->order) {
      _entries[i] = _entries[i - 1];
      i--;
    }
    _entries[i] = r;
  }
  _grid.setModel(this);
  _grid.setRowHeight(20);
}

int GamesApplet::onRender(Canvas& c) {
  _grid.draw(c, 0, 0, c.width(), c.height());
  return _grid.needsAnimation() ? IconGrid::TICK_MS : 1000;
}

bool GamesApplet::onInput(InputEvent ev) {
  if (_grid.onInput(ev)) return true;
  if (ev == InputEvent::Select && _count > 0 && _host) {
    _host->push(_entries[_grid.selected()]->applet);
    return true;
  }
  return false;   // Back bubbles -> host pops
}

GamesApplet& gamesApplet() {
  static GamesApplet a;
  return a;
}

MISHMESH_REGISTER_APPLET_ICON(&gamesApplet(), Placement::AppMenu, "Games", 8,
                              (uint16_t)Icon::Grid);

}  // namespace mishmesh
