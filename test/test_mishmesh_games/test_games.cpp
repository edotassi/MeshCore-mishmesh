#include <gtest/gtest.h>
#include <mishmesh/applets/GamesApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

namespace {
struct DummyGame : Applet {
  DummyGame() : Applet("Dummy") {}
  int onRender(Canvas&) override { return 1000; }
};
DummyGame* const kGame1 = new DummyGame();
Applet* const kNonGame = reinterpret_cast<Applet*>(0x1);
}  // namespace

TEST(GamesApplet, OnlyListsGamesMenuPlacement) {
  resetRegistry();
  static AppletRegistration r1{kGame1, "GameOne", 0, Placement::GamesMenu, 1, nullptr};
  static AppletRegistration r2{kNonGame, "NotAGame", 0, Placement::AppMenu, 0, nullptr};
  registerApplet(&r1);
  registerApplet(&r2);

  AppletContext ctx;
  GamesApplet& a = gamesApplet();
  a.onStart(ctx);
  EXPECT_EQ(1, a.count());
  EXPECT_STREQ("GameOne", a.label(0));
}

TEST(GamesApplet, SortsByOrder) {
  resetRegistry();
  static AppletRegistration r1{kGame1, "Second", 0, Placement::GamesMenu, 5, nullptr};
  static AppletRegistration r2{kGame1, "First", 0, Placement::GamesMenu, 1, nullptr};
  registerApplet(&r1);
  registerApplet(&r2);

  AppletContext ctx;
  GamesApplet& a = gamesApplet();
  a.onStart(ctx);
  ASSERT_EQ(2, a.count());
  EXPECT_STREQ("First", a.label(0));
  EXPECT_STREQ("Second", a.label(1));
}

TEST(GamesApplet, RendersWithoutCrash) {
  resetRegistry();
  static AppletRegistration r1{kGame1, "GameOne", 0, Placement::GamesMenu, 0, nullptr};
  registerApplet(&r1);
  AppletContext ctx;
  GamesApplet& a = gamesApplet();
  a.onStart(ctx);
  FakeDisplayDriver d(160, 80);
  Canvas c(&d, 0);
  a.onRender(c);
  SUCCEED();
}

TEST(GamesApplet, SelectPushesTheFocusedGameOntoTheHost) {
  resetRegistry();
  static AppletRegistration r1{kGame1, "GameOne", 0, Placement::GamesMenu, 0, nullptr};
  registerApplet(&r1);

  FakeDisplayDriver d(160, 80);
  AppletContext ctx;
  AppletHost host(&d, ctx);   // wires ctx.host = &host internally
  GamesApplet& a = gamesApplet();
  host.setRoot(&a);
  EXPECT_TRUE(a.onInput(InputEvent::Select));
  EXPECT_EQ(kGame1, host.foreground());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
