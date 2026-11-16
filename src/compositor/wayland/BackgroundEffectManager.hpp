#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

struct wl_client;
struct wl_display;
struct wl_global;
struct wl_resource;
struct wlr_surface;

namespace LunaDash {

// ext-background-effect-v1: clients ask for blur behind their own surfaces.
// The blur region is double-buffered wl_surface state; how the blur looks is
// compositor policy. The compositor only needs to know whether a surface's
// committed region is non-empty: WindowGlass blurs the whole surface.
class BackgroundEffectManager final {
public:
  // `changed` runs when a surface's committed blur turns on or off, so the
  // caller can schedule a frame even if the commit carried no new buffer.
  BackgroundEffectManager(wl_display *display, std::function<void()> changed);
  ~BackgroundEffectManager();
  BackgroundEffectManager(const BackgroundEffectManager &) = delete;
  BackgroundEffectManager &operator=(const BackgroundEffectManager &) = delete;

  bool valid() const { return global_ != nullptr; }
  // Whether blur is available and the surface's committed blur region is non-empty.
  bool wantsBlur(wlr_surface *surface) const;
  // Advertises (or withdraws) the blur capability to every bound client.
  void setBlurAvailable(bool available);

  struct Attachment;
  struct Callbacks;

private:
  wl_global *global_ = nullptr;
  bool blurAvailable_ = true;
  std::function<void()> changed_;
  std::vector<wl_resource *> managers_;
  std::unordered_map<wlr_surface *, Attachment *> attachments_;

  friend struct Callbacks;
};

} // namespace LunaDash
