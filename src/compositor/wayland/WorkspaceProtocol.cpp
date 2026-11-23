#include "compositor/wayland/WorkspaceProtocol.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <utility>

#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"

#if LUDASH_WLR_HAS_EXT_WORKSPACE

namespace LunaDash {

struct WorkspaceProtocol::Listeners {
  wl_listener commit{};
  wl_listener destroy{};
  std::function<void(void *)> commitCallback;
  std::function<void()> destroyCallback;

  static void onCommit(wl_listener *listener, void *data) {
    Listeners *self = wl_container_of(listener, self, commit);
    self->commitCallback(data);
  }

  static void onDestroy(wl_listener *listener, void *) {
    Listeners *self = wl_container_of(listener, self, destroy);
    auto destroyed = self->destroyCallback;
    destroyed();
  }
};

void WorkspaceProtocol::detach() {
  if (!listeners_)
    return;
  wl_list_remove(&listeners_->commit.link);
  wl_list_remove(&listeners_->destroy.link);
  delete listeners_;
  listeners_ = nullptr;
  manager_ = nullptr;
  group_ = nullptr;
  workspaces_.clear();
  outputs_.clear();
}

bool WorkspaceProtocol::supported() { return true; }

WorkspaceProtocol::WorkspaceProtocol(wl_display *display,
                                     std::function<void(int)> activate)
    : activate_(std::move(activate)) {
  manager_ = wlr_ext_workspace_manager_v1_create(display, 1);
  if (!manager_)
    return;
  group_ = wlr_ext_workspace_group_handle_v1_create(manager_, 0);
  listeners_ = new Listeners();
  listeners_->commitCallback = [this](void *data) { onCommit(data); };
  listeners_->destroyCallback = [this] { detach(); };
  listeners_->commit.notify = &Listeners::onCommit;
  listeners_->destroy.notify = &Listeners::onDestroy;
  wl_signal_add(&manager_->events.commit, &listeners_->commit);
  wl_signal_add(&manager_->events.destroy, &listeners_->destroy);
}

WorkspaceProtocol::~WorkspaceProtocol() { detach(); }

void WorkspaceProtocol::update(int count, int active,
                               const std::vector<wlr_output *> &outputs) {
  if (!manager_ || !group_)
    return;
  for (auto *output : outputs)
    if (std::find(outputs_.begin(), outputs_.end(), output) == outputs_.end())
      wlr_ext_workspace_group_handle_v1_output_enter(group_, output);
  outputs_ = outputs;

  count = std::max(count, 0);
  while (static_cast<int>(workspaces_.size()) < count) {
    const auto index = static_cast<std::uint32_t>(workspaces_.size());
    const auto name = std::to_string(index + 1);
    auto *workspace = wlr_ext_workspace_handle_v1_create(
        manager_, name.c_str(),
        EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_ACTIVATE);
    if (!workspace)
      break;
    wlr_ext_workspace_handle_v1_set_group(workspace, group_);
    wlr_ext_workspace_handle_v1_set_name(workspace, name.c_str());
    wlr_ext_workspace_handle_v1_set_coordinates(workspace, &index, 1);
    workspaces_.push_back(workspace);
  }
  while (static_cast<int>(workspaces_.size()) > count) {
    wlr_ext_workspace_handle_v1_destroy(workspaces_.back());
    workspaces_.pop_back();
  }
  // wlroots only sends states that changed.
  for (std::size_t i = 0; i < workspaces_.size(); ++i)
    wlr_ext_workspace_handle_v1_set_active(workspaces_[i],
                                           static_cast<int>(i) == active);
  active_ = active;
}

void WorkspaceProtocol::onCommit(void *data) {
  auto *event = static_cast<wlr_ext_workspace_v1_commit_event *>(data);
  if (!event || !event->requests || !activate_)
    return;
  // Switching rearranges and may remove handles: collect first, act after.
  int target = -1;
  wlr_ext_workspace_v1_request *request = nullptr;
  wl_list_for_each(request, event->requests, link) {
    if (request->type != WLR_EXT_WORKSPACE_V1_REQUEST_ACTIVATE ||
        !request->activate.workspace)
      continue;
    const auto found = std::find(workspaces_.begin(), workspaces_.end(),
                                 request->activate.workspace);
    if (found != workspaces_.end())
      target = static_cast<int>(found - workspaces_.begin());
  }
  if (target >= 0 && target != active_)
    activate_(target);
}

} // namespace LunaDash

#else

namespace LunaDash {

struct WorkspaceProtocol::Listeners {};

bool WorkspaceProtocol::supported() { return false; }
WorkspaceProtocol::WorkspaceProtocol(wl_display *, std::function<void(int)>) {}
WorkspaceProtocol::~WorkspaceProtocol() = default;
void WorkspaceProtocol::detach() {}
void WorkspaceProtocol::update(int, int, const std::vector<wlr_output *> &) {}
void WorkspaceProtocol::onCommit(void *) {}

} // namespace LunaDash

#endif
