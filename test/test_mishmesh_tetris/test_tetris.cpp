#include <gtest/gtest.h>
#include <mishmesh/applets/TetrisApplet.h>
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

int render(TetrisApplet& a, uint32_t t) {
  FakeDisplayDriver d(160, 80);
  Canvas c(&d, t);
  return a.onRender(c);
}

TetrisApplet& started(AppletContext& ctx) {
  TetrisApplet& a = tetrisApplet();
  a.onStart(ctx);
  render(a, 0);   // t=0: initializes the board
  return a;
}

// Piece index 1 (O) is defined at local cells (1,1),(1,2),(2,1),(2,2), so at
// board x=px it occupies columns px+1 and px+2 - not px/px+1. Kept as a named
// helper so every O-piece placement in these tests agrees on that offset.
void placeO(TetrisApplet& a, int leftCol, int py) {
  a.setPieceForTest(1, 0, leftCol - 1, py);
}
}  // namespace

TEST(TetrisApplet, StartsWithAPieceSpawnedNotGameOver) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  TetrisApplet& a = started(ctx);
  EXPECT_FALSE(a.gameOverForTest());
  EXPECT_GE(a.pieceForTest(), 0);
  EXPECT_LT(a.pieceForTest(), 7);
}

TEST(TetrisApplet, MoveLeftStopsAtTheWall) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  TetrisApplet& a = started(ctx);
  a.setPieceForTest(0 /* I */, 0, 0, 0);   // already flush against the left wall
  int x = a.pieceXForTest();
  a.onInput(InputEvent::NavLeft);
  EXPECT_EQ(x, a.pieceXForTest());   // blocked, unchanged
}

TEST(TetrisApplet, MoveRightStopsAtTheWall) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  TetrisApplet& a = started(ctx);
  placeO(a, a.colsForTest() - 2, 0);   // flush against the right wall
  int x = a.pieceXForTest();
  a.onInput(InputEvent::NavRight);
  EXPECT_EQ(x, a.pieceXForTest());
}

TEST(TetrisApplet, RotateChangesFootprintForAnAsymmetricPiece) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  TetrisApplet& a = started(ctx);
  a.setPieceForTest(0 /* I */, 0, 3, 3);   // clear of any wall, room to rotate
  a.onInput(InputEvent::NavUp);
  EXPECT_EQ(1, a.rotForTest());
}

// Regression: rotating against a wall must kick sideways rather than getting
// stuck (or worse, rotating into an illegal out-of-bounds position).
TEST(TetrisApplet, RotationKicksAwayFromTheWall) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  TetrisApplet& a = started(ctx);
  // Vertical I (rot 1, local col 2) with px=-2 sits exactly at board col 0.
  // Rotating to rot 2 (horizontal, local cols 0-3) needs board cols -2..1
  // unkicked - must shift right to fit.
  a.setPieceForTest(0 /* I */, 1, -2, 3);
  a.onInput(InputEvent::NavUp);
  EXPECT_EQ(2, a.rotForTest());
  EXPECT_EQ(0, a.pieceXForTest());
}

TEST(TetrisApplet, SoftDropMovesDownAndScores) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  TetrisApplet& a = started(ctx);
  placeO(a, 4, 0);
  a.onInput(InputEvent::NavDown);
  EXPECT_EQ(1, a.pieceYForTest());
  EXPECT_EQ(1, a.scoreForTest());
}

TEST(TetrisApplet, HardDropLocksThePieceAtTheFloor) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  TetrisApplet& a = started(ctx);
  int rows = a.rowsForTest();
  placeO(a, 4, 0);
  a.onInput(InputEvent::Select);
  EXPECT_NE(0, a.cellForTest(rows - 1, 4));
  EXPECT_NE(0, a.cellForTest(rows - 1, 5));
}

TEST(TetrisApplet, ClearingALineAwardsScoreAndEmptiesTheRow) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  TetrisApplet& a = started(ctx);
  int rows = a.rowsForTest();
  int bottom = rows - 1;
  // Fill the bottom row except the two columns the O piece will land in.
  for (int col = 0; col < a.colsForTest(); col++) {
    if (col == 4 || col == 5) continue;
    a.setCellForTest(bottom, col, 1);
  }
  placeO(a, 4, bottom - 1);   // one row up, ready to drop one step
  a.tickForTest();           // gravity step locks it and clears the completed line
  EXPECT_EQ(100, a.scoreForTest());
  EXPECT_EQ(0, a.cellForTest(bottom, 4));   // cleared row is empty again
}

// Rows 0-1 filled except the last column (so they DON'T auto-clear), which
// blankets exactly where new pieces spawn (columns 3-6) - the next spawn
// must collide and end the game.
TEST(TetrisApplet, StackReachingTheTopEndsTheGame) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  TetrisApplet& a = started(ctx);
  for (int col = 0; col < a.colsForTest() - 1; col++) {
    a.setCellForTest(0, col, 1);
    a.setCellForTest(1, col, 1);
  }
  placeO(a, 4, 5);   // well clear of the jam, free to fall to the floor
  for (int i = 0; i < 30 && !a.gameOverForTest(); i++) a.tickForTest();
  EXPECT_TRUE(a.gameOverForTest());
}

TEST(TetrisApplet, SelectAfterGameOverRestarts) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  TetrisApplet& a = started(ctx);
  for (int col = 0; col < a.colsForTest() - 1; col++) {
    a.setCellForTest(0, col, 1);
    a.setCellForTest(1, col, 1);
  }
  placeO(a, 4, a.rowsForTest() - 4);
  for (int i = 0; i < 30 && !a.gameOverForTest(); i++) a.tickForTest();
  ASSERT_TRUE(a.gameOverForTest());

  EXPECT_TRUE(a.onInput(InputEvent::Select));
  EXPECT_FALSE(a.gameOverForTest());
  EXPECT_EQ(0, a.scoreForTest());
}

TEST(TetrisApplet, BackBubbles) {
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  TetrisApplet& a = started(ctx);
  EXPECT_FALSE(a.onInput(InputEvent::Back));
}

TEST(TetrisApplet, HighScoreSurvivesAcrossOnStart) {
  FakeApp app;
  FakeStorage storage;
  AppletContext ctx; ctx.app = &app; ctx.storage = &storage;
  TetrisApplet& a = started(ctx);
  EXPECT_EQ(0, a.hiScoreForTest());

  int rows = a.rowsForTest();
  int bottom = rows - 1;
  for (int col = 0; col < a.colsForTest(); col++) {
    if (col == 4 || col == 5) continue;
    a.setCellForTest(bottom, col, 1);
  }
  placeO(a, 4, bottom - 1);
  a.tickForTest();   // clears a line: score 100, and hiScore tracks it live
  ASSERT_EQ(100, a.hiScoreForTest());

  // Force game over so the save-on-game-over path fires.
  for (int col = 0; col < a.colsForTest() - 1; col++) {
    a.setCellForTest(0, col, 1);
    a.setCellForTest(1, col, 1);
  }
  placeO(a, 4, rows - 4);
  for (int i = 0; i < 30 && !a.gameOverForTest(); i++) a.tickForTest();
  ASSERT_TRUE(a.gameOverForTest());

  a.onStart(ctx);   // reopen: high score should reload from storage
  EXPECT_EQ(100, a.hiScoreForTest());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
