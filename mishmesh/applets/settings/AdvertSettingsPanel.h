#pragma once

#include <mishmesh/core/AutoAdvert.h>
#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StepperDialog.h>

namespace mishmesh {

class AppletHost;

// Advert settings: editable device (advert) name + a "Share position" toggle
// controlling whether self-adverts include this node's location. Source of
// truth is AppServices (persisted by the adapter).
class AdvertSettingsPanel : public SettingsPanel {
public:
  const char* title() const override { return "Advert"; }
  void begin(AppletContext& ctx) override;
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;
  bool modalActive() const override { return _editingAutoAdvert; }

  // Test seams (mirror TimeSettingsPanel::rowCountForTest).
  int rowCountForTest() const { return _model.count(); }
  const char* labelForTest(int i) const { return _model.label(i); }
  const char* valueForTest(int i) const { return _model.value(i); }

private:
  class Model : public ListModel {
    AppServices* _app = nullptr;
  public:
    enum Row : int { DeviceName, SharePosition, AutoAdvert, ROW_COUNT };
    void bind(AppServices* app) { _app = app; }
    int count() const override { return ROW_COUNT; }
    const char* label(int i) const override {
      return i == DeviceName ? "Device name" : i == SharePosition ? "Share position" : "Auto advert";
    }
    bool isToggle(int i) const override { return i == SharePosition; }
    bool toggleState(int i) const override {
      return i == SharePosition && _app && _app->shareLocationInAdvert();
    }
    const char* value(int i) const override {
      if (i == DeviceName) return _app ? _app->nodeName() : nullptr;
      if (i == AutoAdvert) return autoAdvertLabel(_app ? _app->autoAdvertIndex() : 0);
      return nullptr;
    }
  } _model;

  AppServices* _app = nullptr;
  AppletHost*  _host = nullptr;
  char _nameBuf[32];                       // keypad scratch; applied only if valid
  static void onNameDone(void* ctx, const char* text);   // keypad confirm
  ListMenu      _list;
  StepperDialog _stepper;
  bool          _editingAutoAdvert = false;
};

AdvertSettingsPanel& advertSettings();   // shared singleton

}  // namespace mishmesh
