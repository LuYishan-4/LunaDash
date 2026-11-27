#include "compositor/session/SessionSplash.hpp"

#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "compositor/window/WindowTuning.hpp"
#include "compositor/window/animation/SceneAnimation.hpp"
#include "core/process/LoopTask.hpp"

#include <drm_fourcc.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace LunaDash {
namespace {
constexpr int kMarkSize = 112;
// Decoded at twice the drawn size, for scaled outputs.
constexpr int kMarkPixels = kMarkSize * 2;
constexpr int kTrackWidth = 160;
constexpr int kTrackHeight = 3;
constexpr int kRunnerWidth = 48;
constexpr int kTrackGap = 56;
constexpr double kRunnerSeconds = 1.6;
constexpr double kMarkFadeSeconds = 0.3;
constexpr std::array<float, 3> kBackground{0.043f, 0.055f, 0.075f};
constexpr std::array<float, 3> kRunner{0.612f, 0.796f, 0.984f};

// Premultiplied ARGB8888 pixels in memory.
struct PixelBuffer {
  wlr_buffer base;
  std::vector<std::uint32_t> pixels;
};

const wlr_buffer_impl pixelBufferImpl = {
    .destroy =
        [](wlr_buffer *buffer) {
          delete reinterpret_cast<PixelBuffer *>(buffer);
        },
    .get_dmabuf = nullptr,
    .get_shm = nullptr,
    .begin_data_ptr_access =
        [](wlr_buffer *buffer, uint32_t flags, void **data, uint32_t *format,
           size_t *stride) {
          if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE)
            return false;
          auto *pixels = reinterpret_cast<PixelBuffer *>(buffer);
          *data = pixels->pixels.data();
          *format = DRM_FORMAT_ARGB8888;
          *stride = static_cast<size_t>(buffer->width) * 4;
          return true;
        },
    .end_data_ptr_access = [](wlr_buffer *) {},
};

// On the task's thread.
std::vector<std::uint32_t> decodeMark(const std::filesystem::path &path) {
  GdkPixbuf *image = gdk_pixbuf_new_from_file_at_size(path.c_str(), kMarkPixels,
                                                      kMarkPixels, nullptr);
  if (!image)
    return {};
  std::vector<std::uint32_t> pixels;
  if (gdk_pixbuf_get_width(image) == kMarkPixels &&
      gdk_pixbuf_get_height(image) == kMarkPixels &&
      gdk_pixbuf_get_n_channels(image) == 4) {
    pixels.resize(std::size_t{kMarkPixels} * kMarkPixels);
    const int stride = gdk_pixbuf_get_rowstride(image);
    const guchar *rows = gdk_pixbuf_read_pixels(image);
    for (int y = 0; y < kMarkPixels; ++y)
      for (int x = 0; x < kMarkPixels; ++x) {
        const guchar *rgba = rows + y * stride + x * 4;
        const std::uint32_t alpha = rgba[3];
        const auto premultiply = [alpha](std::uint32_t value) {
          return (value * alpha + 127) / 255;
        };
        pixels[std::size_t(y) * kMarkPixels + x] =
            alpha << 24 | premultiply(rgba[0]) << 16 |
            premultiply(rgba[1]) << 8 | premultiply(rgba[2]);
      }
  }
  g_object_unref(image);
  return pixels;
}

double easeInOut(double t) {
  return t < 0.5 ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3) / 2;
}

void setColor(wlr_scene_rect *rect, const std::array<float, 3> &rgb,
              float alpha) {
  const float color[4] = {rgb[0] * alpha, rgb[1] * alpha, rgb[2] * alpha,
                          alpha};
  wlr_scene_rect_set_color(rect, color);
}
} // namespace

struct SessionSplash::Impl {
  wlr_scene_tree *tree = nullptr;
  wlr_scene_rect *background = nullptr;
  wlr_scene_buffer *mark = nullptr;
  wlr_scene_rect *track = nullptr;
  wlr_scene_rect *runner = nullptr;
  std::unique_ptr<LoopTask> decode;
  std::vector<std::uint32_t> markPixels;
  std::uint64_t startNs = 0;
  std::uint64_t lastNs = 0;
  // Set by the first frame after the mark is decoded.
  std::uint64_t markShownNs = 0;
  bool markReady = false;
  double fade = 1.0;
  double fadeVelocity = 0.0;
  bool finished = false;

