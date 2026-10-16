#include <gtest/gtest.h>
#include <mishmesh/applets/gps/TrailApplet.h>
#include <mishmesh/applets/gps/TrailRecorder.h>
#include <mishmesh/core/AppletHost.h>
#include "FakeDisplayDriver.h"
#include <map>
#include <string>
#include <string.h>

using namespace mishmesh;

namespace {
struct MemStorage : AppletStorage {
  std::map<std::string, std::string> small, blobs;
  int appends = 0;
  uint8_t load(const char* k, uint8_t* d, uint8_t cap) override {
    auto it = small.find(k); if (it == small.end()) return 0;
    size_t n = it->second.size() < cap ? it->second.size() : cap;
    memcpy(d, it->second.data(), n); return (uint8_t)n;
  }
  bool save(const char* k, const uint8_t* s, uint8_t len) override { small[k] = std::string((const char*)s, len); return true; }
  bool appendBlob(const char* k, const uint8_t* s, size_t len) override { blobs[k].append((const char*)s, len); appends++; return true; }
  size_t readBlobAt(const char* k, size_t off, uint8_t* d, size_t cap) override {
    auto it = blobs.find(k); if (it == blobs.end() || off >= it->second.size()) return 0;
    size_t n = std::min(cap, it->second.size() - off);
    memcpy(d, it->second.data() + off, n); return n;
  }
  size_t blobSize(const char* k) override { auto it = blobs.find(k); return it == blobs.end() ? 0 : it->second.size(); }
  bool removeBlob(const char* k) override { return blobs.erase(k) > 0; }
};

struct FakeApp : AppServices {
  bool supported = true, enabled = true, fix = true;
  int32_t lat = 0, lon = 0; float alt = 100; uint32_t now = 1000000;
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 0; }
  uint32_t epochSeconds() const override { return now; }
  bool gpsSupported() const override { return supported; }
  bool gpsEnabled() const override { return enabled; }
  void setGpsEnabled(bool on) override { enabled = on; }
  bool gpsHasFix() const override { return enabled && fix; }
  float gpsAltitudeM() const override { return alt; }
  bool gpsPosition1e6(int32_t& la, int32_t& lo) const override { la = lat; lo = lon; return true; }
};

const int32_t M10 = 90;   // ~10 m of latitude (1e6 degrees)

struct Rig {
  MemStorage st; FakeApp app; TrailRecorder rec; uint32_t t = 0;
  Rig(bool start = true) { rec.begin(&st); if (start) rec.start(); }
  bool tick(uint32_t adv = 1000) { t += adv; return rec.tick(t, app); }
  size_t pointsOnFlash() { return st.blobSize("trail") / 16; }
  // Decode record i from the stored blob.
  void rec_at(size_t i, int32_t& lat, int32_t& lon, uint8_t& flags) {
    uint8_t b[16]; ASSERT_EQ(16u, st.readBlobAt("trail", i * 16, b, 16));
    lat = (int32_t)(b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24);
    lon = (int32_t)(b[4] | b[5] << 8 | b[6] << 16 | (uint32_t)b[7] << 24);
    flags = b[14];
  }
  // Walk north by ~10 m per second to produce `n` points.
  void walk(int n) { for (int i = 0; i < n; i++) { app.lat += M10 + 5; tick(); } }
};
}  // namespace

TEST(Trail, FirstPointIsSegmentStart) {
  Rig r; r.app.lat = 123; r.app.lon = -456;
  r.tick();
  EXPECT_EQ(1u, r.rec.count());
  r.rec.flush();
  int32_t la, lo; uint8_t fl;
  r.rec_at(0, la, lo, fl);
  EXPECT_EQ(123, la); EXPECT_EQ(-456, lo); EXPECT_EQ(1, fl & 1);
  r.walk(1);
  r.rec.flush();
  r.rec_at(1, la, lo, fl);
  EXPECT_EQ(0, fl & 1);
}

