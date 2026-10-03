#include <mishmesh/applets/NoiseFloorApplet.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

NoiseFloorApplet::NoiseFloorApplet() : Applet("Noise Floor") {}

void NoiseFloorApplet::onStart(AppletContext& ctx) {
  _app = ctx.app;
  _count = 0;
  _head = 0;
  _lastSampleAt = 0;
  _primed = false;
}

void NoiseFloorApplet::sample(uint32_t now) {
  if (!_app) return;
  int16_t v = _app->noiseFloorDbm();
  if (v == INT16_MIN) return;   // unsupported, or no reading yet
  if (_primed && now - _lastSampleAt < SAMPLE_INTERVAL_MS) return;
  _lastSampleAt = now;
  _primed = true;
  _samples[_head] = v;
  _head = (_head + 1) % SAMPLES;
  if (_count < SAMPLES) _count++;
}

int NoiseFloorApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  sample(c.now());

  if (!_primed) {
    c.drawText(fontBody(), w / 2, h / 2 - c.fontHeight(fontBody()) / 2,
               "-- dBm", DisplayDriver::LIGHT, TextAlign::Center);
    return 1000;
  }

  int16_t cur = _samples[(_head - 1 + SAMPLES) % SAMPLES];
  int16_t mn = cur, mx = cur;
  int oldestIdx = (_head - _count + SAMPLES) % SAMPLES;
  for (int i = 0; i < _count; i++) {
    int16_t v = _samples[(oldestIdx + i) % SAMPLES];
    if (v < mn) mn = v;
    if (v > mx) mx = v;
  }

  int capH = c.lineHeight(fontCaption());
  int nh = c.fontHeight(fontNum());

  char cur_s[16];
  snprintf(cur_s, sizeof(cur_s), "%d dBm", (int)cur);
  c.drawText(fontNum(), 3, 1, cur_s, DisplayDriver::LIGHT);

  char mm_s[28];
  snprintf(mm_s, sizeof(mm_s), "min %d  max %d", (int)mn, (int)mx);
  c.drawText(fontCaption(), 3, 2 + nh, mm_s, DisplayDriver::LIGHT);

  // 60s rolling bar chart, auto-scaled to the observed min/max (a flat
  // reading clamps the range to 1 so bars stay a visible sliver instead of
  // dividing by zero).
  int chartY = 3 + nh + capH;
  int chartH = h - chartY - capH - 2;
  if (chartH < 4) chartH = 4;
  int gx = 3;
  int barW = (w - gx - 3) / SAMPLES;
  if (barW < 1) barW = 1;
  int drawW = barW > 1 ? barW - 1 : 1;
  int baseline = chartY + chartH;

  int16_t range = mx - mn;
  if (range < 1) range = 1;

  for (int i = 0; i < _count; i++) {
    int16_t v = _samples[(oldestIdx + i) % SAMPLES];
    int barH = (int)((int32_t)(v - mn) * chartH / range);
    if (barH < 1) barH = 1;
    int x = gx + i * barW;
    c.fillRect(x, baseline - barH, drawW, barH, DisplayDriver::LIGHT);
  }
  c.fillRect(gx, baseline + 1, barW * SAMPLES, 1, DisplayDriver::LIGHT);

  c.drawText(fontCaption(), gx, h - capH, "60s", DisplayDriver::LIGHT);
  return 1000;
}

NoiseFloorApplet& noiseFloorApplet() {
  static NoiseFloorApplet a;
  return a;
}

MISHMESH_REGISTER_APPLET_ICON(&noiseFloorApplet(), Placement::UtilityMenu, "Noise Floor", 3,
                              (uint16_t)Icon::Radio);

}  // namespace mishmesh
