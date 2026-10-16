#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/applets/gps/WaypointStore.h>
#include <mishmesh/widgets/ConfirmDialog.h>
#include <mishmesh/widgets/ListMenu.h>

namespace mishmesh {

// Saved positions: "+ Mark here" then the waypoints, nearest first while there is a
// GPS fix (newest first otherwise). Select on a waypoint opens Navigate / Rename /
// Delete. The last row is the "Nearby alert" setting (Select cycles the radius). Utility-menu singleton; Back bubbles to the host.
class WaypointsApplet : public Applet {
public:
  WaypointsApplet();

  void onStart(AppletContext& ctx) override;
  void onForeground() override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  WaypointStore& store() { return _store; }
  // Test seams: row text as the list shows it (row 0 = "+ Mark here").
  int         rowCountForTest() const { return _rows.count(); }
  const char* rowLabelForTest(int i) const { return _rows.label(i); }
  const char* rowValueForTest(int i) const { return _rows.value(i); }

private:
  struct Rows : ListModel {
    WaypointsApplet* a = nullptr;
    int count() const override { return 2 + a->_orderN; }   // Mark here, waypoints, Nearby alert
    const char* label(int i) const override;
    uint16_t icon(int i) const override;
    const char* value(int i) const override;
  };
  friend struct Rows;

  void refreshPos();     // snapshot the live fix (once per render / rebuild)
  void rebuild();        // recompute the display order
  void markHere();
  static void onRenameDone(void* ctx, const char* text);

  AppServices*  _app = nullptr;
  AppletHost*   _host = nullptr;
  WaypointStore _store;
  Rows          _rows;
  ListMenu      _list;
  ListMenu      _actions;
  ConfirmDialog _confirm;
  bool          _acting = false;
  bool          _confirming = false;
  int           _sel = -1;                    // store index of the waypoint being acted on
  uint8_t       _order[WaypointStore::CAPACITY];
  int           _orderN = 0;
  bool          _havePos = false;
  int32_t       _lat = 0, _lon = 0;
  char          _renameBuf[WaypointStore::NAME_LEN + 1];
};

WaypointsApplet& waypointsApplet();

}  // namespace mishmesh
