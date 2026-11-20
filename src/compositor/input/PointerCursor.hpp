#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <functional>
#include <string>

#include "compositor/window/animation/SceneAnimation.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "core/templates/LunaClint.hpp"
#include "core/templates/WaylandSlot.hpp"

namespace LunaDash {


class PointerCursor final : public Templates::LunaClint<PointerCursor> {
public:
  PointerCursor(wlr_cursor *cursor, wlr_xcursor_manager *manager, uint32_t size,
                std::function<void()> requestFrame);
  ~PointerCursor();
  PointerCursor(const PointerCursor &) = delete;
  PointerCursor &operator=(const PointerCursor &) = delete;

  void showXcursor(const char *name);

  void showSurface(wlr_surface *surface, int32_t hotspotX, int32_t hotspotY);

  void moved();

  bool advance();

private:
  using Clock = std::chrono::steady_clock;
  struct Sample {
    Clock::time_point time;
    double x = 0;
    double y = 0;
  };

  void onSurfaceDestroy();
  // Shows the cursor asked for, at the current growth.
  void apply();

  wlr_cursor *cursor_;
  wlr_xcursor_manager *manager_;

  wlr_xcursor_manager *large_ = nullptr;

  wlr_buffer *largeBuffer_ = nullptr;
  wlr_xcursor_image *largeBufferImage_ = nullptr;
  std::function<void()> requestFrame_;
  const bool enabled_;


  std::string name_ = "default";
  bool clientSurface_ = false;
  wlr_surface *surface_ = nullptr;
  int32_t hotspotX_ = 0;
  int32_t hotspotY_ = 0;
  Templates::WaylandSlot<PointerCursor> surfaceDestroy_;

  std::deque<Sample> strokes_;
  Clock::time_point lastShake_{};
  Clock::time_point lastStep_{};
  double growth_ = 1.0;
  double growthSpeed_ = 0.0;
  double target_ = 1.0;
  bool grown_ = false;
};

} // namespace LunaDash
