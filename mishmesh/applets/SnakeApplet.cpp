#include <mishmesh/applets/SnakeApplet.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

static const char* HISCORE_KEY = "snake_hi";

SnakeApplet::SnakeApplet() : Applet("Snake") {}

void SnakeApplet::onStart(AppletContext& ctx) {
  _storage = ctx.storage;
  _hiScore = 0;
  if (_storage) {
    uint8_t buf[2];
    if (_storage->load(HISCORE_KEY, buf, sizeof(buf)) == sizeof(buf))
      _hiScore = (uint16_t)(buf[0] | (buf[1] << 8));
  }
  _boardReady = false;   // grid geometry depends on the Canvas, sized on first render
}

uint32_t SnakeApplet::rngNext() {
  _rngState = _rngState * 1664525u + 1013904223u;   // small LCG, no libc rand dependency
  return _rngState;
}

void SnakeApplet::initBoard(Canvas& c) {
  int w = c.width(), h = c.height();
  int capH = c.lineHeight(fontCaption());
  _boardY = capH + 2;

  // Cell size scales up so the grid never exceeds MAX_COLS x MAX_ROWS
  // (bounds the body buffer), while still filling whatever panel this is.
  int cellFromW = (w + MAX_COLS - 1) / MAX_COLS;
  int cellFromH = (h - _boardY + MAX_ROWS - 1) / MAX_ROWS;
  _cellPx = cellFromW > cellFromH ? cellFromW : cellFromH;
  if (_cellPx < 4) _cellPx = 4;

  _cols = w / _cellPx;
  _rows = (h - _boardY) / _cellPx;
  if (_cols > MAX_COLS) _cols = MAX_COLS;
  if (_rows > MAX_ROWS) _rows = MAX_ROWS;
  if (_cols < 4) _cols = 4;
  if (_rows < 4) _rows = 4;

  _rngState = c.now() | 1u;
  _boardReady = true;
  resetGame();
}

void SnakeApplet::resetGame() {
  _len = 3;
  int cy = _rows / 2;
  int cx = _cols / 2;
  if (cx < 2) cx = 2;
  // body[0] is the head; lay the starting body out to the left of it.
  _body[0] = (uint16_t)(cy * _cols + cx);
  _body[1] = (uint16_t)(cy * _cols + (cx - 1));
  _body[2] = (uint16_t)(cy * _cols + (cx - 2));
  _dir = Dir::Right;
  _pendingDir = Dir::Right;
  _score = 0;
  _gameOver = false;
  placeFood();
}

bool SnakeApplet::occupied(uint16_t cellIdx, int fromIdx, int toIdxExclusive) const {
  for (int i = fromIdx; i < toIdxExclusive; i++)
    if (_body[i] == cellIdx) return true;
  return false;
}

void SnakeApplet::placeFood() {
  int total = _cols * _rows;
  for (int tries = 0; tries < 200; tries++) {
    uint16_t idx = (uint16_t)(rngNext() % (uint32_t)total);
    if (!occupied(idx, 0, _len)) { _food = idx; return; }
  }
  for (uint16_t idx = 0; idx < (uint16_t)total; idx++) {
    if (!occupied(idx, 0, _len)) { _food = idx; return; }
  }
  // Board completely full: nowhere left to put food - leave it as-is (a win,
  // effectively; the next step() will just find no food to eat).
}

void SnakeApplet::step() {
  _dir = _pendingDir;

  int hx = _body[0] % _cols, hy = _body[0] / _cols;
  switch (_dir) {
    case Dir::Up:    hy--; break;
    case Dir::Down:  hy++; break;
    case Dir::Left:  hx--; break;
    case Dir::Right: hx++; break;
  }
  if (hx < 0 || hx >= _cols || hy < 0 || hy >= _rows) { _gameOver = true; }
  else {
    uint16_t newHead = (uint16_t)(hy * _cols + hx);
    bool eating = (newHead == _food);
    // The tail cell vacates this tick unless we're growing, so it's not a collision.
    int checkTo = eating ? _len : _len - 1;
    if (occupied(newHead, 0, checkTo)) {
      _gameOver = true;
    } else {
      if (eating && _len < MAX_CELLS) {
        _len++;
        _score++;
        if (_score > _hiScore) _hiScore = _score;
      }
      for (int i = _len - 1; i > 0; i--) _body[i] = _body[i - 1];
      _body[0] = newHead;
      if (eating) placeFood();
    }
  }

  if (_gameOver && _storage && _score >= _hiScore) {
    uint8_t buf[2] = {(uint8_t)(_hiScore & 0xFF), (uint8_t)(_hiScore >> 8)};
    _storage->save(HISCORE_KEY, buf, sizeof(buf));
  }
}

bool SnakeApplet::isOpposite(Dir a, Dir b) {
  return (a == Dir::Up && b == Dir::Down) || (a == Dir::Down && b == Dir::Up) ||
         (a == Dir::Left && b == Dir::Right) || (a == Dir::Right && b == Dir::Left);
}

bool SnakeApplet::onInput(InputEvent ev) {
  if (_gameOver) {
    if (ev == InputEvent::Select) { resetGame(); return true; }
    return false;   // Back bubbles -> host pops
  }
  Dir want;
  switch (ev) {
    case InputEvent::NavUp:    want = Dir::Up;    break;
    case InputEvent::NavDown:  want = Dir::Down;  break;
    case InputEvent::NavLeft:  want = Dir::Left;  break;
    case InputEvent::NavRight: want = Dir::Right; break;
    default: return false;   // Back bubbles -> host pops
  }
  if (!isOpposite(want, _dir)) _pendingDir = want;
  return true;
}

int SnakeApplet::onRender(Canvas& c) {
  if (!_boardReady) initBoard(c);

  char hdr[32];
  snprintf(hdr, sizeof(hdr), "Score %u  Hi %u", (unsigned)_score, (unsigned)_hiScore);
  c.drawText(fontCaption(), 2, 1, hdr, DisplayDriver::LIGHT);

  if (!_gameOver) {
    uint32_t now = c.now();
    if (now - _lastTick >= TICK_MS) {
      _lastTick = now;
      step();
    }
  }

  int cell = _cellPx > 1 ? _cellPx - 1 : 1;
  for (int i = 0; i < _len; i++) {
    int x = (_body[i] % _cols) * _cellPx;
    int y = _boardY + (_body[i] / _cols) * _cellPx;
    c.fillRect(x, y, cell, cell, DisplayDriver::LIGHT);
  }
  int fx = (_food % _cols) * _cellPx;
  int fy = _boardY + (_food / _cols) * _cellPx;
  c.drawRect(fx, fy, cell, cell, DisplayDriver::LIGHT);   // outline: distinct from the solid body

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

  return 33;   // ~30fps while exclusive; movement itself is paced by TICK_MS
}

SnakeApplet& snakeApplet() {
  static SnakeApplet a;
  return a;
}

MISHMESH_REGISTER_APPLET_ICON(&snakeApplet(), Placement::GamesMenu, "Snake", 1,
                              (uint16_t)Icon::Reload);   // curved loop reads as a curling snake

}  // namespace mishmesh
