#include <mishmesh/applets/BatteryHistoryApplet.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/BatteryHistory.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

BatteryHistoryApplet::BatteryHistoryApplet() : Applet("Battery") {}

void BatteryHistoryApplet::onStart(AppletContext& ctx) {
  _app = ctx.app;
}

int BatteryHistoryApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();

  BatteryStats st;
  const BatteryHistory* hist = nullptr;
  if (_app && _app->batteryHistory(st)) hist = st.history;

  if (!hist || !hist->primed()) {
    c.drawText(fontBody(), w / 2, h / 2 - c.fontHeight(fontBody()) / 2,
               "-- V", DisplayDriver::LIGHT, TextAlign::Center);
    return 1000;
  }

  uint16_t curMv = st.currentMv ? st.currentMv : hist->bucket(hist->bucketCount() - 1);
  uint16_t mn = hist->minMv(), mx = hist->maxMv();
  if (mn == 0) mn = curMv;
  if (mx == 0) mx = curMv;
  if (curMv < mn) mn = curMv;   // the live reading can be fresher than the last saved bucket
  if (curMv > mx) mx = curMv;

  int capH = c.lineHeight(fontCaption());
  int nh = c.fontHeight(fontNum());

  char cur_s[16];
  snprintf(cur_s, sizeof(cur_s), "%d.%02dV", curMv / 1000, (curMv % 1000) / 10);
  c.drawText(fontNum(), 3, 1, cur_s, DisplayDriver::LIGHT);

  char mm_s[32];
  snprintf(mm_s, sizeof(mm_s), "min %d.%02d  max %d.%02d",
           mn / 1000, (mn % 1000) / 10, mx / 1000, (mx % 1000) / 10);
  c.drawText(fontCaption(), 3, 2 + nh, mm_s, DisplayDriver::LIGHT);

  // 24h rolling bar chart, auto-scaled to the observed min/max - same shape
  // as NoiseFloorApplet's chart, just over BatteryHistory::BUCKETS buckets
  // and skipping any not-yet-sampled (0) buckets rather than drawing them.
  int chartY = 3 + nh + capH;
  int chartH = h - chartY - capH - 2;
  if (chartH < 4) chartH = 4;
  int gx = 3;
  int n = hist->bucketCount();
  int barW = (w - gx - 3) / n;
  if (barW < 1) barW = 1;
  int drawW = barW > 1 ? barW - 1 : 1;
  int baseline = chartY + chartH;

  int range = (int)mx - (int)mn;
  if (range < 1) range = 1;

  for (int i = 0; i < n; i++) {
    uint16_t v = hist->bucket(i);
    if (v == 0) continue;   // not sampled yet
    int barH = (int)(((int32_t)v - mn) * chartH / range);
    if (barH < 1) barH = 1;
    int x = gx + i * barW;
    c.fillRect(x, baseline - barH, drawW, barH, DisplayDriver::LIGHT);
  }
  c.fillRect(gx, baseline + 1, barW * n, 1, DisplayDriver::LIGHT);

  c.drawText(fontCaption(), gx, h - capH, "24h", DisplayDriver::LIGHT);
  return 1000;
}

BatteryHistoryApplet& batteryHistoryApplet() {
  static BatteryHistoryApplet a;
  return a;
}

MISHMESH_REGISTER_APPLET_ICON(&batteryHistoryApplet(), Placement::UtilityMenu, "Battery", 0,
                              (uint16_t)Icon::BatteryFull);

}  // namespace mishmesh
