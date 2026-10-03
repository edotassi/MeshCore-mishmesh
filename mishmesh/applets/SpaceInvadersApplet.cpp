#include <mishmesh/applets/SpaceInvadersApplet.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

static const char* HISCORE_KEY = "invaders_hi";

SpaceInvadersApplet::SpaceInvadersApplet() : Applet("Space Invaders") {}

void SpaceInvadersApplet::onStart(AppletContext& ctx) {
  _storage = ctx.storage;
  _hiScore = 0;
  if (_storage) {
    uint8_t buf[2];
    if (_storage->load(HISCORE_KEY, buf, sizeof(buf)) == sizeof(buf))
      _hiScore = (uint16_t)(buf[0] | (buf[1] << 8));
  }
  _boardReady = false;   // board geometry depends on the Canvas, sized on first render
}

static uint32_t rngNextInvaders(uint32_t& state) {
  state = state * 1664525u + 1013904223u;
  return state;
}

void SpaceInvadersApplet::initBoard(Canvas& c) {
  _w = c.width();
  _h = c.height();
  int capH = c.lineHeight(fontCaption());
  _boardTop = capH + 2;

  _invaderW = _w / (INV_COLS + MARGIN_COLS);
  if (_invaderW < 4) _invaderW = 4;
  _invaderH = 5;
  _paddleW = _w / 8;
  if (_paddleW < 10) _paddleW = 10;
  _paddleY = _h - 4;

  _rngState = c.now() | 1u;
  _boardReady = true;
  resetGame();
}

void SpaceInvadersApplet::spawnInvaders() {
  for (int r = 0; r < INV_ROWS; r++)
    for (int col = 0; col < INV_COLS; col++) _invaders[r][col] = true;
  _formX = (_w - INV_COLS * _invaderW) / 2;   // centred, with room to sweep both ways
  _formY = _boardTop;
  _formDir = 1;
}

void SpaceInvadersApplet::resetGame() {
  _score = 0;
  _lives = 3;
  _gameOver = false;
  _formTickMs = FORM_TICK_MS_BASE;
  _paddleX = (_w - _paddleW) / 2;
  _pBulletActive = false;
  _iBulletActive = false;
  spawnInvaders();
}

int SpaceInvadersApplet::countAliveInvaders() const {
  int n = 0;
  for (int r = 0; r < INV_ROWS; r++)
    for (int col = 0; col < INV_COLS; col++)
      if (_invaders[r][col]) n++;
  return n;
}

void SpaceInvadersApplet::killAllInvadersExceptForTest(int row, int col) {
  for (int r = 0; r < INV_ROWS; r++)
    for (int c = 0; c < INV_COLS; c++) _invaders[r][c] = (r == row && c == col);
}

void SpaceInvadersApplet::endGame() {
  _gameOver = true;
  if (_storage && _score >= _hiScore) {
    uint8_t buf[2] = {(uint8_t)(_hiScore & 0xFF), (uint8_t)(_hiScore >> 8)};
    _storage->save(HISCORE_KEY, buf, sizeof(buf));
  }
}

void SpaceInvadersApplet::maybeFireInvader() {
  if (_iBulletActive) return;
  if (rngNextInvaders(_rngState) % 6 != 0) return;
  for (int tries = 0; tries < 20; tries++) {
    int r = (int)(rngNextInvaders(_rngState) % INV_ROWS);
    int col = (int)(rngNextInvaders(_rngState) % INV_COLS);
    if (_invaders[r][col]) {
      _iBulletX = _formX + col * _invaderW + _invaderW / 2;
      _iBulletY = _formY + r * _invaderH + _invaderH;
      _iBulletActive = true;
      return;
    }
  }
}

