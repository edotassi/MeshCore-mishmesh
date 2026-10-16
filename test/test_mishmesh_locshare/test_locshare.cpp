#include <gtest/gtest.h>
#include <mishmesh/applets/gps/LiveShare.h>
#include <mishmesh/applets/gps/LiveShareApplet.h>
#include <mishmesh/applets/gps/LocationText.h>
#include <mishmesh/applets/gps/NavigateApplet.h>
#include <mishmesh/applets/MessageThreadApplet.h>
#include <mishmesh/applets/MessagesApplet.h>
#include <mishmesh/core/AppletHost.h>
#include "FakeDisplayDriver.h"
#include "FakeMessagesService.h"
#include <string.h>

using namespace mishmesh;

namespace {
struct FakeApp : AppServices {
  bool supported = true, enabled = true, fix = true;
  int32_t lat = 45464211, lon = 9191383;
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 0; }
  uint32_t epochSeconds() const override { return 0; }
  bool gpsSupported() const override { return supported; }
  bool gpsEnabled() const override { return enabled; }
  void setGpsEnabled(bool on) override { enabled = on; }
  bool gpsHasFix() const override { return enabled && fix; }
  bool gpsPosition1e6(int32_t& la, int32_t& lo) const override { la = lat; lo = lon; return true; }
};
#define PIN MISHMESH_PIN_UTF8

bool parses(const char* t, int32_t la, int32_t lo) {
  int32_t a = 0, b = 0;
  return parseLocationText(t, a, b) && a == la && b == lo;
}
bool rejects(const char* t) { int32_t a, b; return !parseLocationText(t, a, b); }
}  // namespace

// ---- format / parse ---------------------------------------------------------

TEST(LocationText, Format) {
  char b[64];
  EXPECT_GT(formatLocationText(b, sizeof(b), 45464211, 9191383, false), 0u);
  EXPECT_STREQ(PIN " 45.46421,9.19138", b);
  formatLocationText(b, sizeof(b), -33868800, -151209300, true);
  EXPECT_STREQ(PIN " -33.86880,-151.20930 (live)", b);
  formatLocationText(b, sizeof(b), 0, 0, false);
  EXPECT_STREQ(PIN " 0.00000,0.00000", b);
  EXPECT_EQ(0u, formatLocationText(b, 8, 1, 1, false));   // too small
}

TEST(LocationText, AcceptTable) {
  EXPECT_TRUE(parses(PIN " 45.46421,9.19138", 45464210, 9191380));
  EXPECT_TRUE(parses(PIN "45.46421,9.19138", 45464210, 9191380));          // no space after pin
  EXPECT_TRUE(parses(PIN "   45.46421, 9.19138", 45464210, 9191380));      // spaces, space after comma
  EXPECT_TRUE(parses(PIN " -33.8688,-151.2093", -33868800, -151209300));
  EXPECT_TRUE(parses(PIN " +10.5,+20.5", 10500000, 20500000));
  EXPECT_TRUE(parses(PIN " 1.5,2.5", 1500000, 2500000));                   // 1 decimal
  EXPECT_TRUE(parses(PIN " 1,2", 1000000, 2000000));                       // no decimals
  EXPECT_TRUE(parses(PIN " 45.1234567,9.7654321", 45123456, 9765432));     // 7 decimals (7th truncated)
  EXPECT_TRUE(parses(PIN " 45.46421,9.19138 (live)", 45464210, 9191380));  // trailing text
  EXPECT_TRUE(parses(PIN " 45.46421,9.19138 meet here", 45464210, 9191380));
  EXPECT_TRUE(parses(PIN " 90,180", 90000000, 180000000));
  EXPECT_TRUE(parses(PIN " -90,-180", -90000000, -180000000));
}

