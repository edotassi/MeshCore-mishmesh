#include <mishmesh/applets/settings/AdvertSettingsPanel.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/NameValidation.h>
#include <string.h>

namespace mishmesh {

void AdvertSettingsPanel::begin(AppletContext& ctx) {
  _app = ctx.app;
  _host = ctx.host;
  _model.bind(_app);
  _list.setRowHeight(14);
  _list.setModel(&_model);
  _list.resetSelection();
}

int AdvertSettingsPanel::renderBody(Canvas& c, int x, int y, int w, int h) {
  _list.draw(c, x, y, w, h);
  if (_editingAutoAdvert) { _stepper.draw(c, 0, 0, c.width(), c.height()); return 100; }
  return _list.needsAnimation() ? ListMenu::TICK_MS : 1000;
}

// Keypad confirm: validate then persist. Empty/invalid keeps the old name
// (mirrors ContactDetailApplet::onRenameDone). Save only - no advert is sent.
void AdvertSettingsPanel::onNameDone(void* ctx, const char* text) {
  auto* self = static_cast<AdvertSettingsPanel*>(ctx);
  if (!self->_app || !isValidNodeName(text)) return;
  self->_app->setNodeName(text);
}

static void autoAdvertStepLabel(int idx, char* out, uint16_t cap) {
  strncpy(out, autoAdvertLabel(idx), cap - 1);
  out[cap - 1] = 0;
}

bool AdvertSettingsPanel::onInput(InputEvent ev) {
  if (_editingAutoAdvert) {
    if (_stepper.onInput(ev)) {
      if (_stepper.result() == StepperResult::Confirmed && _app) {
        _app->setAutoAdvertIndex((uint8_t)_stepper.value());
      }
      if (_stepper.result() != StepperResult::None) {
        _editingAutoAdvert = false;
        _stepper.reset();
      }
    }
    return true;   // swallow everything while modal
  }

  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select && _app) {
    int i = _list.selected();
    if (i == Model::DeviceName) {
      const char* cur = _app->nodeName();
      strncpy(_nameBuf, cur ? cur : "", sizeof(_nameBuf) - 1);   // seed with current name
      _nameBuf[sizeof(_nameBuf) - 1] = 0;
      keypadApplet().configure(_nameBuf, sizeof(_nameBuf) - 1, "Device name",
                               &AdvertSettingsPanel::onNameDone, this);
      if (_host) _host->push(&keypadApplet());
    } else if (i == Model::SharePosition) {
      _app->setShareLocationInAdvert(!_app->shareLocationInAdvert());
    } else {   // AutoAdvert
      _stepper.configure("Auto advert", _app->autoAdvertIndex(), 0, AUTO_ADVERT_COUNT - 1,
                         autoAdvertStepLabel);
      _editingAutoAdvert = true;
    }
    return true;
  }
  return false;   // Back bubbles to the host
}

AdvertSettingsPanel& advertSettings() {
  static AdvertSettingsPanel s;
  return s;
}

}  // namespace mishmesh
