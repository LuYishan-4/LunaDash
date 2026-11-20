#include "compositor/input/PointerCursor.hpp"

#include "core/filesystem/FileIo.hpp"

#include <algorithm>
#include <cmath>
#include <drm_fourcc.h>

namespace LunaDash {
namespace {

using namespace std::chrono_literals;


constexpr auto kShakeWindow = 800ms;

constexpr double kShakeRatio = 3.5;

constexpr double kMinimumSpan = 50.0;

constexpr double kMaximumGrowth = 4.0;
constexpr auto kHold = 400ms;
// The growth moves on this spring (in units of the growth).
constexpr SceneSpring kGrowthSpring{350.0, 0.85, 0.005, 0.05};

int sign(double value) { return (value > 0) - (value < 0); }

wlr_xcursor_image *firstImage(wlr_xcursor_manager *manager, const char *name) {
  auto *xcursor = wlr_xcursor_manager_get_xcursor(manager, name, 1);
  if (!xcursor)
    xcursor = wlr_xcursor_manager_get_xcursor(manager, "default", 1);
  return xcursor && xcursor->image_count > 0 && xcursor->images[0]->width > 0
             ? xcursor->images[0]
             : nullptr;
}

// An xcursor image's pixels as a buffer. wlroots 0.19 and later have
// wlr_xcursor_image_get_buffer(); this works on every supported version.
struct ImageBuffer {
  wlr_buffer base;
  const wlr_xcursor_image *image;
};

ImageBuffer *imageBuffer(wlr_buffer *buffer) {
  return reinterpret_cast<ImageBuffer *>(buffer);
}

const wlr_buffer_impl imageBufferImpl = {
    .destroy = [](wlr_buffer *buffer) { delete imageBuffer(buffer); },
    .get_dmabuf = nullptr,
    .get_shm = nullptr,
    .begin_data_ptr_access =
        [](wlr_buffer *buffer, uint32_t flags, void **data, uint32_t *format,
           size_t *stride) {
          if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE)
            return false;
          const auto *image = imageBuffer(buffer)->image;
          *data = image->buffer;
          *format = DRM_FORMAT_ARGB8888;
          *stride = static_cast<size_t>(image->width) * 4;
          return true;
        },
    .end_data_ptr_access = [](wlr_buffer *) {},
};

} // namespace

PointerCursor::PointerCursor(wlr_cursor *cursor, wlr_xcursor_manager *manager,
                             uint32_t size, std::function<void()> requestFrame)
    : cursor_(cursor), manager_(manager), requestFrame_(std::move(requestFrame)),
      enabled_(environmentInt("LUNADASH_SHAKE_CURSOR", 1) != 0) {
  if (!enabled_)
    return;
  wlr_xcursor_manager_load(manager_, 1);
  large_ = wlr_xcursor_manager_create(
      manager_->name, static_cast<uint32_t>(size * kMaximumGrowth));
  if (large_ && !wlr_xcursor_manager_load(large_, 1)) {
    wlr_xcursor_manager_destroy(large_);
    large_ = nullptr;
  }
}

PointerCursor::~PointerCursor() {
  // The cursor lets go of the large image before its pixels go.
  if (grown_)
    wlr_cursor_set_xcursor(cursor_, manager_, name_.c_str());
  if (largeBuffer_)
    wlr_buffer_drop(largeBuffer_);
  if (large_)
    wlr_xcursor_manager_destroy(large_);
}

void PointerCursor::showXcursor(const char *name) {
  Templates::detachListener(surfaceDestroy_);
  clientSurface_ = false;
  surface_ = nullptr;
  name_ = name;
  apply();
}

void PointerCursor::showSurface(wlr_surface *surface, int32_t hotspotX,
                                int32_t hotspotY) {
  Templates::detachListener(surfaceDestroy_);
  clientSurface_ = true;
  surface_ = surface;
  hotspotX_ = hotspotX;
  hotspotY_ = hotspotY;
  if (surface)
    listen<&PointerCursor::onSurfaceDestroy>(&surface->events.destroy,
                                             surfaceDestroy_);
  apply();
}

void PointerCursor::onSurfaceDestroy() {
  Templates::detachListener(surfaceDestroy_);
  surface_ = nullptr;

  if (grown_)
    apply();
}

