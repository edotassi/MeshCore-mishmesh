#include <gtest/gtest.h>
#include <mishmesh/applets/gps/WaypointStore.h>
#include <mishmesh/applets/gps/WaypointsApplet.h>
#include <mishmesh/applets/gps/GpsAlerts.h>
#include <mishmesh/applets/gps/NavigateApplet.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/core/AppletHost.h>
#include "FakeDisplayDriver.h"
#include <map>
#include <string>
#include <string.h>

using namespace mishmesh;

namespace {
struct MemStorage : AppletStorage {
  std::map<std::string, std::string> blobs;
  int saves = 0;
  std::map<std::string, std::string> small;
  uint8_t load(const char* k, uint8_t* d, uint8_t cap) override {
    auto it = small.find(k);
    if (it == small.end()) return 0;
    size_t n = it->second.size() < cap ? it->second.size() : cap;
    memcpy(d, it->second.data(), n); return (uint8_t)n;
  }
  bool save(const char* k, const uint8_t* s, uint8_t len) override { small[k] = std::string((const char*)s, len); return true; }
  size_t loadBlob(const char* key, uint8_t* dst, size_t cap) override {
    auto it = blobs.find(key);
    if (it == blobs.end()) return 0;
    size_t n = it->second.size() < cap ? it->second.size() : cap;
    memcpy(dst, it->second.data(), n);
    return n;
  }
  bool saveBlob(const char* key, const uint8_t* src, size_t len) override {
    blobs[key] = std::string((const char*)src, len); saves++; return true;
  }
};

struct FakeApp : AppServices {
  bool supported = true, enabled = true, fix = true;
  int32_t lat = 0, lon = 0;
  uint32_t now = 1000;
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 0; }
  uint32_t epochSeconds() const override { return now; }
  bool gpsSupported() const override { return supported; }
  bool gpsEnabled() const override { return enabled; }
  void setGpsEnabled(bool on) override { enabled = on; }
  bool gpsHasFix() const override { return enabled && fix; }
  bool gpsPosition1e6(int32_t& la, int32_t& lo) const override { la = lat; lo = lon; return true; }
};
}  // namespace

// ---- store ------------------------------------------------------------------

TEST(WaypointStore, AddGetRenameRemove) {
  WaypointStore s; s.begin(nullptr);
  EXPECT_EQ(0, s.count());
  EXPECT_EQ(0, s.add("Home", 1000000, 2000000, 77));
  EXPECT_EQ(1, s.add("A very long waypoint name", -5, -6, 0));
  Waypoint w;
  ASSERT_TRUE(s.get(0, w));
  EXPECT_STREQ("Home", w.name); EXPECT_EQ(1000000, w.lat1e6); EXPECT_EQ(2000000, w.lon1e6); EXPECT_EQ(77u, w.created);
  ASSERT_TRUE(s.get(1, w));
  EXPECT_STREQ("A very long ", w.name);   // truncated to 12 chars
  EXPECT_TRUE(s.rename(0, "Camp"));
  s.get(0, w); EXPECT_STREQ("Camp", w.name);
  EXPECT_FALSE(s.rename(0, ""));
  EXPECT_FALSE(s.rename(5, "x"));
  EXPECT_TRUE(s.remove(0));
  EXPECT_EQ(1, s.count());
  s.get(0, w); EXPECT_EQ(-5, w.lat1e6);
  EXPECT_FALSE(s.remove(3));
  EXPECT_FALSE(s.get(-1, w));
}

TEST(WaypointStore, FullReturnsMinusOne) {
  WaypointStore s; s.begin(nullptr);
  for (int i = 0; i < WaypointStore::CAPACITY; i++) EXPECT_EQ(i, s.add("x", i, i, 0));
  EXPECT_EQ(-1, s.add("y", 0, 0, 0));
  EXPECT_EQ(WaypointStore::CAPACITY, s.count());
}

