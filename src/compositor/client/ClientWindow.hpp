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

// The shell a window comes from. ClientSurface.hpp reads `surface` by it.
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

  // Helpers (override-redirect X11 windows, wl-clipboard): kept out of the
  // layout, the window switcher and focus.
  bool utility = false;
  // Opened while a shell overlay held the keyboard (settings opening a file
  // chooser): it floats above the shell until no overlay is left.
  bool shellDialog = false;

  struct {
    wlr_scene_tree *tree = nullptr;
    wlr_scene_tree *content = nullptr;
    // The window alone, in its own scene, for window capture (xdg-shell only).
    wlr_scene_tree *capture = nullptr;
  } scene;

  // What the client and the user asked of the window.
  struct {
    int workspace = 0;
    bool mapped = false;
    bool minimized = false;
    bool maximized = false;
    bool fullscreen = false;
    bool floating = false;
  } state;

  // Where the layout puts the window, in layout coordinates.
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
    // Stacking windows zoom instead of resizing: the client keeps laying
    // itself out at its own size and is drawn this many times as large
    // (clientContentScale, applyClientContentScale).
    double contentScale = 1.0;
    // The size a resize animation draws the window at right now (empty when
    // none runs); its live buffers are stretched to it.
    Size animatedSize;
    // The scale the scene currently draws the window at, per axis.
    double scaleX = 1.0, scaleY = 1.0;
  } effects;

  ClientWindow() = default;
  ClientWindow(const ClientWindow &) = delete;
  ClientWindow &operator=(const ClientWindow &) = delete;
  ~ClientWindow() {
    std::free(appId);
    std::free(title);
    std::free(iconName);
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
