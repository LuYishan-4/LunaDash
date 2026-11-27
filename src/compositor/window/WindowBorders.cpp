#include "compositor/client/ClientWindow.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "compositor/window/WindowTuning.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <drm_fourcc.h>
#include <vector>

namespace LunaDash {
namespace {

// Premultiplied ARGB pixels as a wlroots buffer.
struct PixelBuffer {
  wlr_buffer base;
  std::vector<uint32_t> pixels;
};

void destroyPixels(wlr_buffer *buffer) {
  delete reinterpret_cast<PixelBuffer *>(buffer);
}

bool beginPixels(wlr_buffer *buffer, uint32_t flags, void **data, uint32_t *format,
                 size_t *stride) {
  if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE)
    return false;
  auto *image = reinterpret_cast<PixelBuffer *>(buffer);
  *data = image->pixels.data();
  *format = DRM_FORMAT_ARGB8888;
  *stride = static_cast<size_t>(image->base.width) * 4;
  return true;
}

void endPixels(wlr_buffer *) {}

const wlr_buffer_impl pixelBufferImpl = {
    .destroy = destroyPixels,
    .get_dmabuf = nullptr,
    .get_shm = nullptr,
    .begin_data_ptr_access = beginPixels,
    .end_data_ptr_access = endPixels,
};

// The top-left corner of the outline: a square `radius + width` across whose
// bottom-right corner is the centre of the window's rounded corner, holding
// the antialiased ring between `radius` and `radius + width`. The other
// corners are this one mirrored.
wlr_buffer *outlineCorner(int radius, int width, const float color[4], double scale) {
  const int pixels = std::max(1, static_cast<int>(std::lround((radius + width) * scale)));
  auto *image = new PixelBuffer{};
  image->pixels.resize(static_cast<std::size_t>(pixels) * pixels);
  const double centre = pixels;
  const double inner = radius * scale, outer = (radius + width) * scale;
  for (int y = 0; y < pixels; ++y)
    for (int x = 0; x < pixels; ++x) {
      const double distance = std::hypot(centre - (x + 0.5), centre - (y + 0.5));
      const double coverage =
          std::clamp(std::min(distance - inner, outer - distance) + 0.5, 0.0, 1.0);
      const double alpha = coverage * color[3];
      const auto channel = [&](double value) {
        return static_cast<uint32_t>(std::lround(std::clamp(value, 0.0, 1.0) * 255.0));
      };
      image->pixels[static_cast<std::size_t>(y) * pixels + x] =
          channel(alpha) << 24 | channel(color[0] * alpha) << 16 |
          channel(color[1] * alpha) << 8 | channel(color[2] * alpha);
    }
  wlr_buffer_init(&image->base, &pixelBufferImpl, pixels, pixels);
  return &image->base;
}

} // namespace

void WaylandCompositor::Impl::updateWindowBorder(ClientWindow *client) {
  if (!client || !client->scene.tree || !normalLayer)
    return;
  auto &border = client->border;
  const auto &tree = client->scene.tree->node;
  const bool shown = q->windowTemplate_ && q->windowTemplate_->allowOverlap &&
                     tree.enabled && tree.parent == normalLayer &&
                     client->state.mapped && !client->state.minimized &&
                     !client->state.maximized && !client->state.fullscreen &&
                     !client->utility && !q->overviewShown() &&
                     client->layout.geometry.isValid();
  if (!shown) {
    if (border.tree)
      wlr_scene_node_set_enabled(&border.tree->node, false);
    return;
  }
  if (!border.tree) {
    border.tree = wlr_scene_tree_create(normalLayer);
    if (!border.tree)
      return;
    for (auto *&edge : border.edges)
      edge = wlr_scene_rect_create(border.tree, 1, 1, border.color);
    for (auto *&corner : border.corners)
      corner = wlr_scene_buffer_create(border.tree, nullptr);
  }

  const int width = WindowTuning::stackingBorderWidth;
  const int radius = windowCorners ? windowCorners->radius(client->scene.tree) : 0;
  const auto &colors = q->focused_ == client ? borderFocused : borderUnfocused;
  double scale = 1.0;
  for (const auto *output : outputs)
    if (output && output->output)
      scale = std::max(scale, static_cast<double>(output->output->scale));
  if (radius != border.radius || scale != border.scale ||
      std::memcmp(colors, border.color, sizeof(border.color)) != 0) {
    std::memcpy(border.color, colors, sizeof(border.color));
    border.radius = radius;
    border.scale = scale;
    for (auto *edge : border.edges)
      wlr_scene_rect_set_color(edge, border.color);
    auto *corner = outlineCorner(radius, width, border.color, scale);
    // Top-left as drawn; top-right, bottom-left and bottom-right mirrored.
    constexpr wl_output_transform mirrors[4] = {
        WL_OUTPUT_TRANSFORM_NORMAL, WL_OUTPUT_TRANSFORM_FLIPPED,
        WL_OUTPUT_TRANSFORM_FLIPPED_180, WL_OUTPUT_TRANSFORM_180};
    for (int index = 0; index < 4; ++index) {
      wlr_scene_buffer_set_buffer(border.corners[index], corner);
      wlr_scene_buffer_set_transform(border.corners[index], mirrors[index]);
      wlr_scene_buffer_set_dest_size(border.corners[index], radius + width,
                                     radius + width);
    }
    wlr_buffer_drop(corner);
  }

  // Around the window as it is drawn now, mid-animation included.
  const Size size = client->effects.animatedSize.isValid()
                        ? client->effects.animatedSize
                        : client->layout.geometry.size();
  const int w = size.width(), h = size.height();
  const int straightWidth = std::max(0, w - 2 * radius);
  const int straightHeight = std::max(0, h - 2 * radius);
  const struct {
    int x, y, width, height;
  } edges[4] = {{radius, -width, straightWidth, width},
                {radius, h, straightWidth, width},
                {-width, radius, width, straightHeight},
                {w, radius, width, straightHeight}};
  for (int index = 0; index < 4; ++index) {
    wlr_scene_rect_set_size(border.edges[index], edges[index].width, edges[index].height);
    wlr_scene_node_set_position(&border.edges[index]->node, edges[index].x, edges[index].y);
  }
  const int corners[4][2] = {{-width, -width},
                             {w - radius, -width},
                             {-width, h - radius},
                             {w - radius, h - radius}};
  for (int index = 0; index < 4; ++index)
    wlr_scene_node_set_position(&border.corners[index]->node, corners[index][0],
                                corners[index][1]);
  wlr_scene_node_set_position(&border.tree->node, tree.x, tree.y);
  wlr_scene_node_place_below(&border.tree->node, &client->scene.tree->node);
  wlr_scene_node_set_enabled(&border.tree->node, true);
}

} // namespace LunaDash
