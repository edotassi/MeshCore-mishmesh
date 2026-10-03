#pragma once

#include <mishmesh/core/Applet.h>

namespace mishmesh {

class Canvas;

// Classic Snake: grid auto-sized from the panel at open time (capped at
// MAX_COLS x MAX_ROWS so the body buffer stays small), fixed-speed tick,
// walls kill (same as hitting your own tail). High score persists across
// sessions via AppletContext::storage. Real-time; Back pops, Select on the
// game-over screen restarts. App-menu singleton.
class SnakeApplet : public Applet {
public:
  SnakeApplet();

  bool wantsExclusive() const override { return true; }
  uint16_t repeatMask() const override { return 0; }   // one tap sets heading, no auto-repeat

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  int      lengthForTest()   const { return _len; }
  uint16_t scoreForTest()    const { return _score; }
  uint16_t hiScoreForTest()  const { return _hiScore; }
  bool     gameOverForTest() const { return _gameOver; }
  int      colsForTest()     const { return _cols; }
  int      headXForTest()    const { return _body[0] % _cols; }
  int      headYForTest()    const { return _body[0] / _cols; }
  void     setFoodForTest(int x, int y) { _food = (uint16_t)(y * _cols + x); }

private:
  enum class Dir : uint8_t { Up, Down, Left, Right };

  static const int MAX_COLS = 32;
  static const int MAX_ROWS = 32;
  static const int MAX_CELLS = MAX_COLS * MAX_ROWS;
  static const uint32_t TICK_MS = 150;

  void initBoard(Canvas& c);
  void resetGame();
  void step();
  void placeFood();
  bool occupied(uint16_t cellIdx, int fromIdx, int toIdxExclusive) const;
  uint32_t rngNext();
  static bool isOpposite(Dir a, Dir b);

  AppletStorage* _storage = nullptr;

  bool _boardReady = false;
  int  _cols = 0, _rows = 0, _cellPx = 4, _boardY = 0;

  uint16_t _body[MAX_CELLS];   // _body[0] = head .. _body[_len-1] = tail
  int      _len = 0;
  Dir      _dir = Dir::Right;
  Dir      _pendingDir = Dir::Right;
  uint16_t _food = 0;

  uint16_t _score = 0;
  uint16_t _hiScore = 0;
  bool     _gameOver = false;

  uint32_t _lastTick = 0;
  uint32_t _rngState = 1;
};

SnakeApplet& snakeApplet();

}  // namespace mishmesh
