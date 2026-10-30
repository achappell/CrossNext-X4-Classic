#pragma once
// Host adapter for the parser's panel geometry and board touch capability.
namespace BoardConfig {
struct Board {
  int displayWidth;
  int displayHeight;
};
inline constexpr Board ACTIVE{800, 480};
inline bool touchAvailable = true;
inline bool hasTouch() { return touchAvailable; }
}  // namespace BoardConfig
