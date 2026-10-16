#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/applets/gps/NavTarget.h>

namespace mishmesh {

class Canvas;

// Point-to-node navigation: distance, an arrow towards a fixed target and an ETA
// (moving) or compass bearing (stationary). Launch-only: callers set the target,
// then push it. Degrades like GpsApplet (unsupported / off / waiting for a fix).
// Our position is the live fix (AppServices::gpsPosition1e6), never the static
// advert location. Back bubbles so the host pops but leaves navigation active
// (navTarget(), which GpsAlerts watches for arrival); Select stops it.
class NavigateApplet : public Applet {
public:
  NavigateApplet();

  void setTarget(const char* name, int32_t lat1e6, int32_t lon1e6);
  // Test seams
  const char* targetNameForTest() const { return _name; }
  int32_t targetLatForTest() const { return _lat; }
  int32_t targetLonForTest() const { return _lon; }

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

private:
  AppServices* _app = nullptr;
  AppletHost*  _host = nullptr;
  char    _name[32];
  int32_t _lat = 0, _lon = 0;
};

NavigateApplet& navigateApplet();

}  // namespace mishmesh
