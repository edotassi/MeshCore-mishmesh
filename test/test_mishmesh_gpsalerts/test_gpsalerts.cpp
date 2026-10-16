#include <gtest/gtest.h>
#include <mishmesh/applets/gps/GpsAlerts.h>
#include <mishmesh/applets/gps/NavTarget.h>
#include <mishmesh/applets/gps/NavigateApplet.h>
#include <mishmesh/core/AppletHost.h>
#include "FakeContactsService.h"
#include "FakeDisplayDriver.h"
#include <map>
#include <string>
#include <string.h>

using namespace mishmesh;

namespace {
struct FakeApp : AppServices {
  bool supported = true, enabled = true, fix = true;
  int32_t lat = 0, lon = 0;
  uint32_t now = 100000;
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 0; }
  uint32_t epochSeconds() const override { return now; }
  bool gpsSupported() const override { return supported; }
  bool gpsEnabled() const override { return enabled; }
  void setGpsEnabled(bool on) override { enabled = on; }
  bool gpsHasFix() const override { return enabled && fix; }
  bool gpsPosition1e6(int32_t& la, int32_t& lo) const override { la = lat; lo = lon; return true; }
};

struct MemStorage : AppletStorage {
  std::map<std::string, std::string> m;
  uint8_t load(const char* k, uint8_t* d, uint8_t cap) override {
    auto it = m.find(k); if (it == m.end()) return 0;
    size_t n = it->second.size() < cap ? it->second.size() : cap;
    memcpy(d, it->second.data(), n); return (uint8_t)n;
  }
  bool save(const char* k, const uint8_t* s, uint8_t len) override { m[k] = std::string((const char*)s, len); return true; }
};

// 0.001 deg of latitude ~ 111 m. Latitude offsets below are in 1e6 degrees.
const int32_t M10 = 90;   // ~10 m of latitude

struct Fix {
  FakeApp app; FakeContactsService svc; GpsAlerts alerts; GpsAlert out;
  uint32_t t = 0;
  Fix() { navTarget().clear(); }
  bool tick(uint32_t advanceMs = 1000) { t += advanceMs; return alerts.tick(t, app, svc, out); }
};

FakeContactsService::Row person(const char* name, const char* key, int32_t lat, uint32_t lastAdvert,
                                uint8_t type = (uint8_t)ContactKind::Chat, bool hasLoc = true) {
  FakeContactsService::Row r; r.name = name; memcpy(r.pubkey, key, 6);
  r.hasLocation = hasLoc; r.gpsLat = lat; r.gpsLon = 0; r.lastAdvert = lastAdvert; r.type = type;
  return r;
}
}  // namespace

// ---- arrival ----------------------------------------------------------------

TEST(GpsArrival, FiresOnceAfterArmingAndClearsTarget) {
  Fix f;
  navTarget().set("Camp", 0, 0);
  f.app.lat = 20 * M10;                      // ~200 m away: arms
  EXPECT_FALSE(f.tick());
  EXPECT_TRUE(navTarget().armed());
  f.app.lat = 5 * M10;                       // ~50 m: not yet within 30 m
  EXPECT_FALSE(f.tick());
  f.app.lat = 2 * M10;                       // ~20 m
  ASSERT_TRUE(f.tick());
  EXPECT_EQ(GpsAlert::Arrived, f.out.kind);
  EXPECT_STREQ("Arrived: Camp", f.out.text);
  EXPECT_FALSE(navTarget().active());
  EXPECT_FALSE(f.tick());                    // once only
}

TEST(GpsArrival, StartingAtTargetNeverFires) {
  Fix f;
  navTarget().set("Here", 0, 0);
  f.app.lat = 1 * M10;
  for (int i = 0; i < 5; i++) EXPECT_FALSE(f.tick());
  EXPECT_FALSE(navTarget().armed());
  EXPECT_TRUE(navTarget().active());
}

TEST(GpsArrival, HysteresisBandDoesNotArm) {
  Fix f;
  navTarget().set("Camp", 0, 0);
  f.app.lat = 5 * M10;                       // ~45-50 m: inside the 60 m band
  EXPECT_FALSE(f.tick());
  EXPECT_FALSE(navTarget().armed());
  f.app.lat = 2 * M10;
  EXPECT_FALSE(f.tick());                    // never armed -> no arrival
}

TEST(GpsArrival, NoFixNoArrival) {
  Fix f;
  navTarget().set("Camp", 0, 0);
  f.app.lat = 20 * M10; f.tick();            // arm
  f.app.lat = 0; f.app.fix = false;
  EXPECT_FALSE(f.tick());
  EXPECT_TRUE(navTarget().active());
  f.app.fix = true; f.app.enabled = false;
  EXPECT_FALSE(f.tick());
}

