#pragma once

#include <mishmesh/core/Applet.h>

namespace mishmesh {

class Canvas;

// Live LoRa radio noise floor (ambient channel noise, dBm) as a current
// readout plus a 60-second rolling bar chart. Raw, unsmoothed samples taken
// once per second while this screen is foreground - the history resets each
// time the applet is opened (no background sampling). Read-only; Back pops.
// App-menu singleton.
class NoiseFloorApplet : public Applet {
public:
  NoiseFloorApplet();

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;

  int sampleCountForTest() const { return _count; }

private:
  static const int SAMPLES = 60;   // 60s rolling window at 1 sample/sec
  static const uint32_t SAMPLE_INTERVAL_MS = 1000;

  void sample(uint32_t now);

  AppServices* _app = nullptr;
  int16_t  _samples[SAMPLES];
  int      _count = 0;          // samples currently held (<= SAMPLES)
  int      _head = 0;           // ring index of the next slot to write
  uint32_t _lastSampleAt = 0;
  bool     _primed = false;     // at least one real reading seen
};

NoiseFloorApplet& noiseFloorApplet();

}  // namespace mishmesh
