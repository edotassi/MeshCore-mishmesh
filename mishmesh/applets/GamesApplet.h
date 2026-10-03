#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/widgets/IconGrid.h>

namespace mishmesh {

// Games submenu: an icon grid of every Placement::GamesMenu registration;
// Select pushes the focused one. Same shape as AppMenuApplet, scoped to one
// category so the top-level app menu doesn't get crowded as more games land.
class GamesApplet : public Applet, public ListModel {
  static const int MAX_ENTRIES = 8;
  const AppletRegistration* _entries[MAX_ENTRIES];
  int _count;
  IconGrid _grid;
  AppletHost* _host;
public:
  GamesApplet() : Applet("Games"), _count(0), _host(nullptr) {}

  void onStart(AppletContext& ctx) override;
  int onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  int count() const override { return _count; }
  const char* label(int i) const override { return _entries[i]->label; }
  uint16_t icon(int i) const override { return _entries[i]->icon; }
  const char* value(int) const override { return nullptr; }
};

GamesApplet& gamesApplet();

}  // namespace mishmesh