TEST(LocationText, RejectTable) {
  EXPECT_TRUE(rejects(nullptr));
  EXPECT_TRUE(rejects(""));
  EXPECT_TRUE(rejects("45.46421,9.19138"));                     // no pin
  EXPECT_TRUE(rejects("I am at 45.46421,9.19138"));             // free text coordinates
  EXPECT_TRUE(rejects("hello " PIN " 45.46421,9.19138"));       // pin mid-text
  EXPECT_TRUE(rejects(" " PIN " 45.46421,9.19138"));            // leading space before the pin
  EXPECT_TRUE(rejects(PIN));
  EXPECT_TRUE(rejects(PIN " garbage"));
  EXPECT_TRUE(rejects(PIN " 45.46421"));                        // one number
  EXPECT_TRUE(rejects(PIN " 45.46421;9.19138"));
  EXPECT_TRUE(rejects(PIN " 45.,9.1"));
  EXPECT_TRUE(rejects(PIN " .5,9.1"));
  EXPECT_TRUE(rejects(PIN " 45.12345678,9.1"));                 // 8 decimals
  EXPECT_TRUE(rejects(PIN " 90.0001,10"));                      // lat out of range
  EXPECT_TRUE(rejects(PIN " -91,10"));
  EXPECT_TRUE(rejects(PIN " 10,180.5"));                        // lon out of range
  EXPECT_TRUE(rejects(PIN " 10,-181"));
  EXPECT_TRUE(rejects(PIN " 45.4,9.1abc"));                     // junk glued to the number
  EXPECT_TRUE(rejects(PIN " 45.4,9.1.2"));
  EXPECT_TRUE(rejects(PIN " 1234,10"));                         // absurd integer part
  EXPECT_TRUE(rejects("\xF0\x9F\x93\x8C 45.4,9.1"));            // a different emoji
}

TEST(LocationText, RoundTrip) {
  const int32_t pts[][2] = {{45464210, 9191380}, {-33868800, 151209300}, {0, 0}, {89999990, -179999990}};
  for (auto& p : pts) {
    char b[64]; formatLocationText(b, sizeof(b), p[0], p[1], true);
    int32_t la, lo;
    ASSERT_TRUE(parseLocationText(b, la, lo)) << b;
    EXPECT_EQ(p[0], la); EXPECT_EQ(p[1], lo);
  }
}

// ---- live share -------------------------------------------------------------

namespace {
ConvoKey dm() { return directKey((const uint8_t*)"ALICE!"); }
}

TEST(LiveShare, NeedsTarget) {
  LiveShare s;
  EXPECT_FALSE(s.start(0));
  EXPECT_FALSE(s.active());
}

TEST(LiveShare, ImmediateFirstSendThenInterval) {
  LiveShare s; FakeApp app; FakeMessagesService svc;
  s.setTarget(dm(), "Alice");
  ASSERT_TRUE(s.start(0));
  EXPECT_EQ(LiveShare::Event::Sent, s.tick(0, app, svc));
  EXPECT_EQ(1, svc.sends);
  EXPECT_NE(nullptr, strstr(svc.lastSent.c_str(), "(live)"));
  EXPECT_EQ(LiveShare::Event::None, s.tick(1000, app, svc));
  EXPECT_EQ(LiveShare::Event::None, s.tick(599000, app, svc));      // < 10 min
  EXPECT_EQ(LiveShare::Event::Sent, s.tick(600000, app, svc));
  EXPECT_EQ(2, svc.sends);
}

TEST(LiveShare, SkipsWithoutFixAndNeverSendsStale) {
  LiveShare s; FakeApp app; FakeMessagesService svc;
  app.fix = false;
  s.setTarget(dm(), "Alice"); s.start(0);
  for (uint32_t t = 0; t < 5000; t += 1000) EXPECT_EQ(LiveShare::Event::None, s.tick(t, app, svc));
  EXPECT_EQ(0, svc.sends);
  app.fix = true; app.lat = 1000000; app.lon = 2000000;
  EXPECT_EQ(LiveShare::Event::Sent, s.tick(5000, app, svc));       // first send as soon as a fix exists
  EXPECT_NE(nullptr, strstr(svc.lastSent.c_str(), "1.00000,2.00000"));
  app.enabled = false;
  EXPECT_EQ(LiveShare::Event::None, s.tick(605000, app, svc));      // GPS off: skip, keep sharing
  EXPECT_TRUE(s.active());
}

TEST(LiveShare, EndsAfterDurationWithOneEvent) {
  LiveShare s; FakeApp app; FakeMessagesService svc;
  s.setTarget(dm(), "Alice"); s.start(0);                           // default 2 h
  s.tick(0, app, svc);
  EXPECT_EQ(7200u, s.remainingSecs(0));
  EXPECT_NE(LiveShare::Event::Ended, s.tick(7199000, app, svc));
  LiveShare::Event e = s.tick(7200000, app, svc);
  EXPECT_EQ(LiveShare::Event::Ended, e);
  EXPECT_FALSE(s.active());
  EXPECT_EQ(LiveShare::Event::None, s.tick(7201000, app, svc));
}

