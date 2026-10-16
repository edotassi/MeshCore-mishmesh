#include <gtest/gtest.h>
#include <mishmesh/applets/gps/NavigateApplet.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/Geo.h>
#include "FakeDisplayDriver.h"
#include <string.h>

using namespace mishmesh;

namespace {
struct FakeApp : AppServices {
  bool supported = true, enabled = false, fix = false, posOk = true;
  int32_t lat = 0, lon = 0;
  int  heading = 0;
  float speed = 0.0f;

  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 0; }
  uint32_t epochSeconds() const override { return 0; }

  bool gpsSupported() const override { return supported; }
  bool gpsEnabled() const override { return enabled; }
  void setGpsEnabled(bool on) override { enabled = on; }
  bool gpsHasFix() const override { return enabled && fix; }
  float gpsSpeedKmh() const override { return speed; }
  int   gpsHeadingDeg() const override { return heading; }
  bool gpsPosition1e6(int32_t& la, int32_t& lo) const override { la = lat; lo = lon; return posOk; }
};

// Target 0.01 deg (~1.1 km) north / south of the origin (we are at 0,0).
const int32_t NORTH = 10000, SOUTH = -10000;

struct Frame {
  int ret;
  int arrowTop;    // topmost drawn row inside the arrow band (right half), -1 if none
  size_t fills;
};

Frame render(FakeApp& app, int32_t targetLat, int32_t targetLon = 0) {
  NavigateApplet& a = navigateApplet();
  a.setTarget("Alice", targetLat, targetLon);
  AppletContext ctx; ctx.app = &app;
  a.onStart(ctx);
  FakeDisplayDriver d(128, 64);
  Canvas c(&d, 0);
  Frame f;
  f.ret = a.onRender(c);
  f.fills = d.fills.size();
  f.arrowTop = -1;
  for (auto& r : d.fills) {
    // Arrow band: right half, between the name row and the caption row.
    if (r.x + r.w > 64 && r.x >= 64 - 24 && r.y >= 12 && r.y < 52 && r.h == 1)
      if (f.arrowTop < 0 || r.y < f.arrowTop) f.arrowTop = r.y;
  }
  return f;
}
}  // namespace

// ---- pure math --------------------------------------------------------------

TEST(NavMath, RelativeAngleNormalised) {
  EXPECT_NEAR(0.0f,   geoRelativeAngle(90, 90), 1e-4f);
  EXPECT_NEAR(90.0f,  geoRelativeAngle(90, 0), 1e-4f);
  EXPECT_NEAR(270.0f, geoRelativeAngle(0, 90), 1e-4f);    // target left of heading
  EXPECT_NEAR(180.0f, geoRelativeAngle(0, 180), 1e-4f);
  EXPECT_NEAR(350.0f, geoRelativeAngle(10, 20), 1e-4f);   // wraps below zero
  EXPECT_NEAR(10.0f,  geoRelativeAngle(5, 355), 1e-4f);   // wraps across north
  float tiny = geoRelativeAngle(0.0f, 1e-9f);
  EXPECT_GE(tiny, 0.0f);
  EXPECT_LT(tiny, 360.0f);
  EXPECT_LT(geoRelativeAngle(-720.5f, 3.0f), 360.0f);
  EXPECT_GE(geoRelativeAngle(-720.5f, 3.0f), 0.0f);
}

TEST(NavMath, EtaMinutes) {
  EXPECT_EQ(30, geoEtaMinutes(5.0f, 10.0f));
  EXPECT_EQ(60, geoEtaMinutes(10.0f, 10.0f));
  EXPECT_EQ(1,  geoEtaMinutes(0.01f, 50.0f));    // rounds up to at least a minute
  EXPECT_EQ(0,  geoEtaMinutes(0.0f, 50.0f));     // arrived
  EXPECT_EQ(2,  geoEtaMinutes(0.6f, 20.0f));     // 1.8 -> 2
}

TEST(NavMath, EtaUnknownBelowWalkingPace) {
  EXPECT_EQ(-1, geoEtaMinutes(5.0f, 0.0f));
  EXPECT_EQ(-1, geoEtaMinutes(5.0f, GEO_MIN_HEADING_KMH - 0.1f));
  EXPECT_EQ(-1, geoEtaMinutes(5.0f, -4.0f));
  EXPECT_NE(-1, geoEtaMinutes(5.0f, GEO_MIN_HEADING_KMH));
}

