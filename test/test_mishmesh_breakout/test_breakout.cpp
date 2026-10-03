#include <gtest/gtest.h>
#include <mishmesh/applets/BreakoutApplet.h>
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

int render(BreakoutApplet& a, uint32_t t) {
  FakeDisplayDriver d(160, 80);
  Canvas c(&d, t);
  return a.onRender(c);
}

BreakoutApplet& started(AppletContext& ctx) {
  BreakoutApplet& a = breakoutApplet();
  a.onStart(ctx);
  render(a, 0);   // t=0: initializes the board
  return a;
}

void loseALife(BreakoutApplet& a) {
  a.setBallForTest(10, 1000, 0, 0);   // already past the bottom edge
  a.tickForTest();
}
}  // namespace

TEST(BreakoutApplet, StartsNotLaunchedNotGameOver) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  BreakoutApplet& a = started(ctx);
  EXPECT_FALSE(a.launchedForTest());
  EXPECT_FALSE(a.gameOverForTest());
  EXPECT_EQ(3, a.livesForTest());
  EXPECT_EQ(32, a.bricksRemainingForTest());
}

TEST(BreakoutApplet, SelectLaunchesTheBall) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  BreakoutApplet& a = started(ctx);
  EXPECT_TRUE(a.onInput(InputEvent::Select));
  EXPECT_TRUE(a.launchedForTest());
}

TEST(BreakoutApplet, PaddleStopsAtTheWalls) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  BreakoutApplet& a = started(ctx);
  a.setPaddleXForTest(0);
  a.onInput(InputEvent::NavLeft);
  EXPECT_EQ(0, a.paddleXForTest());

  int maxX = 160 - a.paddleWForTest();
  a.setPaddleXForTest(maxX);
  a.onInput(InputEvent::NavRight);
  EXPECT_EQ(maxX, a.paddleXForTest());
}

TEST(BreakoutApplet, BallBouncesOffTheLeftWall) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  BreakoutApplet& a = started(ctx);
  a.setBallForTest(0, 40, -a.ballSpeedForTest(), 0);
  a.tickForTest();
  EXPECT_GT(a.ballDxForTest(), 0);
  EXPECT_GE(a.ballXForTest(), 0);
}

TEST(BreakoutApplet, BallBouncesOffTheRightWall) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  BreakoutApplet& a = started(ctx);
  a.setBallForTest(157, 40, a.ballSpeedForTest(), 0);
  a.tickForTest();
  EXPECT_LT(a.ballDxForTest(), 0);
}

TEST(BreakoutApplet, BallBouncesOffTheTopWall) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  BreakoutApplet& a = started(ctx);
  a.killAllBricksExceptForTest(-1, -1);   // clear the board so only the wall is in play
  int top = a.boardTopForTest();
  a.setBallForTest(80, top, 0, -a.ballSpeedForTest());
  a.tickForTest();
  EXPECT_GT(a.ballDyForTest(), 0);
}

TEST(BreakoutApplet, BallBouncesOffThePaddle) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  BreakoutApplet& a = started(ctx);
  int px = a.paddleXForTest(), py = a.paddleYForTest();
  a.setBallForTest(px, py - 4, 0, a.ballSpeedForTest());
  a.tickForTest();
  EXPECT_LT(a.ballDyForTest(), 0);
}

TEST(BreakoutApplet, BallBreaksABrickAndScores) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  BreakoutApplet& a = started(ctx);   // full board: destroying one brick must not clear the level
  int top = a.boardTopForTest(), bw = a.brickWForTest();
  int bx = 4 * bw + bw / 2 - 1;
  a.setBallForTest(bx, top - 2, 0, a.ballSpeedForTest());
  a.tickForTest();
  EXPECT_EQ(31, a.bricksRemainingForTest());
  EXPECT_GT(a.scoreForTest(), 0);
}

TEST(BreakoutApplet, ClearingAllBricksRefillsAndSpeedsUp) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  BreakoutApplet& a = started(ctx);
  int top = a.boardTopForTest(), bw = a.brickWForTest();
  int startSpeed = a.ballSpeedForTest();
  a.killAllBricksExceptForTest(0, 4);
  int bx = 4 * bw + bw / 2 - 1;
  a.setBallForTest(bx, top - 2, 0, a.ballSpeedForTest());
  a.tickForTest();
  EXPECT_EQ(32, a.bricksRemainingForTest());
  EXPECT_EQ(startSpeed + 2, a.ballSpeedForTest());
  EXPECT_FALSE(a.launchedForTest());
}

TEST(BreakoutApplet, MissingThePaddleLosesALife) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  BreakoutApplet& a = started(ctx);
  loseALife(a);
  EXPECT_EQ(2, a.livesForTest());
  EXPECT_FALSE(a.gameOverForTest());
  EXPECT_FALSE(a.launchedForTest());
}

TEST(BreakoutApplet, LosingAllLivesEndsTheGame) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  BreakoutApplet& a = started(ctx);
  for (int i = 0; i < 3; i++) loseALife(a);
  EXPECT_TRUE(a.gameOverForTest());
  EXPECT_EQ(0, a.livesForTest());
}

TEST(BreakoutApplet, SelectAfterGameOverRestarts) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  BreakoutApplet& a = started(ctx);
  for (int i = 0; i < 3; i++) loseALife(a);
  ASSERT_TRUE(a.gameOverForTest());

  EXPECT_TRUE(a.onInput(InputEvent::Select));
  EXPECT_FALSE(a.gameOverForTest());
  EXPECT_EQ(3, a.livesForTest());
  EXPECT_EQ(0, a.scoreForTest());
  EXPECT_EQ(32, a.bricksRemainingForTest());
}

TEST(BreakoutApplet, BackBubbles) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  BreakoutApplet& a = started(ctx);
  EXPECT_FALSE(a.onInput(InputEvent::Back));
}

TEST(BreakoutApplet, HighScoreSurvivesAcrossOnStart) {
  FakeApp app;
  FakeStorage storage;
  AppletContext ctx; ctx.app = &app; ctx.storage = &storage;
  BreakoutApplet& a = started(ctx);
  EXPECT_EQ(0, a.hiScoreForTest());

  int top = a.boardTopForTest(), bw = a.brickWForTest();
  a.killAllBricksExceptForTest(0, 4);
  int bx = 4 * bw + bw / 2 - 1;
  a.setBallForTest(bx, top - 2, 0, a.ballSpeedForTest());
  a.tickForTest();
  ASSERT_GT(a.hiScoreForTest(), 0);
  uint16_t hi = a.hiScoreForTest();

  for (int i = 0; i < 3; i++) loseALife(a);
  ASSERT_TRUE(a.gameOverForTest());

  a.onStart(ctx);   // reopen: high score should reload from storage
  EXPECT_EQ(hi, a.hiScoreForTest());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
