#include "compositor/client/ClientWindow.hpp"
#include "compositor/wayland/Runtime.hpp"
#include "compositor/window/WindowSwitcher.hpp"

namespace LunaDash {
void WaylandCompositor::activateTask(int window) {
  for (const auto &client : clients_) {
    if (client->id != window || !client->mapped || client->utility)
      continue;
    workspace_ = client->workspace;
    client->minimized = false;
    tiling_.setMinimized(window, false);
    tiling_.focus(window);
    arrange();
    if (!client->floating) {
      tiling_.resize(window, workArea().width());
      int active = 0;
      for (const auto &member : tiling_.snapshot(workspace_).columns)
        if (!member.minimized)
          ++active;
      tiling_.resizeHeight(window, active == 1
                                       ? workArea().height()
                                       : qRound(workArea().height() * 0.70));
      arrange();
    }
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
    d->finishTiledPointer(false);
  QJsonArray windows;
  for (const auto &client : clients_) {
    if (!client->mapped || client->utility || client->desktop)
      continue;
    windows.append(QJsonObject{{"id", client->id},
                               {"title", client->title},
                               {"appId", client->appId},
                               {"icon", client->iconName},
                               {"workspace", client->workspace}});
  }
  windowSwitcher_->begin(windows, focused_ ? focused_->id : 0, direction,
                         clientEnvironment_);
}
void WaylandCompositor::finishWindowSwitch(bool accept) {
  const int target = windowSwitcher_->finish(accept);
  if (target)
    activateTask(target);
}
} // namespace LunaDash