TEST(LiveShare, StopHaltsSending) {
  LiveShare s; FakeApp app; FakeMessagesService svc;
  s.setTarget(dm(), "Alice"); s.start(0);
  s.tick(0, app, svc);
  s.stop();
  EXPECT_EQ(LiveShare::Event::None, s.tick(600000, app, svc));
  EXPECT_EQ(1, svc.sends);
}

TEST(LiveShare, DurationAndIntervalCycleAndDefaults) {
  LiveShare s;
  EXPECT_STREQ("2 h", LiveShare::durationLabel(s.durationIdx()));
  EXPECT_STREQ("10 min", LiveShare::intervalLabel(s.intervalIdx()));
  s.cycleDuration(); EXPECT_STREQ("4 h", LiveShare::durationLabel(s.durationIdx()));
  s.cycleDuration(); s.cycleDuration(); EXPECT_STREQ("1 h", LiveShare::durationLabel(s.durationIdx()));
  s.cycleInterval(); s.cycleInterval(); EXPECT_EQ(30u * 60, LiveShare::intervalSecs(s.intervalIdx()));
  s.cycleInterval(); EXPECT_EQ(5u * 60, LiveShare::intervalSecs(s.intervalIdx()));
}

TEST(LiveShareApplet, StartRequiresTargetThenRuns) {
  liveShare().resetForTest();
  FakeApp app; FakeMessagesService svc;
  svc.store.appendInbound(dm(), "hi", 2, 1, 1, 0, nullptr, 0);
  FakeDisplayDriver d; AppletContext ctx; ctx.app = &app; ctx.messages = &svc;
  AppletHost host(&d, ctx);
  host.setRoot(&liveShareApplet()); host.loop(0);
  host.dispatch(InputEvent::NavUp);                  // wraps to Start
  host.dispatch(InputEvent::Select);
  EXPECT_FALSE(liveShare().active());
  EXPECT_STREQ("Pick a target", host.toastForTest());
  host.dispatch(InputEvent::NavDown);                // -> Target
  host.dispatch(InputEvent::Select);
  EXPECT_TRUE(liveShareApplet().pickingForTest());
  host.dispatch(InputEvent::Select);                 // first conversation
  EXPECT_FALSE(liveShareApplet().pickingForTest());
  EXPECT_TRUE(liveShare().hasTarget());
  host.dispatch(InputEvent::NavDown);                // Duration
  host.dispatch(InputEvent::Select);
  EXPECT_STREQ("4 h", liveShareApplet().rowValueForTest(1));
  host.dispatch(InputEvent::NavUp); host.dispatch(InputEvent::NavUp);   // wrap to Start
  host.dispatch(InputEvent::Select);
  EXPECT_TRUE(liveShare().active());
  host.dispatch(InputEvent::Select);                 // Stop
  EXPECT_FALSE(liveShare().active());
  liveShare().resetForTest();
}

// ---- thread: send + receive -------------------------------------------------

namespace {
struct ThreadRig {
  FakeMessagesService svc; FakeApp app; FakeDisplayDriver d; AppletContext ctx; AppletHost* host;
  ConvoKey k;
  ThreadRig(ConvoKey key) : k(key) {
    ctx.messages = &svc; ctx.app = &app; host = new AppletHost(&d, ctx);
  }
  ~ThreadRig() { delete host; }
  void open() {
    host->setRoot(&messagesApplet());
    messageThreadApplet().setTarget(k);
    host->push(&messageThreadApplet());
  }
};
}  // namespace

TEST(LocationSend, OneShotNeedsFix) {
  ThreadRig r(dm());
  r.svc.store.appendInbound(r.k, "hi", 2, 1, 1, 0, nullptr, 0);
  r.open();
  auto sendFromQuickList = [&]() {
    r.host->dispatch(InputEvent::NavDown);     // past last message -> Write
    r.host->dispatch(InputEvent::NavDown);     // Quick
    r.host->dispatch(InputEvent::Select);      // quick replies overlay
    r.host->dispatch(InputEvent::NavUp);       // wraps to the trailing "Send my location"
    r.host->dispatch(InputEvent::Select);
  };
  r.app.fix = false;
  sendFromQuickList();
  EXPECT_STREQ("No GPS fix", r.host->toastForTest());
  EXPECT_EQ(0, r.svc.sends);

  r.app.enabled = false;
  r.host->dispatch(InputEvent::NavDown); r.host->dispatch(InputEvent::Select);
  r.host->dispatch(InputEvent::NavUp); r.host->dispatch(InputEvent::Select);
  EXPECT_STREQ("GPS is off", r.host->toastForTest());
  EXPECT_EQ(0, r.svc.sends);

  r.app.enabled = true; r.app.fix = true;
  r.host->dispatch(InputEvent::NavDown); r.host->dispatch(InputEvent::Select);
  r.host->dispatch(InputEvent::NavUp); r.host->dispatch(InputEvent::Select);
  EXPECT_EQ(1, r.svc.sends);
  EXPECT_STREQ(PIN " 45.46421,9.19138", r.svc.lastSent.c_str());
  EXPECT_TRUE(r.svc.lastSentKey.equals(r.k));
}

