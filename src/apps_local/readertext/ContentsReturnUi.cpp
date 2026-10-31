#include "ContentsReturnUi.h"

#include <BoardConfig.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>

#include "../ui/PixelMenuTheme.h"
#include "ContentsReturnScreen.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"

namespace {
const char* returnLabel(char (&label)[256]) {
  if (!BoardConfig::isX4Classic()) return tr(STR_READER_RETURN_POSITION);
  std::snprintf(label, sizeof(label), "%s: %s", tr(STR_QUEST_BACK), tr(STR_READER_RETURN_POSITION));
  return label;
}
}  // namespace

ContentsReturnUi::ContentsReturnUi(GfxRenderer& renderer) : UiAppHost(renderer), renderer(renderer) {
  resetUi();
  app.on(
      1,
      [](const freeink::ui::ActionEvent&, void* context) {
        auto& self = *static_cast<ContentsReturnUi*>(context);
        self.pending = true;
        self.app.clearTapFlash();
      },
      this);
  app.setScreen(&ContentsReturnUi::build, this);
}
void ContentsReturnUi::build(UiScreen& screen, void* context) {
  (void)context;
  char labelBuffer[256];
  const auto* label = returnLabel(labelBuffer);
  const auto box =
      readertext::returnBox(screen.target(), screen.device(), UITheme::getInstance().getStatusBarHeight(), label);
  readertext::returnControl(screen, box, label);
}
void ContentsReturnUi::render(bool mask) {
  pixelui::FontScope fonts(uiTarget, renderer);
  const auto device = uiTarget.deviceContext();
  if (mask) {
    char labelBuffer[256];
    const auto* label = returnLabel(labelBuffer);
    readertext::paintReturn(
        uiTarget, readertext::returnBox(uiTarget, device, UITheme::getInstance().getStatusBarHeight(), label), true,
        label);
    return;
  }
  if (builtWidth != device.width || builtHeight != device.height || builtOrientation != int(device.orientation)) {
    if (builtWidth) resetUi();
    builtWidth = device.width;
    builtHeight = device.height;
    builtOrientation = int(device.orientation);
    renderUi();
  } else {
    // Gray cleanup redraws the B/W pixels after the final paint. Keep the original reveal stamp.
    app.render();
  }
}
bool ContentsReturnUi::activated(const MappedInputManager& input) {
  routeTouch(input);
  return pending;
}
