#pragma once
#include "../ui/PixelMenu.h"

namespace readertext {
inline freeink::ui::Rect returnBox(const freeink::ui::DrawTarget& target, const freeink::ui::DeviceContext& device,
                                   int statusHeight, const char* label = tr(STR_READER_RETURN_POSITION)) {
  const auto safe = device.safeRect();
  freeink::ui::TextStyle style;
  style.font = pixelui::kSmall;
  const int width = std::min(int(safe.width) - 24,
                             int(target.measureText(style.font, label, style).width) + 36);
  return freeink::ui::makeRect(safe.right() - width - 12, safe.bottom() - statusHeight - 56, width, 44);
}
inline void paintReturn(freeink::ui::DrawTarget& target, freeink::ui::Rect box, bool grayMask,
                        const char* label = tr(STR_READER_RETURN_POSITION)) {
  namespace fui = freeink::ui;
  // Zero mask bits preserve the B/W control, including its white lettering.
  target.fill(box, fui::Paint::solid(fui::Color::Black),
              grayMask              ? 0
              : chrome::lightHeader ? lightui::kControlRadius
                                    : 0);
  if (!grayMask) {
    pixelui::text(target, box, label, pixelui::kSmall, fui::Color::White, fui::TextAlign::Center);
  }
}
template <typename Screen>
void returnControl(Screen& screen, freeink::ui::Rect box, const char* label = tr(STR_READER_RETURN_POSITION)) {
  screen.frame().hit(box, 1, 0, freeink::ui::InputTouch);
  paintReturn(screen.target(), box, false, label);
}
}  // namespace readertext
