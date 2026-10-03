#include <gtest/gtest.h>
#include <mishmesh/applets/GpsApplet.h>
#include <mishmesh/core/Canvas.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

namespace {
struct FakeApp : AppServices {
  bool supported = true, enabled = false, fix = false;
  int  sats = 0, heading = 0;
  float speed = 0.0f, altitude = 0.0f, lat = 0.0f, lon = 0.0f;

  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 0; }
  uint32_t epochSeconds() const override { return 0; }

  bool gpsSupported() const override { return supported; }
  bool gpsEnabled() const override { return enabled; }
  void setGpsEnabled(bool on) override { enabled = on; }
  bool gpsHasFix() const override { return enabled && fix; }
  int  gpsSatellites() const override { return sats; }
  float gpsSpeedKmh() const override { return speed; }
  int   gpsHeadingDeg() const override { return heading; }
  float gpsAltitudeM() const override { return altitude; }
  float gpsLatitude()  const override { return lat; }
  float gpsLongitude() const override { return lon; }
};

int render(GpsApplet& a, AppServices* app) {
  AppletContext ctx; ctx.app = app;
  a.onStart(ctx);
  FakeDisplayDriver d(160, 80);
  Canvas c(&d, 0);
  return a.onRender(c);
}
}  // namespace

TEST(GpsApplet, UnsupportedShowsPlaceholder) {
  FakeApp app; app.supported = false;
  EXPECT_EQ(1000, render(gpsApplet(), &app));
}

TEST(GpsApplet, DisabledShowsOffPrompt) {
  FakeApp app; app.supported = true; app.enabled = false;
  EXPECT_EQ(1000, render(gpsApplet(), &app));
}

TEST(GpsApplet, EnabledNoFixShowsSearching) {
  FakeApp app; app.supported = true; app.enabled = true; app.fix = false;
  EXPECT_EQ(1000, render(gpsApplet(), &app));
}

TEST(GpsApplet, EnabledWithFixRendersWithoutCrash) {
  FakeApp app;
  app.supported = true; app.enabled = true; app.fix = true;
  app.sats = 7; app.speed = 42.3f; app.heading = 90; app.altitude = 123.0f;
  app.lat = 45.12345f; app.lon = -122.98765f;
  EXPECT_EQ(1000, render(gpsApplet(), &app));
}

// Southern/western hemisphere coordinates are negative - the %.5f formatting
// must not overflow the fixed buffer or crash.
TEST(GpsApplet, NegativeCoordinatesDoNotCrash) {
  FakeApp app;
  app.supported = true; app.enabled = true; app.fix = true;
  app.lat = -89.99999f; app.lon = -179.99999f;
  EXPECT_EQ(1000, render(gpsApplet(), &app));
}

// Regression: a negative heading (should never happen, but course wrap math
// must not crash or index out of the compass table).
TEST(GpsApplet, NegativeHeadingDoesNotCrash) {
  FakeApp app;
  app.supported = true; app.enabled = true; app.fix = true;
  app.heading = -30;
  EXPECT_EQ(1000, render(gpsApplet(), &app));
}

TEST(GpsApplet, SelectTogglesGpsWhenSupported) {
  FakeApp app; app.supported = true; app.enabled = false;
  AppletContext ctx; ctx.app = &app;
  GpsApplet& a = gpsApplet();
  a.onStart(ctx);
  EXPECT_TRUE(a.onInput(InputEvent::Select));
  EXPECT_TRUE(app.enabled);
  EXPECT_TRUE(a.onInput(InputEvent::Select));
  EXPECT_FALSE(app.enabled);
}

TEST(GpsApplet, SelectDoesNothingWhenUnsupported) {
  FakeApp app; app.supported = false;
  AppletContext ctx; ctx.app = &app;
  GpsApplet& a = gpsApplet();
  a.onStart(ctx);
  EXPECT_FALSE(a.onInput(InputEvent::Select));
}

TEST(GpsApplet, BackBubbles) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  GpsApplet& a = gpsApplet();
  a.onStart(ctx);
  EXPECT_FALSE(a.onInput(InputEvent::Back));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
