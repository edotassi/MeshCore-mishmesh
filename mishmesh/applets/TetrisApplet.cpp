#include <mishmesh/applets/TetrisApplet.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

static const char* HISCORE_KEY = "tetris_hi";

// Each tetromino's four cells at rotation 0, within a 4x4 bounding box
// (row-major, 0..3). Other rotations are derived at runtime by rotating each
// cell (r,c) -> (c, 3-r) around the box, repeated `rot` times - the standard
// trick for an NxN box rotated clockwise.
struct Pt { int8_t r, c; };
static const Pt PIECE_CELLS[7][4] = {
  {{1, 0}, {1, 1}, {1, 2}, {1, 3}},   // I
  {{1, 1}, {1, 2}, {2, 1}, {2, 2}},   // O (rotation-invariant under the transform below)
  {{1, 0}, {1, 1}, {1, 2}, {2, 1}},   // T
  {{1, 1}, {1, 2}, {2, 0}, {2, 1}},   // S
  {{1, 0}, {1, 1}, {2, 1}, {2, 2}},   // Z
  {{1, 0}, {2, 0}, {2, 1}, {2, 2}},   // J
  {{1, 2}, {2, 0}, {2, 1}, {2, 2}},   // L
};

static Pt rotatedCell(int piece, int rot, int cellIdx) {
  Pt p = PIECE_CELLS[piece][cellIdx];
  for (int i = 0; i < (rot & 3); i++) {
    int8_t nr = p.c;
    int8_t nc = 3 - p.r;
    p.r = nr;
    p.c = nc;
  }
  return p;
}

// Lines-cleared-at-once -> score, classic Tetris scoring.
static const uint16_t LINE_SCORE[5] = {0, 100, 300, 500, 800};

TetrisApplet::TetrisApplet() : Applet("Tetris") {}

void TetrisApplet::onStart(AppletContext& ctx) {
  _storage = ctx.storage;
  _hiScore = 0;
  if (_storage) {
    uint8_t buf[2];
    if (_storage->load(HISCORE_KEY, buf, sizeof(buf)) == sizeof(buf))
      _hiScore = (uint16_t)(buf[0] | (buf[1] << 8));
  }
  _boardReady = false;   // grid geometry depends on the Canvas, sized on first render
}

static uint32_t rngNextTetris(uint32_t& state) {
  state = state * 1664525u + 1013904223u;
  return state;
}

void TetrisApplet::initBoard(Canvas& c) {
  int w = c.width(), h = c.height();
  int capH = c.lineHeight(fontCaption());
  _boardY = capH + 2;
  int availH = h - _boardY;

  int cellFromW = w / COLS;
  int cellFromH = availH / MAX_ROWS;
  _cellPx = cellFromW < cellFromH ? cellFromW : cellFromH;
  if (_cellPx < 3) _cellPx = 3;

  _rows = availH / _cellPx;
  if (_rows > MAX_ROWS) _rows = MAX_ROWS;
  if (_rows < MIN_ROWS) _rows = MIN_ROWS;   // a very short panel just clips off-screen rows

  _rngState = c.now() | 1u;
  _boardReady = true;
  resetGame();
}

void TetrisApplet::resetGame() {
  for (int r = 0; r < MAX_ROWS; r++)
    for (int col = 0; col < COLS; col++) _board[r][col] = 0;
  _score = 0;
  _lines = 0;
  _tickMs = 700;
  _gameOver = false;
  spawnPiece();
}

void TetrisApplet::spawnPiece() {
  _piece = (int)(rngNextTetris(_rngState) % 7);
  _rot = 0;
  _px = COLS / 2 - 2;
  _py = -1;   // pieces are defined at local rows 1-2, so this lands them at board rows 0-1
  if (collides(_piece, _rot, _px, _py)) _gameOver = true;
}

bool TetrisApplet::collides(int piece, int rot, int px, int py) const {
  for (int i = 0; i < 4; i++) {
    Pt cell = rotatedCell(piece, rot, i);
    int r = py + cell.r, cc = px + cell.c;
    if (cc < 0 || cc >= COLS) return true;
    if (r >= _rows) return true;
    if (r >= 0 && _board[r][cc]) return true;
    // r < 0: still above the visible board - no collision against the stack.
  }
  return false;
}

void TetrisApplet::lockPiece() {
  for (int i = 0; i < 4; i++) {
    Pt cell = rotatedCell(_piece, _rot, i);
    int r = _py + cell.r, cc = _px + cell.c;
    if (r >= 0 && r < _rows) _board[r][cc] = (uint8_t)(_piece + 1);
  }
}