namespace {
// Opens the per-message menu on the newest message; returns the label list.
std::vector<std::string> menuLabels(ThreadRig& r) {
  r.open();
  r.host->dispatch(InputEvent::Select);
  std::vector<std::string> out;
  for (int i = 0; i < 6; i++) {
    const char* l = messageThreadApplet().msgMenuLabelForTest(i);
    if (l && l[0] && (out.empty() || out.back() != l)) out.push_back(l);
  }
  return out;
}
bool has(const std::vector<std::string>& v, const char* s) {
  for (auto& x : v) if (x == s) return true;
  return false;
}
}  // namespace

TEST(LocationReceive, NavigateOnlyOnLocationMessages) {
  { ThreadRig r(dm());
    const char* t = PIN " 45.46421,9.19138";
    r.svc.store.appendInbound(r.k, t, (uint16_t)strlen(t), 1, 1, 0, nullptr, 0);
    EXPECT_TRUE(has(menuLabels(r), "Navigate")); }
  { ThreadRig r(dm());
    r.svc.store.appendInbound(r.k, "meet at 45.46421,9.19138", 24, 1, 1, 0, nullptr, 0);
    EXPECT_FALSE(has(menuLabels(r), "Navigate")); }
}

TEST(LocationReceive, NavigateSetsTargetFromDmName) {
  ThreadRig r(dm());
  const char* t = PIN " 45.46421,9.19138 (live)";
  r.svc.store.appendInbound(r.k, t, (uint16_t)strlen(t), 1, 1, 0, nullptr, 0);
  r.open();
  r.host->dispatch(InputEvent::Select);              // per-message menu: Reply, Navigate, ...
  r.host->dispatch(InputEvent::NavDown);
  r.host->dispatch(InputEvent::Select);
  EXPECT_EQ((Applet*)&navigateApplet(), r.host->foreground());
  EXPECT_EQ(45464210, navigateApplet().targetLatForTest());
  EXPECT_EQ(9191380, navigateApplet().targetLonForTest());
  EXPECT_NE('\0', navigateApplet().targetNameForTest()[0]);
}

TEST(LocationReceive, NavigateUsesChannelSenderName) {
  ConvoKey ch = channelKey(2);
  ThreadRig r(ch);
  r.svc.senderName = "Bob";
  const char* t = PIN " 10.5,20.5";
  r.svc.store.appendInbound(r.k, t, (uint16_t)strlen(t), 1, 1, 0, nullptr, 0);
  r.open();
  r.host->dispatch(InputEvent::Select);
  r.host->dispatch(InputEvent::NavDown);
  r.host->dispatch(InputEvent::Select);
  EXPECT_EQ((Applet*)&navigateApplet(), r.host->foreground());
  EXPECT_STREQ("Bob", navigateApplet().targetNameForTest());
  EXPECT_EQ(10500000, navigateApplet().targetLatForTest());
  EXPECT_EQ(20500000, navigateApplet().targetLonForTest());
}

TEST(LocationReceive, OutboundLocationHasNoNavigate) {
  ThreadRig r(dm());
  const char* t = PIN " 10.5,20.5";
  r.svc.store.appendOutboundDM(r.k, t, (uint16_t)strlen(t), 1, 1, 0, 0);
  r.open();
  r.host->dispatch(InputEvent::Select);
  EXPECT_FALSE(has([&] {
    std::vector<std::string> v;
    for (int i = 0; i < 6; i++) { const char* l = messageThreadApplet().msgMenuLabelForTest(i); if (l && l[0]) v.push_back(l); }
    return v; }(), "Navigate"));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