TEST(WaypointStore, DefaultNamesReuseGaps) {
  WaypointStore s; s.begin(nullptr);
  char n[8];
  s.nextDefaultName(n, sizeof(n)); EXPECT_STREQ("WP01", n);
  s.add("WP01", 0, 0, 0); s.add("WP03", 0, 0, 0);
  s.nextDefaultName(n, sizeof(n)); EXPECT_STREQ("WP02", n);
  s.add("WP02", 0, 0, 0);
  s.nextDefaultName(n, sizeof(n)); EXPECT_STREQ("WP04", n);
}

TEST(WaypointStore, PersistsRoundTrip) {
  MemStorage st;
  {
    WaypointStore s; s.begin(&st);
    s.add("Home", 45464211, 9191383, 1234);
    s.add("Peak", -33000000, -70000000, 0);
    s.rename(1, "Summit");
  }
  EXPECT_GE(st.saves, 3);
  WaypointStore r; r.begin(&st);
  ASSERT_EQ(2, r.count());
  Waypoint w;
  r.get(0, w); EXPECT_STREQ("Home", w.name); EXPECT_EQ(45464211, w.lat1e6); EXPECT_EQ(9191383, w.lon1e6); EXPECT_EQ(1234u, w.created);
  r.get(1, w); EXPECT_STREQ("Summit", w.name); EXPECT_EQ(-33000000, w.lat1e6); EXPECT_EQ(-70000000, w.lon1e6);
}

TEST(WaypointStore, DeleteIsPersisted) {
  MemStorage st;
  WaypointStore s; s.begin(&st);
  s.add("a", 1, 1, 0); s.add("b", 2, 2, 0);
  s.remove(0);
  WaypointStore r; r.begin(&st);
  ASSERT_EQ(1, r.count());
  Waypoint w; r.get(0, w); EXPECT_STREQ("b", w.name);
}

TEST(WaypointStore, CorruptBlobIsEmpty) {
  MemStorage st;
  WaypointStore s; s.begin(&st);
  s.add("a", 1, 1, 0);
  std::string good = st.blobs["waypoints"];

  std::string bad = good; bad[0] = 'X';           // magic
  st.blobs["waypoints"] = bad;
  { WaypointStore r; r.begin(&st); EXPECT_EQ(0, r.count()); }
  bad = good; bad[2] = 99;                         // version
  st.blobs["waypoints"] = bad;
  { WaypointStore r; r.begin(&st); EXPECT_EQ(0, r.count()); }
  bad = good; bad.pop_back();                      // truncated
  st.blobs["waypoints"] = bad;
  { WaypointStore r; r.begin(&st); EXPECT_EQ(0, r.count()); }
  bad = good; bad[3] = 17;                         // count over capacity
  st.blobs["waypoints"] = bad;
  { WaypointStore r; r.begin(&st); EXPECT_EQ(0, r.count()); }
  st.blobs["waypoints"] = "";
  { WaypointStore r; r.begin(&st); EXPECT_EQ(0, r.count()); }
}

// ---- applet -----------------------------------------------------------------

namespace {
struct Rig {
  MemStorage st; FakeApp app; FakeDisplayDriver d;
  AppletContext ctx; AppletHost* host;
  Rig() { ctx.app = &app; ctx.storage = &st; host = new AppletHost(&d, ctx); }
  ~Rig() { delete host; }
  void open() { host->setRoot(&waypointsApplet()); host->loop(0); }
  void press(InputEvent e) { host->dispatch(e); }
};
// 0.01 deg of latitude ~ 1.1 km.
const int32_t D = 10000;
}  // namespace

TEST(WaypointsApplet, EmptyStillShowsMarkHere) {
  Rig r; r.open();
  EXPECT_EQ(2, waypointsApplet().rowCountForTest());   // Mark here + Nearby alert
  EXPECT_STREQ("+ Mark here", waypointsApplet().rowLabelForTest(0));
}

