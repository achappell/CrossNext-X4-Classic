#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace sectionnav {
// Main-loop-owned commands retain button gestures while the renderer is busy.
// Fixed storage is reused for the activity lifetime; no hot-path allocation.
class Input {
 public:
  enum class Kind : uint8_t { Step, Section, Back, Confirm, Focus };
  struct Event {
    Kind kind;
    int16_t amount;
  };
  void begin(bool held) { inheritedHold = held; }
  bool push(Kind kind, int amount = 1) {
    if (count && (kind == Kind::Step || kind == Kind::Section || kind == Kind::Focus)) {
      auto& last = events[(first + count - 1) % capacity];
      if (last.kind == kind && ((last.amount > 0) == (amount > 0))) {
        last.amount = int16_t(std::clamp(int(last.amount) + amount, -32767, 32767));
        return true;
      }
    }
    if (count == capacity) return false;
    events[(first + count++) % capacity] = {kind, int16_t(amount)};
    return true;
  }
  bool pop(Event& event, int maxSteps = 32767) {
    if (!count) return false;
    event = events[first];
    if ((event.kind == Kind::Step || event.kind == Kind::Focus) && std::abs(int(event.amount)) > maxSteps) {
      event.amount = int16_t(event.amount > 0 ? maxSteps : -maxSteps);
      events[first].amount -= event.amount;
    } else {
      first = (first + 1) % capacity;
      --count;
    }
    return true;
  }
  bool backPending() const { return count && events[first].kind == Kind::Back; }
  bool hasPending() const { return count != 0; }
  void clear() { first = count = 0; }
  template <typename Buttons>
  bool capture(const Buttons& input, uint32_t now, bool buttonOnly = false, bool swapped = false) {
    using Button = typename Buttons::Button;
    const int sign = swapped ? -1 : 1;
    const auto next = buttonOnly ? Button::Down : Button::NavNext;
    const auto previous = buttonOnly ? Button::Up : Button::NavPrevious;
    if (input.wasReleased(Button::Back)) {
      clear();
      return push(Kind::Back);
    }
    if (input.wasReleased(Button::Confirm)) return push(Kind::Confirm);
    if (buttonOnly) {
      const int direction = input.wasReleased(Button::Right) ? 1 : input.wasReleased(Button::Left) ? -1 : 0;
      if (direction) return push(Kind::Focus, direction * sign);
    }
    const bool nextHeld = input.isPressed(next), prevHeld = input.isPressed(previous);
    if (inheritedHold) {
      if (!nextHeld && !prevHeld) inheritedHold = false;
      return true;
    }
    const bool nextLong = input.wasLongPressed(next, 350);
    const bool prevLong = input.wasLongPressed(previous, 350);
    if (nextLong || prevLong ||
        ((nextHeld || prevHeld) && input.getHeldTime() >= 350 && uint32_t(now - repeatedAt) >= 300)) {
      repeatedAt = now;
      return push(Kind::Section, (nextLong || (!prevLong && nextHeld) ? 1 : -1) * sign);
    }
    if (input.wasReleased(next)) return push(Kind::Step, sign);
    if (input.wasReleased(previous)) return push(Kind::Step, -sign);
    return true;
  }

 private:
  static constexpr unsigned capacity = 32;
  Event events[capacity]{};
  unsigned first = 0, count = 0;
  uint32_t repeatedAt = 0;
  bool inheritedHold = false;
};
}  // namespace sectionnav
