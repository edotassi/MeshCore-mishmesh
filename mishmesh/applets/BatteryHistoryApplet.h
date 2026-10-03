#pragma once

#include <mishmesh/core/Applet.h>

namespace mishmesh {

class Canvas;

// Live battery voltage as a current readout plus a 24-hour rolling bar
// chart. Unlike NoiseFloorApplet, sampling/persistence happens in the
// background (UITask ticks + saves the shared BatteryHistory regardless of
// which applet is open) - this applet is a thin, stateless read of
// AppServices::batteryHistory() each render. Read-only; Back pops.
// App-menu singleton.
class BatteryHistoryApplet : public Applet {
public:
  BatteryHistoryApplet();

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;

private:
  AppServices* _app = nullptr;
};

BatteryHistoryApplet& batteryHistoryApplet();

}  // namespace mishmesh