TEST(WaypointsApplet, NearbyAlertRowCyclesAndPersists) {
  Rig r; r.app.fix = false;
  gpsAlerts().begin(&r.st);
  r.open();
  int last = waypointsApplet().rowCountForTest() - 1;
  EXPECT_STREQ("Nearby alert", waypointsApplet().rowLabelForTest(last));
  EXPECT_STREQ("Off", waypointsApplet().rowValueForTest(last));
  r.press(InputEvent::NavUp);            // wraps to the last row
  r.press(InputEvent::Select);
  EXPECT_STREQ("100 m", waypointsApplet().rowValueForTest(last));
  EXPECT_EQ(1, gpsAlerts().nearbyIndex());
  GpsAlerts reloaded; reloaded.begin(&r.st);
  EXPECT_EQ(1, reloaded.nearbyIndex());
  gpsAlerts().resetForTest();
}

TEST(WaypointsApplet, MarkHereSavesWithDefaultNameAndClock) {
  Rig r; r.app.lat = 5 * D; r.app.lon = 7; r.app.now = 4242;
  r.open();
  r.press(InputEvent::Select);
  EXPECT_STREQ("Saved WP01", r.host->toastForTest());
  Waypoint w; ASSERT_TRUE(waypointsApplet().store().get(0, w));
  EXPECT_STREQ("WP01", w.name); EXPECT_EQ(5 * D, w.lat1e6); EXPECT_EQ(7, w.lon1e6); EXPECT_EQ(4242u, w.created);
  EXPECT_EQ(3, waypointsApplet().rowCountForTest());
  r.press(InputEvent::Select);
  EXPECT_STREQ("Saved WP02", r.host->toastForTest());
}

TEST(WaypointsApplet, MarkHereWithoutFixOrGps) {
  Rig r; r.app.fix = false; r.open();
  r.press(InputEvent::Select);
  EXPECT_STREQ("No GPS fix", r.host->toastForTest());
  EXPECT_EQ(0, waypointsApplet().store().count());
  r.app.enabled = false;
  r.press(InputEvent::Select);
  EXPECT_STREQ("GPS is off", r.host->toastForTest());
  r.app.supported = false;
  r.press(InputEvent::Select);
  EXPECT_STREQ("GPS not available", r.host->toastForTest());
  EXPECT_EQ(0, waypointsApplet().store().count());
}

TEST(WaypointsApplet, MarkHereWhenFull) {
  Rig r; r.open();
  for (int i = 0; i < WaypointStore::CAPACITY; i++) r.press(InputEvent::Select);
  EXPECT_EQ(WaypointStore::CAPACITY, waypointsApplet().store().count());
  r.press(InputEvent::Select);
  EXPECT_STREQ("Waypoints full", r.host->toastForTest());
  EXPECT_EQ(WaypointStore::CAPACITY, waypointsApplet().store().count());
}

TEST(WaypointsApplet, OrderNearestFirstWithFixNewestFirstWithout) {
  Rig r;
  { WaypointStore s; s.begin(&r.st);
    s.add("far", 9 * D, 0, 1);      // oldest, farthest
    s.add("near", 1 * D, 0, 2);
    s.add("mid", 4 * D, 0, 3); }    // newest
  r.app.fix = true; r.app.lat = 0; r.app.lon = 0;
  r.open();
  ASSERT_EQ(5, waypointsApplet().rowCountForTest());
  EXPECT_STREQ("near", waypointsApplet().rowLabelForTest(1));
  EXPECT_STREQ("mid",  waypointsApplet().rowLabelForTest(2));
  EXPECT_STREQ("far",  waypointsApplet().rowLabelForTest(3));
  EXPECT_STREQ("1.1 km", waypointsApplet().rowValueForTest(1));

  Rig r2;                                         // fresh session, no fix
  { WaypointStore s; s.begin(&r2.st);
    s.add("far", 9 * D, 0, 1); s.add("near", 1 * D, 0, 2); s.add("mid", 4 * D, 0, 3); }
  r2.app.fix = false;
  r2.open();
  EXPECT_STREQ("mid",  waypointsApplet().rowLabelForTest(1));
  EXPECT_STREQ("near", waypointsApplet().rowLabelForTest(2));
  EXPECT_STREQ("far",  waypointsApplet().rowLabelForTest(3));
  EXPECT_EQ(nullptr, waypointsApplet().rowValueForTest(1));
}

