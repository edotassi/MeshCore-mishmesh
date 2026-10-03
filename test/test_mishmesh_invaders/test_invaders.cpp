#include <gtest/gtest.h>
#include <mishmesh/applets/SpaceInvadersApplet.h>
#include <mishmesh/core/Canvas.h>
#include <string.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

namespace {
struct FakeApp : AppServices {
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 0; }
  uint32_t epochSeconds() const override { return 0; }
};

struct FakeStorage : AppletStorage {
  uint8_t data[8] = {0};
  uint8_t len = 0;
  bool    has = false;
  uint8_t load(const char*, uint8_t* dst, uint8_t cap) override {
    if (!has) return 0;
    uint8_t n = len < cap ? len : cap;
    memcpy(dst, data, n);
    return n;
  }
  bool save(const char*, const uint8_t* src, uint8_t l) override {
    len = l < sizeof(data) ? l : (uint8_t)sizeof(data);
    memcpy(data, src, len);
    has = true;
    return true;
  }
};

int render(SpaceInvadersApplet& a, uint32_t t) {
  FakeDisplayDriver d(160, 80);
  Canvas c(&d, t);
  return a.onRender(c);
}

SpaceInvadersApplet& started(AppletContext& ctx) {
  SpaceInvadersApplet& a = spaceInvadersApplet();
  a.onStart(ctx);
  render(a, 0);   // t=0: initializes the board
  return a;
}
}  // namespace

TEST(SpaceInvadersApplet, StartsFullFormationNotGameOver) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  SpaceInvadersApplet& a = started(ctx);
  EXPECT_FALSE(a.gameOverForTest());
  EXPECT_EQ(3, a.livesForTest());
  EXPECT_EQ(32, a.invadersRemainingForTest());
  EXPECT_FALSE(a.playerBulletActiveForTest());
}

TEST(SpaceInvadersApplet, SelectFiresAPlayerBullet) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  SpaceInvadersApplet& a = started(ctx);
  EXPECT_TRUE(a.onInput(InputEvent::Select));
  EXPECT_TRUE(a.playerBulletActiveForTest());
}

// Only one shot on screen at a time - the classic pacing constraint.
TEST(SpaceInvadersApplet, SelectDoesNothingWhileABulletIsInFlight) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  SpaceInvadersApplet& a = started(ctx);
  a.onInput(InputEvent::Select);
  int y = a.playerBulletYForTest();
  a.onInput(InputEvent::Select);   // should not reset/refire
  EXPECT_EQ(y, a.playerBulletYForTest());
}

TEST(SpaceInvadersApplet, PaddleStopsAtTheWalls) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  SpaceInvadersApplet& a = started(ctx);
  a.setPaddleXForTest(0);
  a.onInput(InputEvent::NavLeft);
  EXPECT_EQ(0, a.paddleXForTest());

  int maxX = 160 - a.paddleWForTest();
  a.setPaddleXForTest(maxX);
  a.onInput(InputEvent::NavRight);
  EXPECT_EQ(maxX, a.paddleXForTest());
}

TEST(SpaceInvadersApplet, PlayerBulletDestroysAnInvaderAndScores) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  SpaceInvadersApplet& a = started(ctx);   // full formation: one hit must not clear the wave
  int fx = a.formXForTest(), fy = a.formYForTest();
  int iw = a.invaderWForTest(), ih = a.invaderHForTest();
  int bx = fx + 4 * iw + iw / 2;   // centred under column 4
  int by = fy + ih + 2;            // just below row 0
  a.setPlayerBulletForTest(bx, by);
  a.tickBulletsForTest();
  EXPECT_EQ(31, a.invadersRemainingForTest());
  EXPECT_GT(a.scoreForTest(), 0);
  EXPECT_FALSE(a.playerBulletActiveForTest());
}

TEST(SpaceInvadersApplet, LastInvaderClearedSpawnsANewWave) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  SpaceInvadersApplet& a = started(ctx);
  int fx = a.formXForTest(), fy = a.formYForTest();
  int iw = a.invaderWForTest(), ih = a.invaderHForTest();
  a.killAllInvadersExceptForTest(0, 4);
  int bx = fx + 4 * iw + iw / 2;
  int by = fy + ih + 2;
  a.setPlayerBulletForTest(bx, by);
  a.tickBulletsForTest();
  EXPECT_EQ(32, a.invadersRemainingForTest());   // refilled for the next wave
}

