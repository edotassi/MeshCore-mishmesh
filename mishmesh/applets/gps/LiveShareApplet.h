#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/widgets/ListMenu.h>

namespace mishmesh {

// Control screen for LiveShare: Target (pick a conversation, channels included),
// Duration, Interval, Start/Stop. Utility-menu singleton; Back bubbles (or closes
// the picker).
class LiveShareApplet : public Applet {
public:
  LiveShareApplet();
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  const char* rowLabelForTest(int i) const { return _rows.label(i); }
  const char* rowValueForTest(int i) const { return _rows.value(i); }
  bool pickingForTest() const { return _picking; }

private:
  struct Rows : ListModel {
    int count() const override { return 4; }
    const char* label(int i) const override;
    const char* value(int i) const override;
  };
  struct Convos : ListModel {
    MessagesService* svc = nullptr;
    int count() const override { return svc ? svc->convoCount() : 0; }
    const char* label(int i) const override;
    uint16_t icon(int i) const override;
  };

  AppServices*     _app = nullptr;
  AppletHost*      _host = nullptr;
  MessagesService* _svc = nullptr;
  Rows     _rows;
  Convos   _convos;
  ListMenu _list;
  ListMenu _pick;
  bool     _picking = false;
  uint32_t _nowMs = 0;       // last render time, for the remaining-time line
};

LiveShareApplet& liveShareApplet();

}  // namespace mishmesh
