#include "compositor/xwayland/WindowIcon.hpp"

#include <cstdint>
#include <vector>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"

namespace LunaDash {

bool writeXWaylandIcon(wlr_xwayland_surface *surface, const std::string &path) {
#if LUDASH_WLR_HAS_XWAYLAND && LUDASH_HAS_XCB_EWMH && WLR_VERSION_MINOR >= 20
  xcb_ewmh_get_wm_icon_reply_t reply;
  if (!surface || !wlr_xwayland_surface_fetch_icon(surface, &reply))
    return false;
  constexpr std::uint64_t kLargest = 128;
  const std::uint32_t *best = nullptr;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  for (auto icon = xcb_ewmh_get_wm_icon_iterator(&reply); icon.rem;
       xcb_ewmh_get_wm_icon_next(&icon)) {
    if (!icon.width || !icon.height)
      continue;
    const std::uint64_t area = std::uint64_t{icon.width} * icon.height;
    const std::uint64_t bestArea = std::uint64_t{width} * height;
    const bool fits = icon.width <= kLargest && icon.height <= kLargest;
    const bool bestFits = width <= kLargest && height <= kLargest;
    if (!best || (fits && (!bestFits || area > bestArea)) ||
        (!fits && !bestFits && area < bestArea)) {
      best = icon.data;
      width = icon.width;
      height = icon.height;
    }
  }
  bool saved = false;
  if (best) {
    // CARDINAL ARGB, not premultiplied, to the RGBA rows a pixbuf takes.
    std::vector<guint8> rgba(std::size_t{width} * height * 4);
    for (std::size_t i = 0; i < std::size_t{width} * height; ++i) {
      const std::uint32_t argb = best[i];
      rgba[i * 4] = (argb >> 16) & 0xff;
      rgba[i * 4 + 1] = (argb >> 8) & 0xff;
      rgba[i * 4 + 2] = argb & 0xff;
      rgba[i * 4 + 3] = (argb >> 24) & 0xff;
    }
    if (auto *pixbuf = gdk_pixbuf_new_from_data(
            rgba.data(), GDK_COLORSPACE_RGB, TRUE, 8, static_cast<int>(width),
            static_cast<int>(height), static_cast<int>(width) * 4, nullptr,
            nullptr)) {
      saved = gdk_pixbuf_save(pixbuf, path.c_str(), "png", nullptr, nullptr);
      g_object_unref(pixbuf);
    }
  }
  xcb_ewmh_get_wm_icon_reply_wipe(&reply);
  return saved;
#else
  (void)surface;
  (void)path;
  return false;
#endif
}

} // namespace LunaDash