TEST(Trail, TenMeterThreshold) {
  Rig r;
  r.tick();                                  // first point
  r.app.lat = 8 * 9;                         // ~8 m: too close
  r.tick(); EXPECT_EQ(1u, r.rec.count());
  r.app.lat = 12 * 9;                        // ~12 m from the first
  r.tick(); EXPECT_EQ(2u, r.rec.count());
  r.app.lat += 5 * 9;                        // ~5 m from the last
  r.tick(); EXPECT_EQ(2u, r.rec.count());
}

TEST(Trail, HeartbeatWhenStationary) {
  Rig r;
  r.tick();
  for (int i = 0; i < 298; i++) r.tick();    // < 5 min
  EXPECT_EQ(1u, r.rec.count());
  for (int i = 0; i < 3; i++) r.tick();
  EXPECT_EQ(2u, r.rec.count());              // heartbeat point, same place
}

TEST(Trail, NoFixNoPointsAndGpsOffPauses) {
  Rig r; r.app.fix = false;
  for (int i = 0; i < 5; i++) r.tick();
  EXPECT_EQ(0u, r.rec.count());
  EXPECT_EQ(TrailRecorder::State::PausedNoFix, r.rec.state(r.app));
  r.app.fix = true; r.app.enabled = false;
  r.tick();
  EXPECT_EQ(0u, r.rec.count());
  EXPECT_TRUE(r.rec.recording());            // still "on"
  EXPECT_EQ(TrailRecorder::State::PausedGpsOff, r.rec.state(r.app));
  r.app.enabled = true;
  r.tick();
  EXPECT_EQ(1u, r.rec.count());
}

TEST(Trail, ResumeAfterGpsOffStartsNewSegment) {
  Rig r;
  r.tick(); r.walk(1); r.rec.flush();        // two points, one segment
  r.app.enabled = false; r.tick();
  r.app.enabled = true; r.app.lat += 5000 * 9; r.tick();
  r.rec.flush();
  int32_t la, lo; uint8_t fl;
  r.rec_at(2, la, lo, fl);
  EXPECT_EQ(1, fl & 1);
}

TEST(Trail, StoppedDoesNotRecord) {
  Rig r(false);
  r.tick(); r.tick();
  EXPECT_EQ(0u, r.rec.count());
  EXPECT_EQ(TrailRecorder::State::Stopped, r.rec.state(r.app));
}

TEST(Trail, BatchesEightPointsOrSixtySeconds) {
  Rig r;
  r.tick(); r.walk(6);                       // 7 points buffered
  EXPECT_EQ(0, r.st.appends);
  EXPECT_EQ(0u, r.pointsOnFlash());
  r.walk(1);                                 // 8th -> one append
  EXPECT_EQ(1, r.st.appends);
  EXPECT_EQ(8u, r.pointsOnFlash());

  Rig q;                                     // time-based flush
  q.tick();
  for (int i = 0; i < 58; i++) q.tick();     // stationary, nothing new buffered
  EXPECT_EQ(0, q.st.appends);
  for (int i = 0; i < 3; i++) q.tick();
  EXPECT_EQ(1, q.st.appends);
  EXPECT_EQ(1u, q.pointsOnFlash());
}

TEST(Trail, FlushOnStopAndClear) {
  Rig r;
  r.tick(); r.walk(2);
  EXPECT_EQ(0, r.st.appends);
  r.rec.stop();
  EXPECT_EQ(3u, r.pointsOnFlash());
  EXPECT_FALSE(r.rec.recording());

  r.rec.start();
  r.walk(2);
  r.rec.clear();                             // buffered points dropped with the rest
  EXPECT_EQ(0u, r.pointsOnFlash());
  EXPECT_EQ(0u, r.rec.count());
  EXPECT_EQ(0.0f, r.rec.lengthKm());
  Rig again(false); again.st = r.st; again.rec.begin(&again.st);
  EXPECT_EQ(0u, again.rec.count());
}