TEST(NavMath, FormatEta) {
  char b[16];
  geoFormatEta(b, sizeof(b), 5);    EXPECT_STREQ("5 min", b);
  geoFormatEta(b, sizeof(b), 59);   EXPECT_STREQ("59 min", b);
  geoFormatEta(b, sizeof(b), 65);   EXPECT_STREQ("1:05 h", b);
  geoFormatEta(b, sizeof(b), 600000); EXPECT_STREQ(">99 h", b);
  geoFormatEta(b, sizeof(b), -1);   EXPECT_STREQ("--", b);
}

// ---- render states ----------------------------------------------------------

TEST(NavigateApplet, UnsupportedShowsPlaceholder) {
  FakeApp app; app.supported = false;
  EXPECT_EQ(1000, render(app, NORTH).ret);
}

TEST(NavigateApplet, OffShowsPromptAndSelectTurnsOn) {
  FakeApp app; app.enabled = false;
  EXPECT_EQ(1000, render(app, NORTH).ret);
  EXPECT_TRUE(navigateApplet().onInput(InputEvent::Select));
  EXPECT_TRUE(app.enabled);
}

TEST(NavigateApplet, SelectUnsupportedIgnored) {
  FakeApp app; app.supported = false;
  render(app, NORTH);
  EXPECT_FALSE(navigateApplet().onInput(InputEvent::Select));
}

TEST(NavigateApplet, NoFixDrawsLessThanFullView) {
  FakeApp app; app.enabled = true; app.fix = true;
  size_t full = render(app, NORTH).fills;
  app.fix = false;
  Frame f = render(app, NORTH);
  EXPECT_EQ(1000, f.ret);
  EXPECT_LT(f.fills, full);          // just the name + "Waiting for fix"
  // A fix with unavailable coordinates is also "no fix".
  app.fix = true; app.posOk = false;
  EXPECT_LT(render(app, NORTH).fills, full);
}

TEST(NavigateApplet, FixDrawsArrowAndRefreshesEverySecond) {
  FakeApp app; app.enabled = true; app.fix = true;
  Frame f = render(app, NORTH);
  EXPECT_EQ(1000, f.ret);
  EXPECT_GE(f.arrowTop, 0);
}

TEST(NavigateApplet, StationaryArrowIsAbsoluteBearing) {
  FakeApp app; app.enabled = true; app.fix = true; app.speed = 0.0f; app.heading = 180;
  // Heading is ignored when stationary: north target points up, south target down.
  Frame north = render(app, NORTH);
  Frame south = render(app, SOUTH);
  ASSERT_GE(north.arrowTop, 0);
  ASSERT_GE(south.arrowTop, 0);
  EXPECT_LT(north.arrowTop, south.arrowTop);
}

TEST(NavigateApplet, MovingArrowIsRelativeToHeading) {
  FakeApp app; app.enabled = true; app.fix = true; app.speed = 20.0f;
  app.heading = 0;                      // heading north, target north: straight ahead (up)
  Frame ahead = render(app, NORTH);
  app.heading = 180;                    // heading south, target north: behind us (down)
  Frame behind = render(app, NORTH);
  ASSERT_GE(ahead.arrowTop, 0);
  ASSERT_GE(behind.arrowTop, 0);
  EXPECT_LT(ahead.arrowTop, behind.arrowTop);

  // Same pixels as the stationary south arrow: relative 180 == absolute south.
  app.speed = 0.0f; app.heading = 0;
  EXPECT_EQ(behind.arrowTop, render(app, SOUTH).arrowTop);
}

TEST(NavigateApplet, UsesLivePositionNotTargetAtOrigin) {
  // Standing on the target: distance 0, must still render (no fake-fix confusion).
  FakeApp app; app.enabled = true; app.fix = true; app.lat = NORTH;
  EXPECT_EQ(1000, render(app, NORTH).ret);
}

TEST(NavigateApplet, BackBubbles) {
  FakeApp app; app.enabled = true; app.fix = true;
  render(app, NORTH);
  EXPECT_FALSE(navigateApplet().onInput(InputEvent::Back));
}

TEST(NavigateApplet, LongNameDoesNotCrash) {
  FakeApp app; app.enabled = true; app.fix = true;
  NavigateApplet& a = navigateApplet();
  a.setTarget("A very very long contact name that must be ellipsized", NORTH, 0);
  AppletContext ctx; ctx.app = &app; a.onStart(ctx);
  FakeDisplayDriver d(128, 64); Canvas c(&d, 0);
  EXPECT_EQ(1000, a.onRender(c));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
