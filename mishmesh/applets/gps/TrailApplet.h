#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/widgets/ConfirmDialog.h>
#include <mishmesh/widgets/ListMenu.h>

namespace mishmesh {

// GPS trail control: status (state, points, length, duration), Start/Stop
// recording and Clear trail. Recording itself runs in the background
// (TrailRecorder, ticked by the adapter); this screen only steers it.
// Recording keeps the GPS powered, so it costs battery. Utility-menu singleton.
class TrailApplet : public Applet {
public:
  TrailApplet();

  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  const char* stateLineForTest() const { return _stateLine; }
  const char* actionLabelForTest(int i) const { return _rows.label(i); }

private:
  struct Rows : ListModel {
    int count() const override { return 2; }
    const char* label(int i) const override;
  };

  AppServices*  _app = nullptr;
  AppletHost*   _host = nullptr;
  Rows          _rows;
  ListMenu      _list;
  ConfirmDialog _confirm;
  bool          _confirming = false;
  char          _stateLine[24] = {0};
};

TrailApplet& trailApplet();

}  // namespace mishmesh