TEST(Trail, LengthCountsWithinSegmentOnly) {
  Rig r;
  r.tick();
  r.app.lat += 1000 * 9; r.tick();           // ~1 km in the same segment? 1000*9e-6deg*111km/deg ~ 1 km
  float oneLeg = r.rec.lengthKm();
  EXPECT_NEAR(1.0f, oneLeg, 0.05f);
  r.app.enabled = false; r.tick();           // pause -> next point starts a new segment
  r.app.enabled = true; r.app.lat += 100000 * 9; r.tick();   // ~100 km jump, not counted
  EXPECT_NEAR(oneLeg, r.rec.lengthKm(), 0.001f);
  r.app.lat += 1000 * 9; r.tick();           // counted again
  EXPECT_NEAR(oneLeg * 2, r.rec.lengthKm(), 0.05f);
}

TEST(Trail, RebootResumesWithNewSegmentAndKeepsTotals) {
  MemStorage st;
  FakeApp app; uint32_t t = 0;
  float len;
  {
    TrailRecorder a; a.begin(&st); a.start();
    t += 1000; a.tick(t, app);
    app.lat += 1000 * 9; t += 1000; a.tick(t, app);
    a.flush();
    len = a.lengthKm();
    ASSERT_NEAR(1.0f, len, 0.05f);
  }
  TrailRecorder b; b.begin(&st);             // "reboot"
  EXPECT_TRUE(b.recording());
  EXPECT_EQ(2u, b.count());
  EXPECT_NEAR(len, b.lengthKm(), 0.001f);
  app.lat += 50000 * 9;                      // far away after the reboot
  t = 1000; b.tick(t, app);
  EXPECT_EQ(3u, b.count());
  EXPECT_NEAR(len, b.lengthKm(), 0.001f);    // new segment: jump not counted
  b.flush();
  uint8_t rec[16];
  ASSERT_EQ(16u, st.readBlobAt("trail", 32, rec, 16));
  EXPECT_EQ(1, rec[14] & 1);
}

TEST(Trail, MetaCountRepairedFromBlobSize) {
  MemStorage st;
  { TrailRecorder a; a.begin(&st); a.start(); FakeApp app; a.tick(1000, app); a.flush(); }
  // Power loss after an append but before the meta save: 2 more points on flash.
  uint8_t extra[32] = {0};
  st.blobs["trail"].append((const char*)extra, 32);
  TrailRecorder b; b.begin(&st);
  EXPECT_EQ(3u, b.count());
  // Meta claims points that are not on flash.
  st.blobs.clear();
  TrailRecorder c; c.begin(&st);
  EXPECT_EQ(0u, c.count());
  EXPECT_EQ(0.0f, c.lengthKm());
  // Garbage meta is ignored.
  st.small["trail_meta"] = "xx";
  TrailRecorder d; d.begin(&st);
  EXPECT_EQ(0u, d.count());
  EXPECT_FALSE(d.recording());
}

TEST(Trail, CapStopsRecordingWithOneShotEvent) {
  MemStorage st;
  // Pre-fill the file to one short of the cap and say so in the meta.
  st.blobs["trail"] = std::string((TrailRecorder::CAP - 1) * 16, '\0');
  TrailRecorder r; r.begin(&st); r.start();
  FakeApp app; uint32_t t = 0;
  t += 1000; EXPECT_TRUE(r.tick(t, app));                 // the cap-reaching point announces
  EXPECT_FALSE(r.recording());
  EXPECT_EQ((uint32_t)TrailRecorder::CAP, r.count());
  EXPECT_EQ(TrailRecorder::State::Full, r.state(app));
  EXPECT_EQ((size_t)TrailRecorder::CAP * 16, st.blobSize("trail"));
  app.lat += 5000;
  t += 1000; EXPECT_FALSE(r.tick(t, app));
  EXPECT_FALSE(r.start());
  r.clear();
  EXPECT_TRUE(r.start());
}

TEST(Trail, ClockUnknownLeavesTimesZero) {
  Rig r; r.app.now = 0;
  r.tick(); r.walk(1);
  EXPECT_EQ(0u, r.rec.firstTime());
}

TEST(Trail, TimesTracked) {
  Rig r; r.app.now = 5000;
  r.tick(); r.app.now = 5600; r.walk(1);
  EXPECT_EQ(5000u, r.rec.firstTime());
  EXPECT_EQ(5600u, r.rec.lastTime());
}

