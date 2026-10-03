#pragma once

#include <mishmesh/core/Applet.h>

namespace mishmesh {

class Canvas;

// Classic Space Invaders: a formation of invaders marches side to side,
// dropping a row and speeding up each time it hits an edge, firing back at
// the player periodically. One shot on screen per side at a time (classic
// pacing constraint). Clearing the formation spawns a fresh, faster wave.
// 3 lives; high score persists via AppletStorage. Controls: Left/Right move
// the ship (repeats while held), Select fires (or restarts after game
// over). Real-time; Back pops. GamesMenu entry (see GamesApplet).
class SpaceInvadersApplet : public Applet {
public:
  SpaceInvadersApplet();

  bool wantsExclusive() const override { return true; }
  uint16_t repeatMask() const override {
    return maskBit(InputEvent::NavLeft) | maskBit(InputEvent::NavRight);
  }

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // Test hooks.
  uint16_t scoreForTest()    const { return _score; }
  uint16_t hiScoreForTest()  const { return _hiScore; }
  int      livesForTest()    const { return _lives; }
  bool     gameOverForTest() const { return _gameOver; }
  int      invadersRemainingForTest() const { return countAliveInvaders(); }
  int      paddleXForTest()  const { return _paddleX; }
  int      paddleWForTest()  const { return _paddleW; }
  int      paddleYForTest()  const { return _paddleY; }
  int      boardTopForTest() const { return _boardTop; }
  int      invaderWForTest() const { return _invaderW; }
  int      invaderHForTest() const { return _invaderH; }
  int      formXForTest()    const { return _formX; }
  int      formYForTest()    const { return _formY; }
  int      formDirForTest()  const { return _formDir; }
  bool     playerBulletActiveForTest()  const { return _pBulletActive; }
  int      playerBulletYForTest()       const { return _pBulletY; }
  bool     invaderBulletActiveForTest() const { return _iBulletActive; }
  int      invaderBulletYForTest()      const { return _iBulletY; }
  void     setPaddleXForTest(int x) { _paddleX = x; }
  void     setFormForTest(int x, int y, int dir) { _formX = x; _formY = y; _formDir = dir; }
  void     killAllInvadersExceptForTest(int row, int col);
  void     setPlayerBulletForTest(int x, int y) {
    _pBulletActive = true; _pBulletX = x; _pBulletY = y;
  }
  void     setInvaderBulletForTest(int x, int y) {
    _iBulletActive = true; _iBulletX = x; _iBulletY = y;
  }
  void     tickBulletsForTest()    { stepBullets(); }
  void     tickFormationForTest()  { stepFormation(); }

private:
  static const int INV_COLS = 8;
  static const int INV_ROWS = 4;
  // The formation is narrower than the board (divided by COLS+MARGIN_COLS,
  // not just COLS), leaving room on both sides to actually sweep side to
  // side - at exactly board-width the grid would have nowhere to move.
  static const int MARGIN_COLS = 3;
  static const int FORM_STEP_PX = 2;
  static const int BULLET_STEP_PX = 3;
  static const uint32_t BULLET_TICK_MS = 25;
  static const uint32_t FORM_TICK_MS_BASE = 500;
  static const uint32_t FORM_TICK_MS_MIN = 120;

  void initBoard(Canvas& c);
  void resetGame();
  void spawnInvaders();
  void stepFormation();
  void stepBullets();
  void maybeFireInvader();
  void endGame();
  int  countAliveInvaders() const;

  AppletStorage* _storage = nullptr;

  bool _boardReady = false;
  int  _w = 0, _h = 0, _boardTop = 0;
  int  _invaderW = 8, _invaderH = 5;
  int  _paddleW = 16, _paddleY = 0;

  bool _invaders[INV_ROWS][INV_COLS];
  int  _formX = 0, _formY = 0, _formDir = 1;
  uint32_t _formTickMs = FORM_TICK_MS_BASE;

  int  _paddleX = 0;
  bool _pBulletActive = false;
  int  _pBulletX = 0, _pBulletY = 0;
  bool _iBulletActive = false;
  int  _iBulletX = 0, _iBulletY = 0;

  uint16_t _score = 0;
  uint16_t _hiScore = 0;
  int      _lives = 3;
  bool     _gameOver = false;

  uint32_t _lastBulletTick = 0;
  uint32_t _lastFormTick = 0;
  uint32_t _rngState = 1;
};

SpaceInvadersApplet& spaceInvadersApplet();

}  // namespace mishmesh