TEST(SpaceInvadersApplet, FormationReversesAtTheRightEdge) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  SpaceInvadersApplet& a = started(ctx);
  int iw = a.invaderWForTest();
  // Placed so the very next step would push the formation's right edge past
  // the board - must bounce instead of crossing it.
  a.setFormForTest(160 - 8 * iw, a.formYForTest(), 1);
  int y = a.formYForTest();
  a.tickFormationForTest();
  EXPECT_EQ(-1, a.formDirForTest());
  EXPECT_EQ(y + a.invaderHForTest(), a.formYForTest());   // dropped one row
}

TEST(SpaceInvadersApplet, FormationReversesAtTheLeftEdge) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  SpaceInvadersApplet& a = started(ctx);
  a.setFormForTest(0, a.formYForTest(), -1);
  int y = a.formYForTest();
  a.tickFormationForTest();
  EXPECT_EQ(1, a.formDirForTest());
  EXPECT_EQ(y + a.invaderHForTest(), a.formYForTest());
}

TEST(SpaceInvadersApplet, FormationReachingThePaddleRowEndsTheGame) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  SpaceInvadersApplet& a = started(ctx);
  a.setFormForTest(a.formXForTest(), a.paddleYForTest(), 1);
  a.tickFormationForTest();
  EXPECT_TRUE(a.gameOverForTest());
}

TEST(SpaceInvadersApplet, InvaderBulletHittingThePlayerLosesALife) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  SpaceInvadersApplet& a = started(ctx);
  int px = a.paddleXForTest(), py = a.paddleYForTest();
  a.setInvaderBulletForTest(px + a.paddleWForTest() / 2, py - 2);
  a.tickBulletsForTest();
  EXPECT_EQ(2, a.livesForTest());
  EXPECT_FALSE(a.gameOverForTest());
  EXPECT_FALSE(a.invaderBulletActiveForTest());
}

TEST(SpaceInvadersApplet, LosingAllLivesEndsTheGame) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  SpaceInvadersApplet& a = started(ctx);
  int px = a.paddleXForTest(), py = a.paddleYForTest();
  for (int i = 0; i < 3; i++) {
    a.setInvaderBulletForTest(px + a.paddleWForTest() / 2, py - 2);
    a.tickBulletsForTest();
  }
  EXPECT_TRUE(a.gameOverForTest());
  EXPECT_EQ(0, a.livesForTest());
}

TEST(SpaceInvadersApplet, SelectAfterGameOverRestarts) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  SpaceInvadersApplet& a = started(ctx);
  a.setFormForTest(a.formXForTest(), a.paddleYForTest(), 1);
  a.tickFormationForTest();
  ASSERT_TRUE(a.gameOverForTest());

  EXPECT_TRUE(a.onInput(InputEvent::Select));
  EXPECT_FALSE(a.gameOverForTest());
  EXPECT_EQ(3, a.livesForTest());
  EXPECT_EQ(0, a.scoreForTest());
  EXPECT_EQ(32, a.invadersRemainingForTest());
}

TEST(SpaceInvadersApplet, BackBubbles) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  SpaceInvadersApplet& a = started(ctx);
  EXPECT_FALSE(a.onInput(InputEvent::Back));
}

TEST(SpaceInvadersApplet, HighScoreSurvivesAcrossOnStart) {
  FakeApp app;
  FakeStorage storage;
  AppletContext ctx; ctx.app = &app; ctx.storage = &storage;
  SpaceInvadersApplet& a = started(ctx);
  EXPECT_EQ(0, a.hiScoreForTest());

  int fx = a.formXForTest(), fy = a.formYForTest();
  int iw = a.invaderWForTest(), ih = a.invaderHForTest();
  a.setPlayerBulletForTest(fx + 4 * iw + iw / 2, fy + ih + 2);
  a.tickBulletsForTest();
  ASSERT_GT(a.hiScoreForTest(), 0);
  uint16_t hi = a.hiScoreForTest();

  a.setFormForTest(a.formXForTest(), a.paddleYForTest(), 1);
  a.tickFormationForTest();
  ASSERT_TRUE(a.gameOverForTest());

  a.onStart(ctx);   // reopen: high score should reload from storage
  EXPECT_EQ(hi, a.hiScoreForTest());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
