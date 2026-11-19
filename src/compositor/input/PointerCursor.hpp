#pragma once

// The standard headers go first: WlrootsHeaders.hpp renames C++ keywords
// while it includes the C headers.
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

// The cursor image: the xcursor name or client surface asked for, drawn
// larger while the pointer is shaken so it is easy to find again. Shaking
// is on unless the compositor starts with LUNADASH_SHAKE_CURSOR=0
// (`export LUNADASH_SHAKE_CURSOR=0` before the session starts).
class PointerCursor final : public Templates::LunaClint<PointerCursor> {
public:
  // `manager` is the compositor's theme at `size`; `requestFrame` asks every
  // output for a frame, which then calls advance().
  PointerCursor(wlr_cursor *cursor, wlr_xcursor_manager *manager, uint32_t size,
                std::function<void()> requestFrame);
  ~PointerCursor();
  PointerCursor(const PointerCursor &) = delete;
  PointerCursor &operator=(const PointerCursor &) = delete;

  void showXcursor(const char *name);
  // A client's cursor surface; null hides the cursor.
  void showSurface(wlr_surface *surface, int32_t hotspotX, int32_t hotspotY);
  // After every pointer motion: notices shaking.
  void moved();
  // Once per output frame: steps the growth; true while it still changes.
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
  // The same theme at the largest growth, so the grown cursor stays sharp.
  wlr_xcursor_manager *large_ = nullptr;
  std::function<void()> requestFrame_;
  const bool enabled_;

  // What was asked for: an xcursor name, or a client surface (null hides).
  std::string name_ = "default";
  bool clientSurface_ = false;
  wlr_surface *surface_ = nullptr;
  int32_t hotspotX_ = 0;
  int32_t hotspotY_ = 0;
  Templates::WaylandSlot<PointerCursor> surfaceDestroy_;

  // Recent motion; consecutive moves in one direction count as one stroke.
  std::deque<Sample> strokes_;
  Clock::time_point lastShake_{};
  Clock::time_point lastStep_{};
  // How many times larger the cursor is drawn, and where it is heading.
  double growth_ = 1.0;
  double growthSpeed_ = 0.0;
  double target_ = 1.0;
  bool grown_ = false;
};

} // namespace LunaDash