// ---- applet -----------------------------------------------------------------

namespace {
struct ARig {
  MemStorage st; FakeApp app; FakeDisplayDriver d; AppletContext ctx; AppletHost* host;
  ARig() {
    trailRecorder().begin(&st); trailRecorder().stop(); trailRecorder().clear();
    ctx.app = &app; ctx.storage = &st; host = new AppletHost(&d, ctx);
    host->setRoot(&trailApplet()); host->loop(0);
  }
  ~ARig() { delete host; trailRecorder().begin(nullptr); }
  void press(InputEvent e) { host->dispatch(e); }
};
}  // namespace

TEST(TrailApplet, StartStopToggle) {
  ARig r;
  EXPECT_STREQ("Start recording", trailApplet().actionLabelForTest(0));
  r.press(InputEvent::Select);
  EXPECT_TRUE(trailRecorder().recording());
  EXPECT_STREQ("Recording started", r.host->toastForTest());
  EXPECT_STREQ("Stop recording", trailApplet().actionLabelForTest(0));
  r.host->loop(100);
  EXPECT_STREQ("Recording", trailApplet().stateLineForTest());
}

TEST(TrailApplet, StateLines) {
  ARig r;
  r.host->loop(10);
  EXPECT_STREQ("Stopped", trailApplet().stateLineForTest());
  r.press(InputEvent::Select);
  r.host->loop(2000);
  EXPECT_STREQ("Recording", trailApplet().stateLineForTest());
  r.app.fix = false; r.host->loop(4000);
  EXPECT_STREQ("Paused (no fix)", trailApplet().stateLineForTest());
  r.app.enabled = false; r.host->loop(6000);
  EXPECT_STREQ("Paused (GPS off)", trailApplet().stateLineForTest());
  r.press(InputEvent::Select);               // stop
  EXPECT_FALSE(trailRecorder().recording());
}

TEST(TrailApplet, StartWithGpsOffTurnsItOn) {
  ARig r; r.app.enabled = false;
  r.press(InputEvent::Select);
  EXPECT_TRUE(r.app.enabled);
  EXPECT_TRUE(trailRecorder().recording());
  EXPECT_STREQ("GPS on", r.host->toastForTest());
}

TEST(TrailApplet, StartUnsupportedDoesNothing) {
  ARig r; r.app.supported = false;
  r.press(InputEvent::Select);
  EXPECT_FALSE(trailRecorder().recording());
  EXPECT_STREQ("GPS not available", r.host->toastForTest());
}

TEST(TrailApplet, StartWhenFullToasts) {
  ARig r;
  r.st.blobs["trail"] = std::string(TrailRecorder::CAP * 16, '\0');
  trailRecorder().begin(&r.st);
  r.host->loop(10);
  r.press(InputEvent::Select);
  EXPECT_FALSE(trailRecorder().recording());
  EXPECT_STREQ("Trail full - clear it first", r.host->toastForTest());
  r.host->loop(20);
  EXPECT_STREQ("Full", trailApplet().stateLineForTest());
}

TEST(TrailApplet, ClearNeedsConfirm) {
  ARig r;
  trailRecorder().start();
  trailRecorder().tick(1000, r.app);
  trailRecorder().flush();
  ASSERT_EQ(1u, trailRecorder().count());
  r.press(InputEvent::NavDown);              // Clear trail
  r.press(InputEvent::Select);               // confirm opens on Cancel
  r.press(InputEvent::Select);               // Cancel
  EXPECT_EQ(1u, trailRecorder().count());
  r.press(InputEvent::Select);
  r.press(InputEvent::NavRight);             // Confirm
  r.press(InputEvent::Select);
  EXPECT_EQ(0u, trailRecorder().count());
  EXPECT_EQ(0u, r.st.blobSize("trail"));
}

TEST(TrailApplet, BackBubbles) {
  ARig r;
  EXPECT_FALSE(trailApplet().onInput(InputEvent::Back));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
