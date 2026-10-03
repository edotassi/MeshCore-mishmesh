#include <mishmesh/applets/UtilityApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

void UtilityApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _count = 0;
  for (const AppletRegistration* r = registeredApplets(); r; r = r->next) {
    if (r->placement != Placement::UtilityMenu || _count >= MAX_ENTRIES) continue;
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

int UtilityApplet::onRender(Canvas& c) {
  _grid.draw(c, 0, 0, c.width(), c.height());
  return _grid.needsAnimation() ? IconGrid::TICK_MS : 1000;
}

bool UtilityApplet::onInput(InputEvent ev) {
  if (_grid.onInput(ev)) return true;
  if (ev == InputEvent::Select && _count > 0 && _host) {
    _host->push(_entries[_grid.selected()]->applet);
    return true;
  }
  return false;   // Back bubbles -> host pops
}

UtilityApplet& utilityApplet() {
  static UtilityApplet a;
  return a;
}

MISHMESH_REGISTER_APPLET_ICON(&utilityApplet(), Placement::AppMenu, "Utility", 2,
                              (uint16_t)Icon::Chip);

}  // namespace mishmesh
