#include <gtest/gtest.h>
#include <mishmesh/applets/UtilityApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

namespace {
struct DummyUtil : Applet {
  DummyUtil() : Applet("Dummy") {}
  int onRender(Canvas&) override { return 1000; }
};
DummyUtil* const kUtil1 = new DummyUtil();
Applet* const kNonUtil = reinterpret_cast<Applet*>(0x1);
}  // namespace

TEST(UtilityApplet, OnlyListsUtilityMenuPlacement) {
  resetRegistry();
  static AppletRegistration r1{kUtil1, "UtilOne", 0, Placement::UtilityMenu, 1, nullptr};
  static AppletRegistration r2{kNonUtil, "NotUtility", 0, Placement::AppMenu, 0, nullptr};
  registerApplet(&r1);
  registerApplet(&r2);

  AppletContext ctx;
  UtilityApplet& a = utilityApplet();
  a.onStart(ctx);
  EXPECT_EQ(1, a.count());
  EXPECT_STREQ("UtilOne", a.label(0));
}

TEST(UtilityApplet, SortsByOrder) {
  resetRegistry();
  static AppletRegistration r1{kUtil1, "Second", 0, Placement::UtilityMenu, 5, nullptr};
  static AppletRegistration r2{kUtil1, "First", 0, Placement::UtilityMenu, 1, nullptr};
  registerApplet(&r1);
  registerApplet(&r2);

  AppletContext ctx;
  UtilityApplet& a = utilityApplet();
  a.onStart(ctx);
  ASSERT_EQ(2, a.count());
  EXPECT_STREQ("First", a.label(0));
  EXPECT_STREQ("Second", a.label(1));
}

TEST(UtilityApplet, RendersWithoutCrash) {
  resetRegistry();
  static AppletRegistration r1{kUtil1, "UtilOne", 0, Placement::UtilityMenu, 0, nullptr};
  registerApplet(&r1);
  AppletContext ctx;
  UtilityApplet& a = utilityApplet();
  a.onStart(ctx);
  FakeDisplayDriver d(160, 80);
  Canvas c(&d, 0);
  a.onRender(c);
  SUCCEED();
}

TEST(UtilityApplet, SelectPushesTheFocusedItemOntoTheHost) {
  resetRegistry();
  static AppletRegistration r1{kUtil1, "UtilOne", 0, Placement::UtilityMenu, 0, nullptr};
  registerApplet(&r1);

  FakeDisplayDriver d(160, 80);
  AppletContext ctx;
  AppletHost host(&d, ctx);
  UtilityApplet& a = utilityApplet();
  host.setRoot(&a);
  EXPECT_TRUE(a.onInput(InputEvent::Select));
  EXPECT_EQ(kUtil1, host.foreground());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