TEST(GpsArrival, ChecksOncePerSecond) {
  Fix f;
  navTarget().set("Camp", 0, 0);
  f.app.lat = 20 * M10;
  f.tick(0);                                 // arms
  f.app.lat = 0;
  EXPECT_FALSE(f.tick(500));                 // too soon
  EXPECT_TRUE(f.tick(600));
}

TEST(GpsArrival, NoTargetNoAlert) {
  Fix f;
  EXPECT_FALSE(f.tick());
}

TEST(GpsArrival, LongNameFitsToast) {
  Fix f;
  navTarget().set("An extremely long waypoint name", 0, 0);
  f.app.lat = 20 * M10; f.tick();
  f.app.lat = 0;
  ASSERT_TRUE(f.tick());
  EXPECT_LE(strlen(f.out.text), 27u);
}

TEST(GpsArrival, StopNavigationClearsTarget) {
  FakeApp app; app.enabled = true; app.fix = true;
  FakeDisplayDriver d; AppletContext ctx; ctx.app = &app;
  AppletHost host(&d, ctx);
  struct Root : Applet { Root() : Applet("r") {} int onRender(Canvas&) override { return 1000; } } root;
  host.setRoot(&root);
  navigateApplet().setTarget("Camp", 1, 1);
  EXPECT_TRUE(navTarget().active());
  host.push(&navigateApplet());
  host.dispatch(InputEvent::Back);           // Back leaves navigation running
  EXPECT_TRUE(navTarget().active());
  host.push(&navigateApplet());
  host.dispatch(InputEvent::Select);         // GPS on: Select stops
  EXPECT_FALSE(navTarget().active());
  EXPECT_STREQ("Navigation stopped", host.toastForTest());
  EXPECT_EQ(1, host.depth());
}

// ---- nearby -----------------------------------------------------------------

TEST(GpsNearby, OffByDefaultAndNothingFires) {
  Fix f;
  f.svc.chats.push_back(person("Ann", "ANN001", 2 * M10, f.app.now));
  EXPECT_EQ(0, f.alerts.nearbyIndex());
  EXPECT_FALSE(f.tick());
}

TEST(GpsNearby, RadiusRespected) {
  Fix f; f.alerts.setNearbyIndex(1);          // 100 m
  f.svc.chats.push_back(person("Far", "FAR001", 20 * M10, f.app.now));   // ~200 m
  EXPECT_FALSE(f.tick());
  f.svc.chats.push_back(person("Near", "NEA001", 5 * M10, f.app.now));   // ~50 m
  ASSERT_TRUE(f.tick(GpsAlerts::NEARBY_CHECK_MS));
  EXPECT_EQ(GpsAlert::Nearby, f.out.kind);
  EXPECT_STREQ("Nearby: Near 50 m", f.out.text);
}

TEST(GpsNearby, OnlyChatKind) {
  Fix f; f.alerts.setNearbyIndex(2);
  f.svc.repeaters.push_back(person("Rep", "REP001", M10, f.app.now, (uint8_t)ContactKind::Repeater));
  f.svc.rooms.push_back(person("Room", "ROM001", M10, f.app.now, (uint8_t)ContactKind::Room));
  f.svc.sensors.push_back(person("Sens", "SEN001", M10, f.app.now, (uint8_t)ContactKind::Sensor));
  EXPECT_FALSE(f.tick());
}

TEST(GpsNearby, SkipsContactsWithoutLocation) {
  Fix f; f.alerts.setNearbyIndex(2);
  f.svc.chats.push_back(person("NoLoc", "NOL001", 0, f.app.now, (uint8_t)ContactKind::Chat, false));
  EXPECT_FALSE(f.tick());
}

TEST(GpsNearby, StalePositionsSkipped) {
  Fix f; f.alerts.setNearbyIndex(2);
  f.svc.chats.push_back(person("Old", "OLD001", M10, f.app.now - 7201));
  EXPECT_FALSE(f.tick());
  f.svc.chats.push_back(person("Never", "NEV001", M10, 0));               // no advert time
  EXPECT_FALSE(f.tick(GpsAlerts::NEARBY_CHECK_MS));
  f.svc.chats.push_back(person("Fresh", "FRE001", M10, f.app.now - 7000));
  ASSERT_TRUE(f.tick(GpsAlerts::NEARBY_CHECK_MS));
  EXPECT_NE(nullptr, strstr(f.out.text, "Fresh"));
}