namespace {
struct DummyRoot : Applet {
  DummyRoot() : Applet("root") {}
  int onRender(Canvas&) override { return 1000; }
};
}  // namespace

TEST(WaypointsApplet, NavigatePushesNavigateWithTarget) {
  Rig r;
  { WaypointStore s; s.begin(&r.st); s.add("Camp", 123456, 654321, 1); }
  r.app.fix = false;
  r.open();
  r.press(InputEvent::NavDown);          // -> Camp
  r.press(InputEvent::Select);           // action list
  r.press(InputEvent::Select);           // Navigate (first action)
  EXPECT_EQ((Applet*)&navigateApplet(), r.host->foreground());
  EXPECT_STREQ("Camp", navigateApplet().targetNameForTest());
  EXPECT_EQ(123456, navigateApplet().targetLatForTest());
  EXPECT_EQ(654321, navigateApplet().targetLonForTest());
}

TEST(WaypointsApplet, DeleteNeedsConfirm) {
  Rig r;
  { WaypointStore s; s.begin(&r.st); s.add("Camp", 1, 1, 1); }
  r.app.fix = false;
  r.open();
  r.press(InputEvent::NavDown);
  r.press(InputEvent::Select);           // actions
  r.press(InputEvent::NavDown);
  r.press(InputEvent::NavDown);          // Delete
  r.press(InputEvent::Select);           // opens confirm, opened on Cancel
  EXPECT_EQ(1, waypointsApplet().store().count());
  r.press(InputEvent::Select);           // Cancel
  EXPECT_EQ(1, waypointsApplet().store().count());
  r.press(InputEvent::Select);           // still in actions: Delete again
  r.press(InputEvent::NavRight);         // Confirm
  r.press(InputEvent::Select);
  EXPECT_EQ(0, waypointsApplet().store().count());
  WaypointStore reloaded; reloaded.begin(&r.st);
  EXPECT_EQ(0, reloaded.count());
}

TEST(WaypointsApplet, RenameOpensKeypad) {
  Rig r;
  { WaypointStore s; s.begin(&r.st); s.add("Camp", 1, 1, 1); }
  r.app.fix = false;
  r.open();
  r.press(InputEvent::NavDown);
  r.press(InputEvent::Select);
  r.press(InputEvent::NavDown);          // Rename
  r.press(InputEvent::Select);
  EXPECT_EQ((Applet*)&keypadApplet(), r.host->foreground());
}

TEST(WaypointsApplet, BackFromActionsReturnsToListThenBubbles) {
  Rig r;
  { WaypointStore s; s.begin(&r.st); s.add("Camp", 1, 1, 1); }
  r.app.fix = false;
  DummyRoot root; r.host->setRoot(&root);
  r.host->push(&waypointsApplet());
  r.host->loop(0);
  r.press(InputEvent::NavDown);
  r.press(InputEvent::Select);
  r.press(InputEvent::Back);
  EXPECT_EQ(2, r.host->depth());         // consumed: back to the list
  r.press(InputEvent::Back);
  EXPECT_EQ(1, r.host->depth());         // bubbled: host popped
}

TEST(WaypointsApplet, RendersAllModesWithoutCrash) {
  Rig r;
  { WaypointStore s; s.begin(&r.st); s.add("Camp", 1, 1, 1); }
  r.open();
  r.host->loop(1000);
  r.press(InputEvent::NavDown);
  r.press(InputEvent::Select);
  r.host->loop(2000);
  EXPECT_FALSE(r.d.fills.empty());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
