#pragma once

#include <mishmesh/core/Applet.h>

namespace mishmesh {

class Canvas;

// Classic Tetris. Board is a fixed 10 columns wide (the standard width -
// piece shapes/spacing depend on it); rows adapt to the panel (12..20) so it
// still plays on a short landscape OLED as well as a tall portrait e-ink.
// Gravity speeds up as lines clear. High score persists across sessions via
// AppletStorage. Controls: Left/Right move, Down soft-drops (repeats while
// held), Up rotates, Select hard-drops (or restarts after game over).
// Real-time; Back pops. GamesMenu entry (see GamesApplet).
class TetrisApplet : public Applet {
public:
  TetrisApplet();

  bool wantsExclusive() const override { return true; }
  uint16_t repeatMask() const override {
    return maskBit(InputEvent::NavLeft) | maskBit(InputEvent::NavRight) |
           maskBit(InputEvent::NavDown);
  }

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  // Test hooks.
  int      rowsForTest()    const { return _rows; }
  int      colsForTest()    const { return COLS; }
  uint16_t scoreForTest()   const { return _score; }
  uint16_t hiScoreForTest() const { return _hiScore; }
  bool     gameOverForTest() const { return _gameOver; }
  int      pieceForTest()   const { return _piece; }
  int      rotForTest()     const { return _rot; }
  int      pieceXForTest()  const { return _px; }
  int      pieceYForTest()  const { return _py; }
  uint8_t  cellForTest(int r, int c) const { return _board[r][c]; }
  void     setCellForTest(int r, int c, uint8_t v) { _board[r][c] = v; }
  void     setPieceForTest(int piece, int rot, int x, int y) {
    _piece = piece; _rot = rot; _px = x; _py = y;
  }
  void     tickForTest() { step(); }

private:
  static const int COLS = 10;
  static const int MAX_ROWS = 20;
  static const int MIN_ROWS = 12;

  void initBoard(Canvas& c);
  void resetGame();
  void spawnPiece();
  bool collides(int piece, int rot, int px, int py) const;
  void lockPiece();
  int  clearLines();
  void step();

  AppletStorage* _storage = nullptr;

  bool _boardReady = false;
  int  _cellPx = 4, _boardY = 0, _rows = MIN_ROWS;

  uint8_t _board[MAX_ROWS][COLS];   // 0 = empty, else 1..7 = locked piece id + 1

  int _piece = 0, _rot = 0, _px = 0, _py = 0;   // current falling piece

  uint16_t _score = 0;
  uint16_t _hiScore = 0;
  uint32_t _lines = 0;
  bool     _gameOver = false;

  uint32_t _lastTick = 0;
  uint32_t _tickMs = 700;
  uint32_t _rngState = 1;
};

TetrisApplet& tetrisApplet();

}  // namespace mishmesh