void PointerCursor::moved() {
  if (!large_)
    return;
  const Sample sample{Clock::now(), cursor_->x, cursor_->y};
  if (!strokes_.empty() && strokes_.back().x == sample.x &&
      strokes_.back().y == sample.y)
    return;
  if (strokes_.size() >= 2) {
    auto &last = strokes_.back();
    const auto &before = strokes_[strokes_.size() - 2];
    if (sign(last.x - before.x) == sign(sample.x - last.x) &&
        sign(last.y - before.y) == sign(sample.y - last.y))
      last = sample;
    else
      strokes_.push_back(sample);
  } else {
    strokes_.push_back(sample);
  }
  while (sample.time - strokes_.front().time > kShakeWindow)
    strokes_.pop_front();

  double path = 0;
  double left = sample.x, right = sample.x, top = sample.y, bottom = sample.y;
  for (std::size_t i = 0; i < strokes_.size(); ++i) {
    const auto &at = strokes_[i];
    left = std::min(left, at.x);
    right = std::max(right, at.x);
    top = std::min(top, at.y);
    bottom = std::max(bottom, at.y);
    if (i > 0)
      path += std::hypot(at.x - strokes_[i - 1].x, at.y - strokes_[i - 1].y);
  }
  const double span = std::hypot(right - left, bottom - top);
  if (span < kMinimumSpan || path / span < kShakeRatio)
    return;
  // Starting from rest: the first step counts from now.
  if (growth_ == 1.0 && target_ == 1.0)
    lastStep_ = sample.time;
  lastShake_ = sample.time;
  target_ = std::max(target_, std::min(kMaximumGrowth, 2.0 + path / span - kShakeRatio));
  requestFrame_();
}

bool PointerCursor::advance() {
  if (growth_ == 1.0 && target_ == 1.0)
    return false;
  const auto now = Clock::now();
  const double seconds =
      std::clamp(std::chrono::duration<double>(now - lastStep_).count(), 0.0, 0.1);
  lastStep_ = now;
  if (target_ > 1.0 && now - lastShake_ > kHold)
    target_ = 1.0;
  double offset = growth_ - target_;
  stepSceneSpring(offset, growthSpeed_, kGrowthSpring, seconds);
  const bool settled = std::abs(offset) < kGrowthSpring.restDistance &&
                       std::abs(growthSpeed_) < kGrowthSpring.restSpeed;
  growth_ = settled ? target_ : std::max(1.0, target_ + offset);
  if (settled)
    growthSpeed_ = 0.0;
  // A hidden cursor was already hidden by showSurface().
  if (!clientSurface_ || surface_)
    apply();
  return growth_ != 1.0 || target_ != 1.0;
}

void PointerCursor::apply() {
  // A cursor a client hides stays hidden, however it is shaken.
  if (growth_ > 1.0 && large_ && (!clientSurface_ || surface_)) {
    // A client's own image cannot be redrawn larger: the arrow stands in.
    const char *name = clientSurface_ ? "default" : name_.c_str();
    auto *large = firstImage(large_, name);
    auto *base = firstImage(manager_, name);
    if (large && base) {
      // The buffer's scale makes it `growth_` times the theme's own size.
      const double scale = large->width / (base->width * growth_);
      if (large != largeBufferImage_) {
        if (largeBuffer_)
          wlr_buffer_drop(largeBuffer_);
        auto *buffer = new ImageBuffer{{}, large};
        wlr_buffer_init(&buffer->base, &imageBufferImpl,
                        static_cast<int>(large->width),
                        static_cast<int>(large->height));
        largeBuffer_ = &buffer->base;
        largeBufferImage_ = large;
      }
      // wlroots scales the image by the buffer's scale but takes the hotspot
      // as it is, in logical pixels: it is scaled here to stay on the pointer.
      wlr_cursor_set_buffer(
          cursor_, largeBuffer_,
          static_cast<int32_t>(std::lround(large->hotspot_x / scale)),
          static_cast<int32_t>(std::lround(large->hotspot_y / scale)),
          static_cast<float>(scale));
      grown_ = true;
      return;
    }
  }
  if (clientSurface_)
    wlr_cursor_set_surface(cursor_, surface_, hotspotX_, hotspotY_);
  else
    wlr_cursor_set_xcursor(cursor_, manager_, name_.c_str());
  grown_ = false;
}

} // namespace LunaDash
