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


class WorkspaceProtocol final {
public:

  WorkspaceProtocol(wl_display *display, std::function<void(int index)> activate);
  ~WorkspaceProtocol();
  WorkspaceProtocol(const WorkspaceProtocol &) = delete;
  WorkspaceProtocol &operator=(const WorkspaceProtocol &) = delete;

  static bool supported();
  bool valid() const { return manager_ != nullptr; }


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