  void showMark() {
    decode.reset();
    if (markPixels.empty())
      return;
    auto *buffer = new PixelBuffer{{}, std::move(markPixels)};
    wlr_buffer_init(&buffer->base, &pixelBufferImpl, kMarkPixels, kMarkPixels);
    wlr_scene_buffer_set_buffer(mark, &buffer->base);
    wlr_buffer_drop(&buffer->base);
    wlr_scene_buffer_set_dest_size(mark, kMarkSize, kMarkSize);
    markReady = true;
  }
};

SessionSplash::SessionSplash(wlr_scene_tree *parent, wl_event_loop *loop,
                             const std::filesystem::path &markPath)
    : d(std::make_unique<Impl>()) {
  d->tree = wlr_scene_tree_create(parent);
  if (!d->tree)
    return;
  const float clear[4] = {};
  d->background = wlr_scene_rect_create(d->tree, 1, 1, clear);
  d->mark = wlr_scene_buffer_create(d->tree, nullptr);
  d->track = wlr_scene_rect_create(d->tree, kTrackWidth, kTrackHeight, clear);
  d->runner = wlr_scene_rect_create(d->tree, kRunnerWidth, kTrackHeight, clear);
  if (!markPath.empty())
    d->decode = std::make_unique<LoopTask>(
        loop,
        [impl = d.get(), markPath] { impl->markPixels = decodeMark(markPath); },
        [impl = d.get()] { impl->showMark(); });
}

SessionSplash::~SessionSplash() {
  d->decode.reset();
  if (d->tree)
    wlr_scene_node_destroy(&d->tree->node);
}

void SessionSplash::finish() { d->finished = true; }

bool SessionSplash::advance(std::uint64_t nowNs, int width, int height) {
  if (!d->tree || !d->background || !d->mark || !d->track || !d->runner)
    return false;
  if (!d->startNs)
    d->startNs = d->lastNs = nowNs;
  const double step =
      nowNs > d->lastNs ? std::min(0.1, (nowNs - d->lastNs) / 1e9) : 0.0;
  d->lastNs = nowNs;
  if (d->finished) {
    double offset = d->fade;
    stepSceneSpring(offset, d->fadeVelocity, WindowTuning::splashFadeSpring,
                    step);
    d->fade = std::clamp(offset, 0.0, 1.0);
    if (d->fade < WindowTuning::splashFadeSpring.restDistance)
      return false;
  }
  const auto fade = static_cast<float>(d->fade);

  wlr_scene_rect_set_size(d->background, std::max(1, width), std::max(1, height));
  setColor(d->background, kBackground, fade);

  const int markX = (width - kMarkSize) / 2;
  const int markY = height / 2 - kMarkSize / 2 - kTrackGap / 2;
  wlr_scene_node_set_position(&d->mark->node, markX, markY);
  if (d->markReady && !d->markShownNs)
    d->markShownNs = nowNs;
  const double markAlpha =
      d->markShownNs
          ? std::min(1.0, (nowNs - d->markShownNs) / 1e9 / kMarkFadeSeconds)
          : 0.0;
  wlr_scene_buffer_set_opacity(d->mark, static_cast<float>(markAlpha) * fade);

  const int trackX = (width - kTrackWidth) / 2;
  const int trackY = markY + kMarkSize + kTrackGap;
  wlr_scene_node_set_position(&d->track->node, trackX, trackY);
  setColor(d->track, {1.0f, 1.0f, 1.0f}, 0.10f * fade);

  // The light crosses the bar and leaves it, clipped to it.
  const double seconds = (nowNs - d->startNs) / 1e9;
  const double cycle = std::fmod(seconds, kRunnerSeconds) / kRunnerSeconds;
  const int left = static_cast<int>(std::lround(
      -kRunnerWidth + (kTrackWidth + kRunnerWidth) * easeInOut(cycle)));
  const int start = std::clamp(left, 0, kTrackWidth);
  const int end = std::clamp(left + kRunnerWidth, 0, kTrackWidth);
  wlr_scene_node_set_enabled(&d->runner->node, end > start);
  if (end > start) {
    wlr_scene_rect_set_size(d->runner, end - start, kTrackHeight);
    wlr_scene_node_set_position(&d->runner->node, trackX + start, trackY);
    setColor(d->runner, kRunner, fade);
  }
  return true;
}

} // namespace LunaDash
