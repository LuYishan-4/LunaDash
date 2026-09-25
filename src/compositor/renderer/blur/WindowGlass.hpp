#pragma once

#include <QList>
#include <QSize>
#include <QString>
#include <QtGlobal>
#include <memory>

struct wlr_allocator;
struct wlr_renderer;
struct wlr_scene_node;
struct wlr_scene_tree;

namespace LunaDash {

// Per-window underlays in the existing wlroots scene. The compositor retains
// window membership, stacking and animation ownership.
class WindowGlass final {
public:
  struct Surface {
    wlr_scene_tree *tree = nullptr;
    QSize size;
    bool enabled = true;
    int cornerRadius = 16;
  };

  WindowGlass(wlr_renderer *renderer, wlr_allocator *allocator,
              wlr_scene_node *root);
  ~WindowGlass();
  WindowGlass(const WindowGlass &) = delete;
  WindowGlass &operator=(const WindowGlass &) = delete;

  bool configure(bool blurEnabled, int radius, float opacity);
  void update(const QList<Surface> &surfaces, bool animationsActive);
  bool ready() const;
  bool failed() const;
  quint64 frames() const;
  QString error() const;

private:
  class Impl;
  std::unique_ptr<Impl> d;
};

} // namespace LunaDash
