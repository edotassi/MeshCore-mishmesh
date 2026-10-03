#include <mishmesh/applets/BreakoutApplet.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

static const char* HISCORE_KEY = "breakout_hi";
static const int MAX_BALL_SPEED = 40;

BreakoutApplet::BreakoutApplet() : Applet("Breakout") {}

void BreakoutApplet::onStart(AppletContext& ctx) {
  _storage = ctx.storage;
  _hiScore = 0;
  if (_storage) {
    uint8_t buf[2];
    if (_storage->load(HISCORE_KEY, buf, sizeof(buf)) == sizeof(buf))
      _hiScore = (uint16_t)(buf[0] | (buf[1] << 8));
  }
  _boardReady = false;   // board geometry depends on the Canvas, sized on first render
}

static uint32_t rngNextBreakout(uint32_t& state) {
  state = state * 1664525u + 1013904223u;
  return state;
}

void BreakoutApplet::initBoard(Canvas& c) {
  _w = c.width();
  _h = c.height();
  int capH = c.lineHeight(fontCaption());
  _boardTop = capH + 2;

  _brickW = _w / BRICK_COLS;
  if (_brickW < 4) _brickW = 4;
  _brickH = 5;
  _paddleW = _w / 6;
  if (_paddleW < 12) _paddleW = 12;
  _paddleY = _h - 4;

  _rngState = c.now() | 1u;
  _boardReady = true;
  resetGame();
}

void BreakoutApplet::spawnBricks() {
  for (int r = 0; r < BRICK_ROWS; r++)
    for (int col = 0; col < BRICK_COLS; col++) _bricks[r][col] = true;
}

void BreakoutApplet::resetGame() {
  _score = 0;
  _lives = 3;
  _gameOver = false;
  _ballSpeed = 24;
  _paddleX = (_w - _paddleW) / 2;
  _launched = false;
  spawnBricks();
}

void BreakoutApplet::launchBall() {
  _bx = (_paddleX + _paddleW / 2) * SCALE;
  _by = (_paddleY - BALL_SIZE - 1) * SCALE;
  _bdx = (rngNextBreakout(_rngState) & 1) ? _ballSpeed / 2 : -_ballSpeed / 2;
  _bdy = -_ballSpeed;
  _launched = true;
}

int BreakoutApplet::bricksRemainingForTest() const {
  int n = 0;
  for (int r = 0; r < BRICK_ROWS; r++)
    for (int col = 0; col < BRICK_COLS; col++)
      if (_bricks[r][col]) n++;
  return n;
}

void BreakoutApplet::killAllBricksExceptForTest(int row, int col) {
  for (int r = 0; r < BRICK_ROWS; r++)
    for (int c = 0; c < BRICK_COLS; c++) _bricks[r][c] = (r == row && c == col);
}

void BreakoutApplet::stepBall() {
  int nx = _bx + _bdx;
  int ny = _by + _bdy;

  int maxX = (_w - BALL_SIZE) * SCALE;
  if (nx < 0) { nx = 0; _bdx = -_bdx; }
  else if (nx > maxX) { nx = maxX; _bdx = -_bdx; }

  if (ny < _boardTop * SCALE) { ny = _boardTop * SCALE; _bdy = -_bdy; }

  // Brick collision: test the ball's centre against the grid cell it would
  // occupy next. Simple vertical-only reflection - accurate enough at this
  // scale, and how most small Breakout clones handle it.
  int cx = nx / SCALE + BALL_SIZE / 2;
  int cy = ny / SCALE + BALL_SIZE / 2;
  int brickRow = (cy - _boardTop) / _brickH;
  int brickCol = cx / _brickW;
  if (brickRow >= 0 && brickRow < BRICK_ROWS && brickCol >= 0 && brickCol < BRICK_COLS &&
      _bricks[brickRow][brickCol]) {
    _bricks[brickRow][brickCol] = false;
    _score = (uint16_t)(_score + (BRICK_ROWS - brickRow) * 10);   // higher rows worth more
    if (_score > _hiScore) _hiScore = _score;
    _bdy = -_bdy;
    ny = _by;   // stay put this tick rather than tunnel into where the brick was

    if (bricksRemainingForTest() == 0) {
      spawnBricks();
      if (_ballSpeed < MAX_BALL_SPEED) _ballSpeed += 2;
      _launched = false;
      return;
    }
  }

  // Paddle collision: only while descending into the paddle's row.
  int ballBottom = ny / SCALE + BALL_SIZE;
  if (_bdy > 0 && ballBottom >= _paddleY && (_by / SCALE + BALL_SIZE) < _paddleY) {
    int ballCentreX = nx / SCALE + BALL_SIZE / 2;
    if (ballCentreX >= _paddleX && ballCentreX <= _paddleX + _paddleW) {
      int offset = ballCentreX - (_paddleX + _paddleW / 2);   // signed, px from paddle centre
      int halfW = _paddleW / 2;
      _bdx = halfW > 0 ? (offset * _ballSpeed) / halfW : 0;
      _bdy = -_ballSpeed;
      ny = (_paddleY - BALL_SIZE) * SCALE;
    }
  }

  // Missed the paddle: lose a life, or end the run.
  if (ny / SCALE > _h) {
    _lives--;
    if (_lives <= 0) {
      _gameOver = true;
      if (_storage && _score >= _hiScore) {
        uint8_t buf[2] = {(uint8_t)(_hiScore & 0xFF), (uint8_t)(_hiScore >> 8)};
        _storage->save(HISCORE_KEY, buf, sizeof(buf));
      }
    } else {
      _launched = false;
    }
    return;
  }

  _bx = nx;
  _by = ny;
}

