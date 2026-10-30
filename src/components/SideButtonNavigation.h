#pragma once

#include <BoardConfig.h>
#include <FreeInkUICore.h>

#include "MappedInputManager.h"
#include "activities/Activity.h"

namespace sidebuttons {
inline bool classic() { return BoardConfig::isX4Classic(); }
inline int frontStep(const MappedInputManager& input) {
  using Button = MappedInputManager::Button;
  const int direction = input.wasReleased(Button::Right) ? 1 : input.wasReleased(Button::Left) ? -1 : 0;
  return input.isNavDirectionSwapped() ? -direction : direction;
}
inline int shortStep(const MappedInputManager& input) {
  using Button = MappedInputManager::Button;
  if (classic()) {
    const int direction = input.wasReleased(Button::Down) ? 1 : input.wasReleased(Button::Up) ? -1 : 0;
    return input.isNavDirectionSwapped() ? -direction : direction;
  }
  return input.wasReleased(Button::NavNext) ? 1 : input.wasReleased(Button::NavPrevious) ? -1 : 0;
}
inline int longStep(const MappedInputManager& input) {
  using Button = MappedInputManager::Button;
  const auto next = classic() ? Button::Down : Button::NavNext;
  const auto previous = classic() ? Button::Up : Button::NavPrevious;
  const int sign = classic() && input.isNavDirectionSwapped() ? -1 : 1;
  if (input.wasLongPressed(next, 500)) return sign;
  if (input.wasLongPressed(previous, 500)) return -sign;
  return 0;
}
inline int swipeStep(const MappedInputManager& input) {
  switch (input.wasSwipe()) {
    case MappedInputManager::SwipeDir::Left:
    case MappedInputManager::SwipeDir::Up:
      return 1;
    case MappedInputManager::SwipeDir::Right:
    case MappedInputManager::SwipeDir::Down:
      return -1;
    case MappedInputManager::SwipeDir::None:
      return 0;
  }
  return 0;
}
inline int horizontalStep(const MappedInputManager& input) {
  const auto swipe = input.wasSwipe();
  return swipe == MappedInputManager::SwipeDir::Left ? 1 : swipe == MappedInputManager::SwipeDir::Right ? -1 : 0;
}
template <typename Buffer>
freeink::ui::ActionEvent pageSwipe(Buffer& buffer, const MappedInputManager& input) {
  const int direction = horizontalStep(input);
  if (!direction || !buffer.routable()) return {};
  const uint16_t mask = direction > 0 ? freeink::ui::InputNext : freeink::ui::InputPrev;
  for (size_t n = 0; n < buffer.count(); ++n) {
    if (!(buffer.data()[n].inputMask & mask)) continue;
    freeink::ui::InputSnapshot snapshot{};
    snapshot.next = direction > 0;
    snapshot.prev = direction < 0;
    return buffer.route(snapshot);
  }
  return {};
}
inline bool pending(const MappedInputManager& input) {
  using Button = MappedInputManager::Button;
  return input.wasReleased(Button::NavNext) || input.wasReleased(Button::NavPrevious) ||
         ((input.isPressed(Button::NavNext) || input.isPressed(Button::NavPrevious)) && input.getHeldTime() >= 500) ||
         input.wasReleased(Button::Confirm);
}

template <typename Buffer>
void focus(Buffer& buffer, int direction, Activity& owner) {
  buffer.setExplicitCoordinateNavigation(true);
  freeink::ui::InputSnapshot input{};
  input.focusNext = direction > 0;
  input.focusPrev = direction < 0;
  const auto before = buffer.focusPosition();
  for (int remaining = direction; remaining != 0; remaining += direction > 0 ? -1 : 1) buffer.route(input);
  if (before != buffer.focusPosition()) owner.requestUpdate();
}

template <typename Buffer>
freeink::ui::ActionEvent confirm(Buffer& buffer, Activity& owner) {
  buffer.setExplicitCoordinateNavigation(classic());
  freeink::ui::InputSnapshot input{};
  input.confirm = true;
  const auto before = buffer.focusPosition();
  const auto event = buffer.route(input);
  if (before != buffer.focusPosition()) owner.requestUpdate();
  return event;
}
template <typename Buffer>
bool cancelEditor(Buffer& buffer, const MappedInputManager& input, Activity& owner) {
  if (!classic() || !input.wasReleased(MappedInputManager::Button::Back) || !buffer.leaveCoordinateEditor())
    return false;
  owner.requestUpdate();
  return true;
}

template <typename Buffer>
bool moveEditor(Buffer& buffer, const MappedInputManager& input, Activity& owner) {
  if (!classic() || !buffer.coordinateEditing()) return false;
  using Button = MappedInputManager::Button;
  const int dx = input.wasReleased(Button::ScreenRight) ? 1 : input.wasReleased(Button::ScreenLeft) ? -1 : 0;
  const int dy = input.wasReleased(Button::ScreenDown) ? 1 : input.wasReleased(Button::ScreenUp) ? -1 : 0;
  if (!dx && !dy) return false;
  if (buffer.moveCoordinateCursor(dx, dy)) owner.requestUpdate();
  return true;
}
template <typename Buffer>
bool poll(Buffer& buffer, const MappedInputManager& input, int& x, int& y, bool enabled = true) {
  buffer.setExplicitCoordinateNavigation(classic());
  enabled = enabled || classic();
  if (input.wasScreenTapped(x, y)) {
    buffer.setFocusedIndex(-1);
    if (input.wasPressed(MappedInputManager::Button::NavNext))
      input.suppressNextRelease(MappedInputManager::Button::NavNext);
    if (input.wasPressed(MappedInputManager::Button::NavPrevious))
      input.suppressNextRelease(MappedInputManager::Button::NavPrevious);
    return true;
  }
  x = y = -1;
  if (!enabled || !pending(input) || !buffer.routable()) return false;
  freeink::ui::Rect rect;
  const bool enteringEditor = classic() && buffer.coordinateFocused() && !buffer.coordinateEditing();
  if (input.wasReleased(MappedInputManager::Button::Confirm) && !enteringEditor && buffer.buttonFocusRect(rect)) {
    x = rect.x + rect.width / 2;
    y = rect.y + rect.height / 2;
  }
  return true;
}

// poll() admits navigation and activation. Geometry handlers require the
// coordinates supplied only by a touch or a confirmed coordinate target.
inline bool hasActivation(int x, int y) { return x >= 0 && y >= 0; }

inline void append(const MappedInputManager& input, freeink::ui::InputSnapshot& snapshot) {
  using Button = MappedInputManager::Button;
  if ((snapshot.touchPressed || snapshot.touchReleased) && snapshot.touchX >= 0 && snapshot.touchY >= 0) {
    if (input.wasPressed(Button::NavNext)) input.suppressNextRelease(Button::NavNext);
    if (input.wasPressed(Button::NavPrevious)) input.suppressNextRelease(Button::NavPrevious);
    return;
  }
  if (pending(input)) snapshot.touchReleased = false;
  const int front = classic() ? frontStep(input) : 0;
  const int direction = front ? front : shortStep(input);
  snapshot.focusNext = direction > 0;
  snapshot.focusPrev = direction < 0;
  snapshot.next = false;
  snapshot.prev = false;
  snapshot.confirm = snapshot.confirm || input.wasReleased(Button::Confirm);
}

template <typename Buffer>
freeink::ui::ActionEvent route(Buffer& buffer, freeink::ui::InputSnapshot snapshot, const MappedInputManager& input,
                               Activity& owner, bool enabled = true) {
  enabled = enabled || classic();
  int touchX = 0, touchY = 0;
  const bool touch = input.wasScreenTapped(touchX, touchY);
  if (enabled && !touch && input.wasReleased(MappedInputManager::Button::Confirm) && buffer.hasButtonFocus()) {
    snapshot.touchReleased = false;
    snapshot.confirm = true;
  }
  if (enabled) append(input, snapshot);
  const auto before = buffer.focusPosition();
  const auto event = buffer.route(snapshot);
  if (before != buffer.focusPosition()) owner.requestUpdate();
  return event;
}
}  // namespace sidebuttons
