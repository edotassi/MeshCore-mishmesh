#include <gtest/gtest.h>
#include <mishmesh/applets/NoiseFloorApplet.h>
#include <mishmesh/core/Canvas.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

namespace {
struct FakeApp : AppServices {
  int16_t v = INT16_MIN;
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 0; }
  uint32_t epochSeconds() const override { return 0; }
  int16_t noiseFloorDbm() const override { return v; }
};
}  // namespace

// Before any real reading, the applet shows a placeholder and never samples -
// never pretend an unsupported/not-yet-read radio has a 0 dBm floor.
TEST(NoiseFloorApplet, NoDataYetShowsPlaceholder) {
  FakeApp app;   // v stays INT16_MIN
  AppletContext ctx; ctx.app = &app;
  NoiseFloorApplet& a = noiseFloorApplet();
  a.onStart(ctx);
  FakeDisplayDriver d(160, 80);
  Canvas c(&d, 0);
  EXPECT_EQ(1000, a.onRender(c));
  EXPECT_EQ(0, a.sampleCountForTest());
}

// One sample per second while foreground; calls inside the same second don't
// double-count, and the buffer caps at the 60s window.
TEST(NoiseFloorApplet, SamplesOncePerSecondUpToWindow) {
  FakeApp app; app.v = -100;
  AppletContext ctx; ctx.app = &app;
  NoiseFloorApplet& a = noiseFloorApplet();
  a.onStart(ctx);

  for (uint32_t t = 0; t < 3000; t += 100) {   // 30 renders inside 3 "seconds"
    FakeDisplayDriver d(160, 80);
    Canvas c(&d, t);
    a.onRender(c);
  }
  EXPECT_EQ(3, a.sampleCountForTest());   // one sample per elapsed second, not per render

  for (int i = 0; i < 100; i++) {         // far beyond the 60-sample window
    FakeDisplayDriver d(160, 80);
    Canvas c(&d, 3000 + (uint32_t)i * 1000);
    a.onRender(c);
  }
  EXPECT_EQ(60, a.sampleCountForTest());
}

// Regression: once primed, the applet must actually draw the chart (not just
// text) - the history bars are fillRect calls on the canvas.
TEST(NoiseFloorApplet, PrimedRenderDrawsChartBars) {
  FakeApp app; app.v = -90;
  AppletContext ctx; ctx.app = &app;
  NoiseFloorApplet& a = noiseFloorApplet();
  a.onStart(ctx);
  FakeDisplayDriver d(160, 80);
  Canvas c(&d, 0);
  a.onRender(c);
  EXPECT_FALSE(d.fills.empty());
}

// A reading returning to "unsupported" mid-session must not corrupt the
// history or crash - it's just skipped, prior samples stay put.
TEST(NoiseFloorApplet, UnsupportedReadingMidSessionIsSkipped) {
  FakeApp app; app.v = -95;
  AppletContext ctx; ctx.app = &app;
  NoiseFloorApplet& a = noiseFloorApplet();
  a.onStart(ctx);
  { FakeDisplayDriver d(160, 80); Canvas c(&d, 0); a.onRender(c); }
  EXPECT_EQ(1, a.sampleCountForTest());

  app.v = INT16_MIN;
  { FakeDisplayDriver d(160, 80); Canvas c(&d, 1000); a.onRender(c); }
  EXPECT_EQ(1, a.sampleCountForTest());   // no new sample banked
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