TEST(GpsNearby, UnknownClockSkips) {
  Fix f; f.alerts.setNearbyIndex(2); f.app.now = 0;
  f.svc.chats.push_back(person("Ann", "ANN001", M10, 5));
  EXPECT_FALSE(f.tick());
}

TEST(GpsNearby, CooldownOneHourPerContact) {
  Fix f; f.alerts.setNearbyIndex(2);
  f.svc.chats.push_back(person("Ann", "ANN001", M10, f.app.now));
  ASSERT_TRUE(f.tick());
  uint32_t first = f.t;
  EXPECT_FALSE(f.tick(GpsAlerts::NEARBY_CHECK_MS));
  f.app.now += 1800;
  EXPECT_FALSE(f.tick(1800000 - GpsAlerts::NEARBY_CHECK_MS));            // 30 min in
  f.app.now = 100000 + 3601;
  f.svc.chats[0].lastAdvert = f.app.now;                                  // still fresh
  f.t = first + GpsAlerts::COOLDOWN_MS;
  ASSERT_TRUE(f.tick());                                                  // cooldown over
}

TEST(GpsNearby, NearestOnlyPerCheckThenNextOnNextCheck) {
  Fix f; f.alerts.setNearbyIndex(2);
  f.svc.chats.push_back(person("Mid", "MID001", 30 * M10, f.app.now));
  f.svc.chats.push_back(person("Closest", "CLO001", 5 * M10, f.app.now));
  f.svc.chats.push_back(person("Other", "OTH001", 60 * M10, f.app.now));
  ASSERT_TRUE(f.tick());
  EXPECT_NE(nullptr, strstr(f.out.text, "Closest"));
  EXPECT_FALSE(f.tick(1000));                                             // not a burst
  ASSERT_TRUE(f.tick(GpsAlerts::NEARBY_CHECK_MS));
  EXPECT_NE(nullptr, strstr(f.out.text, "Mid"));
}

TEST(GpsNearby, ThirtySecondCadence) {
  Fix f; f.alerts.setNearbyIndex(2);
  EXPECT_FALSE(f.tick(0));                                                // consumes the first check
  f.svc.chats.push_back(person("Ann", "ANN001", M10, f.app.now));
  EXPECT_FALSE(f.tick(29000));
  EXPECT_TRUE(f.tick(1000));
}

TEST(GpsNearby, NoFixNothing) {
  Fix f; f.alerts.setNearbyIndex(2);
  f.svc.chats.push_back(person("Ann", "ANN001", M10, f.app.now));
  f.app.fix = false;
  EXPECT_FALSE(f.tick());
  f.app.fix = true; f.app.enabled = false;
  EXPECT_FALSE(f.tick(GpsAlerts::NEARBY_CHECK_MS));
}

TEST(GpsNearby, CooldownTableReplacesOldestWhenFull) {
  Fix f; f.alerts.setNearbyIndex(2);
  // 17 contacts alerted one after another: the table holds 16, so the first falls out.
  for (int i = 0; i < 17; i++) {
    char key[7]; snprintf(key, sizeof(key), "K%05d", i);
    f.svc.chats.clear();
    f.svc.chats.push_back(person("P", key, M10, f.app.now));
    ASSERT_TRUE(f.tick(GpsAlerts::NEARBY_CHECK_MS)) << i;
  }
  f.svc.chats.clear();
  f.svc.chats.push_back(person("P", "K00000", M10, f.app.now));           // evicted -> alerts again
  EXPECT_TRUE(f.tick(GpsAlerts::NEARBY_CHECK_MS));
  f.svc.chats.clear();
  f.svc.chats.push_back(person("P", "K00016", M10, f.app.now));           // still cooling
  EXPECT_FALSE(f.tick(GpsAlerts::NEARBY_CHECK_MS));
}

TEST(GpsNearby, SettingPersistsAndClamps) {
  MemStorage st;
  GpsAlerts a; a.begin(&st);
  EXPECT_EQ(0, a.nearbyIndex());
  a.setNearbyIndex(3);
  GpsAlerts b; b.begin(&st);
  EXPECT_EQ(3, b.nearbyIndex());
  EXPECT_STREQ("1 km", GpsAlerts::nearbyLabel(3));
  EXPECT_EQ(5000, GpsAlerts::nearbyRadiusM(4));
  a.setNearbyIndex(99);
  EXPECT_EQ(0, a.nearbyIndex());
  st.m["gps_near"] = std::string(1, (char)77);                             // corrupt value
  GpsAlerts c; c.begin(&st);
  EXPECT_EQ(0, c.nearbyIndex());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
