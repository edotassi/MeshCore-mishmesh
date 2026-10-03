#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/widgets/IconGrid.h>

namespace mishmesh {

// Utility submenu: an icon grid of every Placement::UtilityMenu
// registration; Select pushes the focused one. Same shape as GamesApplet -
// a category submenu so the top-level app menu doesn't get crowded as more
// diagnostic/utility tools land.
class UtilityApplet : public Applet, public ListModel {
  static const int MAX_ENTRIES = 8;
  const AppletRegistration* _entries[MAX_ENTRIES];
  int _count;
  IconGrid _grid;
  AppletHost* _host;
public:
  UtilityApplet() : Applet("Utility"), _count(0), _host(nullptr) {}

  void onStart(AppletContext& ctx) override;
  int onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  int count() const override { return _count; }
  const char* label(int i) const override { return _entries[i]->label; }
  uint16_t icon(int i) const override { return _entries[i]->icon; }
  const char* value(int) const override { return nullptr; }
};

UtilityApplet& utilityApplet();

}  // namespace mishmesh
