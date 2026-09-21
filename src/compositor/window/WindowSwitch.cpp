#include "compositor/animation/SceneWindowAnimations.hpp"
#include "compositor/client/ClientWindow.hpp"
#include "compositor/renderer/capture/ThumbnailReadback.h"
#include "compositor/wayland/Register.hpp"
#include "compositor/window/WindowRules.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include <QFutureWatcher>
#include <QImage>
#include <QTimer>
#include <QtConcurrent>
#include <algorithm>

namespace LunaDash {
void WaylandCompositor::publishWindowLayout() {
  QJsonArray clients;
  for (const auto &client : clients_) {
    if (client->utility || !client->mapped)
      continue;
    clients.append(QJsonObject{{"id", client->id},
                               {"title", client->title},
                               {"appId", client->appId},
                               {"icon", client->iconName},
                               {"workspace", client->workspace},
                               {"mapped", true},
                               {"desktop", client->desktop},
                               {"floating", client->floating},
                               {"minimized", client->minimized},
                               {"maximized", client->maximized},
                               {"hiddenByMaximize", client->hiddenByMaximize},
                               {"focused", focused_ == client.get()},
                               {"x", client->geometry.x()},
                               {"y", client->geometry.y()},
                               {"width", client->geometry.width()},
                               {"height", client->geometry.height()}});
  }
  windowSwitcher_->setLayout(clients, workspace_, d->pointerWindow != 0);
}
void WaylandCompositor::selectWorkspace(int workspace) {
  if (workspace < 0 || workspace >= 10)
    return;
  if (workspace >= desktopPreferences().value("workspaceCount").toInt()) {
    QString error;
    if (!updateDesktopPreferences({{"workspaceCount", workspace + 1}}, &error))
      return;
  }
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
void WaylandCompositor::beginWindowSwitch(int direction) {
  if (windowSwitcher_->active()) {
    windowSwitcher_->step(direction);
    return;
  }
  if (d->pointerWindow)
    d->finishWindowPointer(false);
  QJsonArray workspaces;
  QList<int> capture;
  const auto area = workArea();
  for (int workspace = 0; workspace < 10; ++workspace) {
    QJsonArray windows;
    QRect bounds = area;
    const auto placements = windowLayout_->presentation(workspace, area);
    for (const auto &client : clients_) {
      if (!client->mapped || client->utility || client->desktop ||
          client->workspace != workspace)
        continue;
      QRect geometry = client->geometry;
      for (const auto &placement : placements)
        if (placement.window == static_cast<LayoutWindowId>(client->id) &&
            !placement.minimized)
          geometry = placement.geometry;
      if (!client->minimized && !client->hiddenByMaximize)
        bounds = bounds.united(geometry);
      windows.append(QJsonObject{{"id", client->id},
                                 {"title", client->title},
                                 {"appId", client->appId},
                                 {"icon", client->iconName},
                                 {"x", geometry.x() - area.x()},
                                 {"y", geometry.y() - area.y()},
                                 {"width", geometry.width()},
                                 {"height", geometry.height()},
                                 {"minimized", client->minimized},
                                 {"hiddenByMaximize", client->hiddenByMaximize},
                                 {"focused", focused_ == client.get()}});
      if (!client->minimized && !client->hiddenByMaximize)
        capture.append(client->id);
    }
    for (int i = 0; i < windows.size(); ++i) {
      auto window = windows[i].toObject();
      window["x"] = window.value("x").toInt() + area.x() - bounds.x();
      window["y"] = window.value("y").toInt() + area.y() - bounds.y();
      windows[i] = window;
    }
    workspaces.append(QJsonObject{{"id", workspace + 1},
                                  {"windows", windows},
                                  {"width", bounds.width()},
                                  {"height", bounds.height()}});
  }
  windowSwitcher_->begin(workspaces, workspace_ + 1, direction,
                         clientEnvironment_);
  const int serial = windowSwitcher_->serial();
  QTimer::singleShot(0, this, [this, serial, capture] {
    captureWorkspaceThumbnail(serial, capture);
  });
}
void WaylandCompositor::captureWorkspaceThumbnail(int serial,
                                                  QList<int> windows) {
  if (!windowSwitcher_->active() || serial != windowSwitcher_->serial() ||
      windows.isEmpty())
    return;
  const int id = windows.takeFirst();
  auto found =
      std::find_if(clients_.begin(), clients_.end(), [id](const auto &client) {
        return client->id == id && client->mapped;
      });
  QImage image;
  if (found != clients_.end()) {
    auto *surface = (*found)->surface->surface;
    auto *texture = wlr_surface_get_texture(surface);
    const QSize size = QSize(std::max(1, surface->current.width),
                             std::max(1, surface->current.height))
                           .scaled(320, 200, Qt::KeepAspectRatio);
    image = QImage(size, QImage::Format_RGBA8888);
    wlr_fbox source{};
    wlr_surface_get_buffer_source_box(surface, &source);
    if (!texture || !ludash_thumbnail_read(
                        d->renderer, d->allocator, texture, &source,
                        wlr_output_transform_invert(surface->current.transform),
                        image.width(), image.height(), image.bits()))
      image = {};
  }
  if (image.isNull()) {
    QTimer::singleShot(16, this, [this, serial, windows] {
      captureWorkspaceThumbnail(serial, windows);
    });
    return;
  }
  const QString path = windowSwitcher_->thumbnailPath(id);
  auto *watcher = new QFutureWatcher<bool>(this);
  connect(watcher, &QFutureWatcher<bool>::finished, this,
          [this, watcher, serial, id, path, windows] {
            if (watcher->result())
              windowSwitcher_->setThumbnail(serial, id, path);
            watcher->deleteLater();
            QTimer::singleShot(16, this, [this, serial, windows] {
              captureWorkspaceThumbnail(serial, windows);
            });
          });
  watcher->setFuture(
      QtConcurrent::run([image, path] { return image.save(path, "PNG"); }));
}
void WaylandCompositor::finishWindowSwitch(bool accept) {
  const int target = windowSwitcher_->finish(accept);
  if (target)
    selectWorkspace(target - 1);
}
} // namespace LunaDash
