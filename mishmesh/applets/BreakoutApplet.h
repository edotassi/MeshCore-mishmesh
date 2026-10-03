#pragma once

#include <mishmesh/core/Applet.h>

namespace mishmesh {

class Canvas;

// Classic Breakout: paddle + ball + a brick grid, adaptive to the panel at
// open time. Ball physics run on a fast fixed-point tick (sub-pixel
// resolution, SCALE units/px) independent of the paddle, which moves per
// input for responsiveness. Clearing every brick refills the grid and
// speeds the ball up slightly. 3 lives; high score persists across
// sessions via AppletStorage. Controls: Left/Right move the paddle
// (repeats while held), Select launches the ball (or restarts after game
// over). Real-time; Back pops. GamesMenu entry (see GamesApplet).
class BreakoutApplet : public Applet {
public:
  BreakoutApplet();

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
  bool     launchedForTest() const { return _launched; }
  int      paddleXForTest()  const { return _paddleX; }
  int      paddleWForTest()  const { return _paddleW; }
  int      ballXForTest()    const { return _bx / SCALE; }
  int      ballYForTest()    const { return _by / SCALE; }
  int      bricksRemainingForTest() const;
  int      boardTopForTest() const { return _boardTop; }
  int      brickWForTest() const { return _brickW; }
  int      brickHForTest() const { return _brickH; }
  int      paddleYForTest() const { return _paddleY; }
  int      ballSpeedForTest() const { return _ballSpeed; }
  int      ballDxForTest() const { return _bdx; }
  int      ballDyForTest() const { return _bdy; }
  void     setPaddleXForTest(int x) { _paddleX = x; }
  void     setBallForTest(int x, int y, int dx, int dy) {
    _bx = x * SCALE; _by = y * SCALE; _bdx = dx; _bdy = dy; _launched = true;
  }
  void     killAllBricksExceptForTest(int row, int col);
  void     tickForTest() { stepBall(); }

private:
  static const int SCALE = 16;
  static const int BRICK_COLS = 8;
  static const int BRICK_ROWS = 4;
  static const int BALL_SIZE = 3;
  static const uint32_t TICK_MS = 20;

  void initBoard(Canvas& c);
  void resetGame();
  void spawnBricks();
  void launchBall();
  void stepBall();

  AppletStorage* _storage = nullptr;

  bool _boardReady = false;
  int  _w = 0, _h = 0, _boardTop = 0;
  int  _brickW = 8, _brickH = 5;
  int  _paddleW = 20, _paddleY = 0;

  bool _bricks[BRICK_ROWS][BRICK_COLS];

  int  _paddleX = 0;
  int  _bx = 0, _by = 0;     // ball position, fixed-point (SCALE units/px)
  int  _bdx = 0, _bdy = 0;   // ball velocity, fixed-point units per tick
  int  _ballSpeed = 24;      // magnitude used for dy on a bounce; dx varies with paddle hit angle
  bool _launched = false;

  uint16_t _score = 0;
  uint16_t _hiScore = 0;
  int      _lives = 3;
  bool     _gameOver = false;

  uint32_t _lastTick = 0;
  uint32_t _rngState = 1;
};

BreakoutApplet& breakoutApplet();

}  // namespace mishmesh
