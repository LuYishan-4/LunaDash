#pragma once

#include "core/geometry/Geometry.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>

struct wlr_scene_tree;
struct wlr_xdg_surface;
struct wlr_xwayland_surface;

namespace LunaDash {

enum class ClientType { XdgShell, X11 };

struct ClientWindow {
  ClientType type = ClientType::XdgShell;
  union {
    wlr_xdg_surface *xdg;
    wlr_xwayland_surface *xwayland;
  } surface{};
  int id = 0;
  std::int64_t processId = 0;

  char *appId = strdup("");
  char *title = strdup("");
  char *iconName = strdup("");
  // An X11 window's own icon as a PNG, when it sets one; "" otherwise.
  char *iconFile = strdup("");
  int iconRevision = 0;

  bool utility = false;
  bool shellDialog = false;

  struct {
    wlr_scene_tree *tree = nullptr;
    wlr_scene_tree *content = nullptr;
    wlr_scene_tree *capture = nullptr;
  } scene;

  struct {
    int workspace = 0;
    bool mapped = false;
    bool minimized = false;
    bool maximized = false;
    bool fullscreen = false;
    bool floating = false;
    // A minimize asked for while the overview is open (holdMinimize).
    bool minimizeHeld = false;
  } state;

  struct {
    Rect geometry;
    Rect manualGeometry;        // moved or resized by hand while floating
    Size preferredFloatingSize; // the size it takes when it floats
    Size lastSize;              // the size last sent to the client
    bool pending = false;       // a new tile, configured once it is placed
    bool manualResize = false;
    bool hiddenByMaximize = false;
    bool initialRuleApplied = false;
  } layout;

  // Drawing only: visual effects, none of which moves the window.
  struct {
    double contentScale = 1.0;
    Size animatedSize;
    double scaleX = 1.0, scaleY = 1.0;
  } effects;

  ClientWindow() = default;
  ClientWindow(const ClientWindow &) = delete;
  ClientWindow &operator=(const ClientWindow &) = delete;
  ~ClientWindow() {
    std::free(appId);
    std::free(title);
    std::free(iconName);
    std::free(iconFile);
  }
};

inline void setClientText(char *&field, std::string_view value) {
  auto *copy = static_cast<char *>(std::malloc(value.size() + 1));
  if (!copy)
    return;
  std::memcpy(copy, value.data(), value.size());
  copy[value.size()] = '\0';
  std::free(field);
  field = copy;
}

} // namespace LunaDash
