#include <gtest/gtest.h>
#include <mishmesh/applets/SnakeApplet.h>
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

// Ticks the applet by rendering once every TICK_MS (150ms), advancing exactly
// one game step per call, starting from t=0 (which only initializes the board).
uint32_t tick(SnakeApplet& a, uint32_t t) {
  t += 150;
  FakeDisplayDriver d(160, 80);
  Canvas c(&d, t);
  a.onRender(c);
  return t;
}
}  // namespace

TEST(SnakeApplet, StartsWithThreeSegmentsNotGameOver) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  SnakeApplet& a = snakeApplet();
  a.onStart(ctx);
  FakeDisplayDriver d(160, 80);
  Canvas c(&d, 0);
  a.onRender(c);   // t=0: only initializes the board, no step yet
  EXPECT_EQ(3, a.lengthForTest());
  EXPECT_FALSE(a.gameOverForTest());
  EXPECT_EQ(0, a.scoreForTest());
}

TEST(SnakeApplet, EatingFoodGrowsAndScores) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  SnakeApplet& a = snakeApplet();
  a.onStart(ctx);
  FakeDisplayDriver d(160, 80);
  Canvas c(&d, 0);
  a.onRender(c);   // init board; snake starts heading right

  a.setFoodForTest(a.headXForTest() + 1, a.headYForTest());
  tick(a, 0);       // moves onto the food

  EXPECT_EQ(4, a.lengthForTest());
  EXPECT_EQ(1, a.scoreForTest());
  EXPECT_EQ(1, a.hiScoreForTest());
  EXPECT_FALSE(a.gameOverForTest());
}

TEST(SnakeApplet, CannotReverseDirectly) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  SnakeApplet& a = snakeApplet();
  a.onStart(ctx);
  FakeDisplayDriver d(160, 80);
  Canvas c(&d, 0);
  a.onRender(c);   // init board; heading right
  int hx = a.headXForTest(), hy = a.headYForTest();

  a.onInput(InputEvent::NavLeft);   // directly opposite of Right: must be ignored
  tick(a, 0);

  EXPECT_EQ(hx + 1, a.headXForTest());   // still moved right, not left
  EXPECT_EQ(hy, a.headYForTest());
  EXPECT_FALSE(a.gameOverForTest());
}

TEST(SnakeApplet, HittingTheWallEndsTheGame) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  SnakeApplet& a = snakeApplet();
  a.onStart(ctx);
  FakeDisplayDriver d(160, 80);
  Canvas c(&d, 0);
  a.onRender(c);   // init board; heading right

  int stepsToWall = a.colsForTest() - a.headXForTest();
  uint32_t t = 0;
  for (int i = 0; i < stepsToWall; i++) t = tick(a, t);

  EXPECT_TRUE(a.gameOverForTest());
}

// Regression: Select on the game-over screen must start a brand new run, not
// resume the dead one.
TEST(SnakeApplet, SelectAfterGameOverRestarts) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  SnakeApplet& a = snakeApplet();
  a.onStart(ctx);
  FakeDisplayDriver d(160, 80);
  Canvas c(&d, 0);
  a.onRender(c);
  int stepsToWall = a.colsForTest() - a.headXForTest();
  uint32_t t = 0;
  for (int i = 0; i < stepsToWall; i++) t = tick(a, t);
  ASSERT_TRUE(a.gameOverForTest());

  EXPECT_TRUE(a.onInput(InputEvent::Select));
  EXPECT_FALSE(a.gameOverForTest());
  EXPECT_EQ(3, a.lengthForTest());
  EXPECT_EQ(0, a.scoreForTest());
}

TEST(SnakeApplet, HighScoreSurvivesAcrossOnStart) {
  FakeApp app;
  FakeStorage storage;
  AppletContext ctx; ctx.app = &app; ctx.storage = &storage;
  SnakeApplet& a = snakeApplet();

  a.onStart(ctx);
  EXPECT_EQ(0, a.hiScoreForTest());
  FakeDisplayDriver d(160, 80);
  Canvas c(&d, 0);
  a.onRender(c);
  a.setFoodForTest(a.headXForTest() + 1, a.headYForTest());
  uint32_t t = tick(a, 0);          // eat once: score 1

  int stepsToWall = a.colsForTest() - a.headXForTest();
  for (int i = 0; i < stepsToWall; i++) t = tick(a, t);   // then run into the wall
  ASSERT_TRUE(a.gameOverForTest());
  ASSERT_EQ(1, a.hiScoreForTest());

  a.onStart(ctx);   // reopen the applet: high score should be reloaded from storage
  EXPECT_EQ(1, a.hiScoreForTest());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