int TetrisApplet::clearLines() {
  int cleared = 0;
  for (int r = _rows - 1; r >= 0; r--) {
    bool full = true;
    for (int col = 0; col < COLS; col++)
      if (!_board[r][col]) { full = false; break; }
    if (!full) continue;
    for (int rr = r; rr > 0; rr--)
      for (int col = 0; col < COLS; col++) _board[rr][col] = _board[rr - 1][col];
    for (int col = 0; col < COLS; col++) _board[0][col] = 0;
    cleared++;
    r++;   // re-check this row index, now holding what was above it
  }
  return cleared;
}

void TetrisApplet::step() {
  if (!collides(_piece, _rot, _px, _py + 1)) {
    _py++;
    return;
  }
  lockPiece();
  int cleared = clearLines();
  if (cleared > 0) {
    _score = (uint16_t)(_score + LINE_SCORE[cleared]);
    if (_score > _hiScore) _hiScore = _score;
    _lines += (uint32_t)cleared;
    uint32_t level = _lines / 10;
    uint32_t ms = 700 - level * 40;
    _tickMs = ms < 150 ? 150 : ms;
  }
  spawnPiece();
  if (_gameOver && _storage && _score >= _hiScore) {
    uint8_t buf[2] = {(uint8_t)(_hiScore & 0xFF), (uint8_t)(_hiScore >> 8)};
    _storage->save(HISCORE_KEY, buf, sizeof(buf));
  }
}

bool TetrisApplet::onInput(InputEvent ev) {
  if (_gameOver) {
    if (ev == InputEvent::Select) { resetGame(); return true; }
    return false;   // Back bubbles -> host pops
  }
  switch (ev) {
    case InputEvent::NavLeft:
      if (!collides(_piece, _rot, _px - 1, _py)) _px--;
      return true;
    case InputEvent::NavRight:
      if (!collides(_piece, _rot, _px + 1, _py)) _px++;
      return true;
    case InputEvent::NavDown:
      if (!collides(_piece, _rot, _px, _py + 1)) { _py++; _score++; }
      return true;
    case InputEvent::NavUp: {
      int newRot = (_rot + 1) & 3;
      static const int KICKS[] = {0, -1, 1, -2, 2};
      for (int k = 0; k < 5; k++) {
        if (!collides(_piece, newRot, _px + KICKS[k], _py)) {
          _rot = newRot;
          _px += KICKS[k];
          break;
        }
      }
      return true;
    }
    case InputEvent::Select:
      while (!collides(_piece, _rot, _px, _py + 1)) { _py++; _score += 2; }
      step();   // lock, clear, spawn immediately rather than waiting for the next tick
      return true;
    default:
      return false;   // Back bubbles -> host pops
  }
}

int TetrisApplet::onRender(Canvas& c) {
  if (!_boardReady) initBoard(c);

  char hdr[32];
  snprintf(hdr, sizeof(hdr), "Score %u  Hi %u", (unsigned)_score, (unsigned)_hiScore);
  c.drawText(fontCaption(), 2, 1, hdr, DisplayDriver::LIGHT);

  if (!_gameOver) {
    uint32_t now = c.now();
    if (now - _lastTick >= _tickMs) {
      _lastTick = now;
      step();
    }
  }

  int cell = _cellPx > 1 ? _cellPx - 1 : 1;
  for (int r = 0; r < _rows; r++)
    for (int col = 0; col < COLS; col++) {
      if (!_board[r][col]) continue;
      c.fillRect(col * _cellPx, _boardY + r * _cellPx, cell, cell, DisplayDriver::LIGHT);
    }
  if (!_gameOver) {
    for (int i = 0; i < 4; i++) {
      Pt cp = rotatedCell(_piece, _rot, i);
      int r = _py + cp.r, col = _px + cp.c;
      if (r < 0) continue;
      c.fillRect(col * _cellPx, _boardY + r * _cellPx, cell, cell, DisplayDriver::LIGHT);
    }
  }

  if (_gameOver) {
    int w = c.width(), h = c.height();
    c.fillStipple(0, _boardY, w, h - _boardY, DisplayDriver::LIGHT);
    c.drawText(fontBody(), w / 2, h / 2 - c.fontHeight(fontBody()) - 4, "Game Over",
               DisplayDriver::LIGHT, TextAlign::Center);
    char sc[24];
    snprintf(sc, sizeof(sc), "Score %u", (unsigned)_score);
    c.drawText(fontBody(), w / 2, h / 2 + 2, sc, DisplayDriver::LIGHT, TextAlign::Center);
    c.drawText(fontCaption(), w / 2, h - c.lineHeight(fontCaption()), "Select: restart",
               DisplayDriver::LIGHT, TextAlign::Center);
  }

  return 33;   // ~30fps while exclusive; movement itself is paced by _tickMs
}

TetrisApplet& tetrisApplet() {
  static TetrisApplet a;
  return a;
}

MISHMESH_REGISTER_APPLET_ICON(&tetrisApplet(), Placement::GamesMenu, "Tetris", 2,
                              (uint16_t)Icon::Chip);   // blocky square reads as a tetromino

}  // namespace mishmesh
