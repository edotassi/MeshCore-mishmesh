#include <gtest/gtest.h>
#include <mishmesh/applets/BatteryHistoryApplet.h>
#include <mishmesh/core/BatteryHistory.h>
#include <mishmesh/core/Canvas.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

namespace {
struct FakeApp : AppServices {
  BatteryHistory hist;
  bool hasHist = true;
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 0; }
  uint32_t epochSeconds() const override { return 0; }
  bool batteryHistory(BatteryStats& out) const override {
    if (!hasHist) return false;
    out.currentMv = hist.primed() ? hist.bucket(hist.bucketCount() - 1) : 0;
    out.history = &hist;
    return true;
  }
};
}  // namespace

TEST(BatteryHistoryApplet, NoDataYetShowsPlaceholder) {
  FakeApp app;   // hist never ticked
  AppletContext ctx; ctx.app = &app;
  BatteryHistoryApplet& a = batteryHistoryApplet();
  a.onStart(ctx);
  FakeDisplayDriver d(160, 80);
  Canvas c(&d, 0);
  EXPECT_EQ(1000, a.onRender(c));
}

TEST(BatteryHistoryApplet, UnavailableServiceShowsPlaceholder) {
  FakeApp app; app.hasHist = false;
  AppletContext ctx; ctx.app = &app;
  BatteryHistoryApplet& a = batteryHistoryApplet();
  a.onStart(ctx);
  FakeDisplayDriver d(160, 80);
  Canvas c(&d, 0);
  EXPECT_EQ(1000, a.onRender(c));
}

TEST(BatteryHistoryApplet, PrimedRenderDrawsChart) {
  FakeApp app;
  app.hist.tick(1000, 4100);
  app.hist.tick(1000 + BatteryHistory::BUCKET_SECS, 4050);
  app.hist.tick(1000 + 2 * BatteryHistory::BUCKET_SECS, 3900);
  AppletContext ctx; ctx.app = &app;
  BatteryHistoryApplet& a = batteryHistoryApplet();
  a.onStart(ctx);
  FakeDisplayDriver d(160, 80);
  Canvas c(&d, 0);
  a.onRender(c);
  EXPECT_FALSE(d.fills.empty());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