bool BreakoutApplet::onInput(InputEvent ev) {
  if (_gameOver) {
    if (ev == InputEvent::Select) { resetGame(); return true; }
    return false;   // Back bubbles -> host pops
  }
  switch (ev) {
    case InputEvent::NavLeft:
      _paddleX -= 4;
      if (_paddleX < 0) _paddleX = 0;
      return true;
    case InputEvent::NavRight:
      _paddleX += 4;
      if (_paddleX > _w - _paddleW) _paddleX = _w - _paddleW;
      return true;
    case InputEvent::Select:
      if (!_launched) launchBall();
      return true;
    default:
      return false;   // Back bubbles -> host pops
  }
}

int BreakoutApplet::onRender(Canvas& c) {
  if (!_boardReady) initBoard(c);

  char hdr[40];
  snprintf(hdr, sizeof(hdr), "Score %u  Hi %u  Lives %d", (unsigned)_score,
           (unsigned)_hiScore, _lives);
  c.drawText(fontCaption(), 2, 1, hdr, DisplayDriver::LIGHT);

  if (!_launched) {
    // Ball rests on the paddle, following it, until Select launches it.
    _bx = (_paddleX + _paddleW / 2 - BALL_SIZE / 2) * SCALE;
    _by = (_paddleY - BALL_SIZE - 1) * SCALE;
  } else if (!_gameOver) {
    uint32_t now = c.now();
    if (now - _lastTick >= TICK_MS) {
      _lastTick = now;
      stepBall();
    }
  }

  for (int r = 0; r < BRICK_ROWS; r++)
    for (int col = 0; col < BRICK_COLS; col++) {
      if (!_bricks[r][col]) continue;
      c.fillRect(col * _brickW, _boardTop + r * _brickH, _brickW - 1, _brickH - 1,
                 DisplayDriver::LIGHT);
    }

  c.fillRect(_paddleX, _paddleY, _paddleW, 2, DisplayDriver::LIGHT);
  c.fillRect(_bx / SCALE, _by / SCALE, BALL_SIZE, BALL_SIZE, DisplayDriver::LIGHT);

  if (!_launched && !_gameOver) {
    c.drawText(fontCaption(), _w / 2, _boardTop + BRICK_ROWS * _brickH + 4,
               "Select: launch", DisplayDriver::LIGHT, TextAlign::Center);
  }

  if (_gameOver) {
    c.fillStipple(0, _boardTop, _w, _h - _boardTop, DisplayDriver::LIGHT);
    c.drawText(fontBody(), _w / 2, _h / 2 - c.fontHeight(fontBody()) - 4, "Game Over",
               DisplayDriver::LIGHT, TextAlign::Center);
    char sc[24];
    snprintf(sc, sizeof(sc), "Score %u", (unsigned)_score);
    c.drawText(fontBody(), _w / 2, _h / 2 + 2, sc, DisplayDriver::LIGHT, TextAlign::Center);
    c.drawText(fontCaption(), _w / 2, _h - c.lineHeight(fontCaption()), "Select: restart",
               DisplayDriver::LIGHT, TextAlign::Center);
  }

  return 33;   // ~30fps while exclusive; ball physics are paced by TICK_MS
}

BreakoutApplet& breakoutApplet() {
  static BreakoutApplet a;
  return a;
}

MISHMESH_REGISTER_APPLET_ICON(&breakoutApplet(), Placement::GamesMenu, "Breakout", 3,
                              (uint16_t)Icon::Plus);   // small centred mark reads as the ball

}  // namespace mishmesh