void SpaceInvadersApplet::stepFormation() {
  int next = _formX + _formDir * FORM_STEP_PX;
  bool hitRight = _formDir > 0 && (next + INV_COLS * _invaderW) > _w;
  bool hitLeft = _formDir < 0 && next < 0;
  if (hitRight || hitLeft) {
    _formDir = -_formDir;
    _formY += _invaderH;
  } else {
    _formX = next;
  }

  if (_formY + INV_ROWS * _invaderH >= _paddleY) { endGame(); return; }

  maybeFireInvader();

  int destroyed = INV_ROWS * INV_COLS - countAliveInvaders();
  uint32_t ms = FORM_TICK_MS_BASE - (uint32_t)destroyed * 10;
  _formTickMs = ms < FORM_TICK_MS_MIN ? FORM_TICK_MS_MIN : ms;
}

void SpaceInvadersApplet::stepBullets() {
  if (_pBulletActive) {
    _pBulletY -= BULLET_STEP_PX;
    if (_pBulletY < _boardTop) {
      _pBulletActive = false;
    } else {
      int row = (_pBulletY - _formY) / _invaderH;
      int col = (_pBulletX - _formX) / _invaderW;
      if (row >= 0 && row < INV_ROWS && col >= 0 && col < INV_COLS && _invaders[row][col]) {
        _invaders[row][col] = false;
        _score = (uint16_t)(_score + (INV_ROWS - row) * 10);
        if (_score > _hiScore) _hiScore = _score;
        _pBulletActive = false;
        if (countAliveInvaders() == 0) spawnInvaders();   // next wave; score/lives carry over
      }
    }
  }

  if (_iBulletActive) {
    _iBulletY += BULLET_STEP_PX;
    if (_iBulletY > _h) {
      _iBulletActive = false;
    } else if (_iBulletY >= _paddleY && _iBulletX >= _paddleX && _iBulletX <= _paddleX + _paddleW) {
      _iBulletActive = false;
      _lives--;
      if (_lives <= 0) endGame();
    }
  }
}

bool SpaceInvadersApplet::onInput(InputEvent ev) {
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
      if (!_pBulletActive) {
        _pBulletActive = true;
        _pBulletX = _paddleX + _paddleW / 2;
        _pBulletY = _paddleY - 2;
      }
      return true;
    default:
      return false;   // Back bubbles -> host pops
  }
}

int SpaceInvadersApplet::onRender(Canvas& c) {
  if (!_boardReady) initBoard(c);

  char hdr[40];
  snprintf(hdr, sizeof(hdr), "Score %u  Hi %u  Lives %d", (unsigned)_score,
           (unsigned)_hiScore, _lives);
  c.drawText(fontCaption(), 2, 1, hdr, DisplayDriver::LIGHT);

  if (!_gameOver) {
    uint32_t now = c.now();
    if (now - _lastBulletTick >= BULLET_TICK_MS) {
      _lastBulletTick = now;
      stepBullets();
    }
    if (!_gameOver && now - _lastFormTick >= _formTickMs) {
      _lastFormTick = now;
      stepFormation();
    }
  }

  for (int r = 0; r < INV_ROWS; r++)
    for (int col = 0; col < INV_COLS; col++) {
      if (!_invaders[r][col]) continue;
      c.fillRect(_formX + col * _invaderW, _formY + r * _invaderH, _invaderW - 1,
                 _invaderH - 1, DisplayDriver::LIGHT);
    }

  c.fillRect(_paddleX, _paddleY, _paddleW, 2, DisplayDriver::LIGHT);
  if (_pBulletActive) c.fillRect(_pBulletX, _pBulletY, 1, 3, DisplayDriver::LIGHT);
  if (_iBulletActive) c.fillRect(_iBulletX, _iBulletY, 1, 3, DisplayDriver::LIGHT);

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

  return 33;   // ~30fps while exclusive; bullets/formation are paced by their own ticks
}

SpaceInvadersApplet& spaceInvadersApplet() {
  static SpaceInvadersApplet a;
  return a;
}

MISHMESH_REGISTER_APPLET_ICON(&spaceInvadersApplet(), Placement::GamesMenu, "Space Invaders", 4,
                              (uint16_t)Icon::Bell);   // squat glyph reads as an invader

}  // namespace mishmesh
