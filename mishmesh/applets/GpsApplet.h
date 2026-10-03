#pragma once

#include <mishmesh/core/Applet.h>

namespace mishmesh {

class Canvas;

// Live GPS speed / heading / altitude, for the tracker board variants.
// Gracefully degrades: "not available" if the board has no GPS chip,
// "off" (Select turns it on) if the board has one but it's disabled,
// "searching" while enabled but no fix yet. Real-time 1Hz readout; Back
// pops. App-menu singleton.
class GpsApplet : public Applet {
public:
  GpsApplet();

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

private:
  AppServices* _app = nullptr;
};

GpsApplet& gpsApplet();

}  // namespace mishmesh
