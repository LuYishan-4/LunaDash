#pragma once

#include <functional>
#include <vector>

struct wl_display;
struct wl_listener;
struct wlr_output;
struct wlr_ext_workspace_manager_v1;
struct wlr_ext_workspace_group_handle_v1;
struct wlr_ext_workspace_handle_v1;

namespace LunaDash {

// ext-workspace-v1: publishes the desktop's workspaces so any shell (SlintLu,
// Quickshell, Waybar) can list and switch them without LunaDash-specific IPC.
// LunaDash's workspaces are shared by every output, so there is one group
// that every output enters; workspace i has the id and name "i+1".
class WorkspaceProtocol final {
public:
  // `activate` runs when a client asks to switch to the workspace at `index`.
  WorkspaceProtocol(wl_display *display, std::function<void(int index)> activate);
  ~WorkspaceProtocol();
  WorkspaceProtocol(const WorkspaceProtocol &) = delete;
  WorkspaceProtocol &operator=(const WorkspaceProtocol &) = delete;

  // Whether this wlroots supports the protocol at all.
  static bool supported();
  bool valid() const { return manager_ != nullptr; }

  // Publishes `count` workspaces with `active` current and the group on every
  // output in `outputs`. Only differences reach clients; call it freely.
  void update(int count, int active, const std::vector<wlr_output *> &outputs);

  struct Listeners;

private:
  void onCommit(void *data);
  void detach();

  wlr_ext_workspace_manager_v1 *manager_ = nullptr;
  wlr_ext_workspace_group_handle_v1 *group_ = nullptr;
  std::vector<wlr_ext_workspace_handle_v1 *> workspaces_;
  std::vector<wlr_output *> outputs_;
  int active_ = -1;
  std::function<void(int)> activate_;
  Listeners *listeners_ = nullptr;
};

} // namespace LunaDash
