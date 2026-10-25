#include <gdk-pixbuf/gdk-pixbuf.h>

#include "compositor/client/ClientWindow.hpp"
#include "compositor/renderer/capture/ThumbnailReadback.h"
#include "compositor/wayland/Register.hpp"
#include "compositor/window/WindowRules.hpp"
#include "compositor/window/animation/WindowAnimation.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <utility>
#include <vector>

namespace LunaDash {
struct WaylandCompositor::ThumbnailWriteState {
  std::future<bool> result;
  int serial = 0;
  int window = 0;
  std::string path;
  std::vector<int> remaining;
};

void WaylandCompositor::publishWindowLayout() {
  Json clients = Json::array();
  for (const auto &client : clients_) {
    if (client->utility || !client->mapped)
      continue;
    const SceneAnimationRectangle fallback{
        client->geometry.x(), client->geometry.y(),
        client->geometry.width(), client->geometry.height()};
    const auto frame = windowAnimations_
                           ? windowAnimations_->animation().visualGeometry(
                                 client->sceneTree, fallback)
                           : fallback;
    clients.push_back(Json::object({{"id", client->id},
                               {"frameX", frame.left},
                               {"frameY", frame.top},
                               {"frameWidth", frame.width},
                               {"frameHeight", frame.height},
                               {"cornerRadius", d->windowCorners
                                    ? d->windowCorners->radius(client->sceneTree) : 0},
                               {"fullscreen", client->fullscreen},
                               {"visible", client->sceneTree && client->sceneTree->node.enabled},
                               {"title", client->title},
                               {"appId", client->appId},
                               {"icon", client->iconName},
                               {"workspace", client->workspace},
                               {"mapped", true},
                               {"desktop", client->desktop},
                               {"x11", client->x11},
                               {"floating", client->floating},
                               {"minimized", client->minimized},
                               {"maximized", client->maximized},
                               {"hiddenByMaximize", client->hiddenByMaximize},
                               {"focused", focused_ == client.get()},
                               {"x", client->geometry.x()},
                               {"y", client->geometry.y()},
                               {"width", client->geometry.width()},
                               {"height", client->geometry.height()}}));
  }
  windowSwitcher_->setLayout(clients, workspace_, d->pointerWindow != 0);
}
int WaylandCompositor::workspaceCount() const {
  // A bound for the numbers, not a limit anyone reaches.
  constexpr int kMaximumWorkspaces = 100;
  int last = -1;
  for (const auto &client : clients_)
    if (!client->utility && !client->desktop &&
        (client->mapped || client->layoutPending))
      last = std::max(last, client->workspace);
  return std::clamp(std::max(last + 2, workspace_ + 1), 1, kMaximumWorkspaces);
}
void WaylandCompositor::compactWorkspaces() {
  std::vector<bool> occupied;
  for (const auto &client : clients_) {
    if (client->utility || client->desktop ||
        !(client->mapped || client->layoutPending))
      continue;
    if (client->workspace >= static_cast<int>(occupied.size()))
      occupied.resize(static_cast<std::size_t>(client->workspace) + 1, false);
    occupied[static_cast<std::size_t>(client->workspace)] = true;
  }
  int span = static_cast<int>(occupied.size());
  for (int workspace = span - 1; workspace-- > 0;) {
    if (occupied[static_cast<std::size_t>(workspace)] || workspace == workspace_)
      continue;
    --span;
    for (const auto &client : clients_)
      if (client->workspace > workspace)
        --client->workspace;
    if (workspace_ > workspace)
      --workspace_;
  }
  // An empty current workspace far past the others moves next to them.
  workspace_ = std::min(workspace_, span);
}
void WaylandCompositor::selectWorkspace(int workspace) {
  if (workspace < 0)
    return;
  workspace = std::min(workspace, workspaceCount() - 1);
  if (workspace != workspace_ && windowSwitcher_->active() &&
      windowSwitcher_->scope() == WindowSwitcher::Scope::Windows)
    windowSwitcher_->finish(false);
  if (workspace != workspace_)
    workspaceSlide_ = workspace > workspace_ ? 1 : -1;
  workspace_ = workspace;
  arrange();
  synchronizeWindowFocus();
}
void WaylandCompositor::activateTask(int window) {
  for (const auto &client : clients_) {
    if (client->id != window || !client->mapped || client->utility)
      continue;
    const bool restore = client.get() == focused_ &&
                         client->workspace == workspace_ && client->maximized;
    selectWorkspace(client->workspace);
    client->minimized = false;
#if LUDASH_WLR_HAS_XWAYLAND
    if (client->x11 && client->xwayland)
      wlr_xwayland_surface_set_minimized(client->xwayland, false);
#endif
    windowLayout_->setMinimized(window, false);
    if (windowTemplate_ &&
        windowActivationTogglesMaximize(*windowTemplate_, *client))
      setMaximized(client.get(), !restore);
    windowLayout_->focus(window);
    arrange();
    focus(client.get());
    return;
  }
}
void WaylandCompositor::beginWindowSwitch(int direction, bool workspaces) {
  const auto scope = workspaces ? WindowSwitcher::Scope::Workspaces
                               : WindowSwitcher::Scope::Windows;
  if (windowSwitcher_->active() && windowSwitcher_->scope() != scope)
    finishWindowSwitch(false);
  if (windowSwitcher_->active()) {
    windowSwitcher_->step(direction);
    return;
  }
  if (d->pointerWindow)
    d->finishWindowPointer(false);
  Json choices = Json::array();
  std::vector<int> capture;
  const auto area = workArea();
  if (!workspaces) {
    publishWindowLayout();
    // Every window in the taskbar: the current workspace first, then the
    // others in order. Choosing one switches to its workspace.
    const auto clients = jsonArray(windowSwitcher_->snapshot(), "clients");
    choices = WindowSwitcher::windowsForWorkspace(clients, workspace_);
    for (const auto &choice : choices)
      capture.push_back(jsonInt(choice, "id"));
    for (int workspace = 0; workspace < workspaceCount(); ++workspace)
      if (workspace != workspace_)
        for (const auto &choice :
             WindowSwitcher::windowsForWorkspace(clients, workspace))
          choices.push_back(choice);
    if (windowSwitcher_->begin(choices, focused_ ? focused_->id : 0,
                                direction, clientEnvironment_, scope)) {
      const int serial = windowSwitcher_->serial();
      d->scheduleCallback(0, [this, serial, capture] {
        captureWorkspaceThumbnail(serial, capture);
      });
    }
    return;
  }
  for (int workspace = 0; workspace < workspaceCount(); ++workspace) {
    Json windows = Json::array();
    Rect bounds = area;
    const auto placements = windowLayout_->presentation(workspace, area);
    for (const auto &client : clients_) {
      if (!client->mapped || client->utility || client->desktop ||
          client->workspace != workspace)
        continue;
      Rect geometry = client->geometry;
      for (const auto &placement : placements)
        if (placement.window == static_cast<LayoutWindowId>(client->id) &&
            !placement.minimized)
          geometry = placement.geometry;
      if (!client->minimized && !client->hiddenByMaximize)
        bounds = bounds.united(geometry);
      windows.push_back(Json::object({{"id", client->id},
                                 {"title", client->title},
                                 {"appId", client->appId},
                                 {"icon", client->iconName},
                                 {"x", geometry.x() - area.x()},
                                 {"y", geometry.y() - area.y()},
                                 {"width", geometry.width()},
                                 {"height", geometry.height()},
                                 {"minimized", client->minimized},
                                 {"hiddenByMaximize", client->hiddenByMaximize},
                                 {"focused", focused_ == client.get()}}));
      if (!client->minimized && !client->hiddenByMaximize)
        capture.push_back(client->id);
    }
    for (auto &window : windows) {
      if (!window.is_object())
        window = Json::object();
      window["x"] = jsonInt(window, "x") + area.x() - bounds.x();
      window["y"] = jsonInt(window, "y") + area.y() - bounds.y();
    }
    choices.push_back(Json::object({{"id", workspace + 1},
                                  {"windows", windows},
                                  {"width", bounds.width()},
                                  {"height", bounds.height()}}));
  }
  windowSwitcher_->begin(choices, workspace_ + 1, direction,
                         clientEnvironment_, scope);
  const int serial = windowSwitcher_->serial();
  d->scheduleCallback(0, [this, serial, capture] {
    captureWorkspaceThumbnail(serial, capture);
  });
}
void WaylandCompositor::captureWorkspaceThumbnail(int serial,
                                                  std::vector<int> windows) {
  if (thumbnailWrite_) {
    if (thumbnailWrite_->serial != serial) {
      queuedThumbnailSerial_ = serial;
      queuedThumbnailWindows_ = std::move(windows);
    }
    return;
  }
  if (!windowSwitcher_->active() || serial != windowSwitcher_->serial() ||
      windows.empty())
    return;
  const int id = windows.front();
  windows.erase(windows.begin());
  auto found =
      std::find_if(clients_.begin(), clients_.end(), [id](const auto &client) {
        return client->id == id && client->mapped;
      });
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> pixels;
  bool captured = false;
  if (found != clients_.end() && (*found)->wlSurface) {
    auto *surface = (*found)->wlSurface;
    auto *texture = wlr_surface_get_texture(surface);
    const auto sourceWidth = std::max(1, surface->current.width);
    const auto sourceHeight = std::max(1, surface->current.height);
    const double scale =
        std::min({1.0, 320.0 / sourceWidth, 200.0 / sourceHeight});
    width = std::max(1, static_cast<int>(sourceWidth * scale));
    height = std::max(1, static_cast<int>(sourceHeight * scale));
    pixels.resize(static_cast<std::size_t>(width) * height * 4);
    wlr_fbox source{};
    wlr_surface_get_buffer_source_box(surface, &source);
    captured =
        texture && ludash_thumbnail_read(
                       d->renderer, d->allocator, texture, &source,
                       wlr_output_transform_invert(
                           surface->current.transform),
                       width, height, pixels.data());
  }
  if (!captured) {
    d->scheduleCallback(16, [this, serial, windows] {
      captureWorkspaceThumbnail(serial, windows);
    });
    return;
  }
  const std::string path = windowSwitcher_->thumbnailPath(id);
  auto write = std::make_shared<ThumbnailWriteState>();
  write->serial = serial;
  write->window = id;
  write->path = path;
  write->remaining = std::move(windows);
  try {
    auto image = std::make_shared<std::vector<std::uint8_t>>(
        std::move(pixels));
    write->result = std::async(std::launch::async, [image, path, width,
                                                    height] {
      auto *pixbuf = gdk_pixbuf_new_from_data(
          image->data(), GDK_COLORSPACE_RGB, TRUE, 8, width, height, width * 4,
          nullptr, nullptr);
      if (!pixbuf)
        return false;
      GError *error = nullptr;
      const bool saved = gdk_pixbuf_save(pixbuf, path.c_str(),
                                         "png", &error, nullptr);
      g_object_unref(pixbuf);
      if (error)
        g_error_free(error);
      return saved;
    });
  } catch (...) {
    d->scheduleCallback(16, [this, serial, remaining = write->remaining] {
      captureWorkspaceThumbnail(serial, remaining);
    });
    return;
  }
  thumbnailWrite_ = std::move(write);
  d->scheduleCallback(0, [this] { pollThumbnailWrite(); });
}

void WaylandCompositor::pollThumbnailWrite() {
  if (!thumbnailWrite_)
    return;
  if (thumbnailWrite_->result.wait_for(std::chrono::seconds(0)) !=
      std::future_status::ready) {
    d->scheduleCallback(2, [this] { pollThumbnailWrite(); });
    return;
  }

  const auto completed = std::move(thumbnailWrite_);
  bool saved = false;
  try {
    saved = completed->result.get();
  } catch (...) {
  }
  const bool current = windowSwitcher_->active() &&
                       completed->serial == windowSwitcher_->serial();
  if (saved && current)
    windowSwitcher_->setThumbnail(completed->serial, completed->window,
                                   completed->path);
  else if (saved) {
    std::error_code error;
    std::filesystem::remove(completed->path, error);
  }

  if (queuedThumbnailSerial_ != 0) {
    const int serial = std::exchange(queuedThumbnailSerial_, 0);
    auto queued = std::move(queuedThumbnailWindows_);
    queuedThumbnailWindows_.clear();
    d->scheduleCallback(16, [this, serial, queued = std::move(queued)] {
      captureWorkspaceThumbnail(serial, queued);
    });
    return;
  }

  if (current) {
    d->scheduleCallback(16, [this, serial = completed->serial,
                              remaining = std::move(completed->remaining)] {
      captureWorkspaceThumbnail(serial, remaining);
    });
  }
}
void WaylandCompositor::finishWindowSwitch(bool accept) {
  const auto scope = windowSwitcher_->scope();
  const int target = windowSwitcher_->finish(accept);
  if (!target)
    return;
  if (scope == WindowSwitcher::Scope::Workspaces) {
    selectWorkspace(target - 1);
    return;
  }
  // Alt+Tab focuses, never toggles maximization like a taskbar activation.
  for (const auto &client : clients_) {
    if (client->id != target || !client->mapped || client->utility ||
        client->desktop)
      continue;
    if (client->workspace != workspace_)
      selectWorkspace(client->workspace);
    client->minimized = false;
#if LUDASH_WLR_HAS_XWAYLAND
    if (client->x11 && client->xwayland)
      wlr_xwayland_surface_set_minimized(client->xwayland, false);
#endif
    windowLayout_->setMinimized(target, false);
    windowLayout_->focus(target);
    arrange();
    focus(client.get());
    return;
  }
}
} // namespace LunaDash
