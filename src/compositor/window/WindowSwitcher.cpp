#include "compositor/window/WindowSwitcher.hpp"
#include "core/filesystem/FileIo.hpp"
#include "core/text/Url.hpp"

#include <algorithm>
#include <filesystem>
#include <unistd.h>

namespace LunaDash {
WindowSwitcher::WindowSwitcher(wl_event_loop *eventLoop) {
  if (eventLoop)
    captureRunner_ = std::make_unique<ProcessRunner>(eventLoop, false);
  if (eventLoop)
    feedbackTimer_ = wl_event_loop_add_timer(eventLoop, handleFeedbackTimer,
                                             this);
}
WindowSwitcher::~WindowSwitcher() {
  if (feedbackTimer_)
    wl_event_source_remove(feedbackTimer_);
  if (captureRunner_)
    captureRunner_->cancel();
  if (!channelPath_.empty())
    ::unlink(channelPath_.c_str());
}
void WindowSwitcher::setChannelPath(const std::string &path) {
  channelPath_ = path;
  writeSnapshot();
}
Json WindowSwitcher::windowsForWorkspace(const Json &clients,
                                               int workspace) {
  Json windows = Json::array();
  if (!clients.is_array())
    return windows;
  for (const auto &client : clients) {
    if (!client.is_object())
      continue;
    if (jsonBool(client, "mapped") && !jsonBool(client, "utility") &&
        !jsonBool(client, "desktop") &&
        jsonInt(client, "workspace", -1) == workspace)
      windows.push_back(client);
  }
  return windows;
}
bool WindowSwitcher::active() const { return active_; }
WindowSwitcher::Scope WindowSwitcher::scope() const { return scope_; }
int WindowSwitcher::handleFeedbackTimer(void *data) {
  auto *self = static_cast<WindowSwitcher *>(data);
  if (!self)
    return 0;
  self->feedbackPending_ = false;
  self->writeSnapshot();
  return 0;
}
void WindowSwitcher::dismissPopups() {
  dismissSerial_ = dismissSerial_ >= 999999 ? 1 : dismissSerial_ + 1;
  publish();
}
bool WindowSwitcher::begin(const Json &windows, int focused,
                           int direction,
                           const ProcessEnvironment &environment,
                           Scope scope) {
  if (active_ && scope_ != scope)
    finish(false);
  if (active_) {
    step(direction);
    return true;
  }
  if (!windows.is_array() || windows.empty())
    return false;
  scope_ = scope;
  workspaces_ = windows;
  index_ = 0;
  for (std::size_t i = 0; i < workspaces_.size(); ++i)
    if (jsonInt(workspaces_[i], "id") == focused)
      index_ = static_cast<int>(i);
  active_ = true;
  ready_ = false;
  background_.clear();
  ++serial_;
  step(direction);
  captureBackground(environment);
  return true;
}
void WindowSwitcher::step(int direction) {
  if (!active_ || workspaces_.empty())
    return;
  const int count = static_cast<int>(workspaces_.size());
  index_ = ((index_ + direction) % count + count) % count;
  publish();
}
bool WindowSwitcher::select(int window) {
  if (!active_)
    return false;
  for (std::size_t i = 0; i < workspaces_.size(); ++i) {
    if (jsonInt(workspaces_[i], "id") != window)
      continue;
    index_ = static_cast<int>(i);
    publish();
    return true;
  }
  return false;
}
int WindowSwitcher::finish(bool accept) {
  if (!active_)
    return 0;
  const int selected = accept && !workspaces_.empty()
                           ? jsonInt(workspaces_[static_cast<std::size_t>(index_)], "id")
                           : 0;
  active_ = ready_ = false;
  if (captureRunner_)
    captureRunner_->cancel();
  publish();
  return selected;
}
void WindowSwitcher::remove(int window) {
  if (scope_ == Scope::Windows) {
    for (int i = static_cast<int>(workspaces_.size()); i-- > 0;) {
      if (jsonInt(workspaces_[static_cast<std::size_t>(i)], "id") != window)
        continue;
      workspaces_.erase(static_cast<std::size_t>(i));
      if (i < index_)
        --index_;
    }
    if (workspaces_.empty())
      finish(false);
    else
      index_ = std::min(index_, static_cast<int>(workspaces_.size()) - 1);
    publish();
    return;
  }
  for (std::size_t w = 0; w < workspaces_.size(); ++w) {
    auto workspace = jsonAsObject(workspaces_[w]);
    auto members = jsonArray(workspace, "windows");
    for (std::size_t i = members.size(); i-- > 0;)
      if (jsonInt(members[i], "id") == window)
        members.erase(i);
    workspace["windows"] = members;
    workspaces_[w] = workspace;
  }
  publish();
}
int WindowSwitcher::serial() const { return serial_; }
std::string WindowSwitcher::thumbnailPath(int window) const {
  return captureDirectory_.filePath(std::to_string(serial_) + "-window-" +
                                    std::to_string(window) + ".png");
}
void WindowSwitcher::setThumbnail(int serial, int window, const std::string &path) {
  if (!active_ || serial != serial_) {
    ::unlink(path.c_str());
    return;
  }
  if (scope_ == Scope::Windows) {
    for (auto &entry : workspaces_)
      if (entry.is_object() && jsonInt(entry, "id") == window)
        entry["thumbnail"] = fileUrl(path);
    publish();
    return;
  }
  for (auto &workspace : workspaces_) {
    if (!workspace.is_object())
      continue;
    auto members = jsonArray(workspace, "windows");
    for (auto &member : members)
      if (member.is_object() && jsonInt(member, "id") == window)
        member["thumbnail"] = fileUrl(path);
    workspace["windows"] = members;
  }
  publish();
}
void WindowSwitcher::setLayout(const Json &clients, int workspace,
                               bool dragging) {
  clients_ = clients;
  workspace_ = workspace;
  dragging_ = dragging;
  publish();
}
void WindowSwitcher::setLauncherState(int serial, bool open) {
  if (launcherSerial_ == serial && launcherOpen_ == open)
    return;
  launcherSerial_ = serial;
  launcherOpen_ = open;
  publish();
}
void WindowSwitcher::setDrag(const Json &drag) {
  drag_ = drag;
  publish();
}
Json WindowSwitcher::snapshot() const {
  return {{"active", active_},
          {"ready", ready_},
          {"index", index_},
          {"serial", serial_},
          {"scope", scope_ == Scope::Windows ? "windows" : "workspaces"},
          {"windows", scope_ == Scope::Windows ? workspaces_ : Json::array()},
          {"workspaces", scope_ == Scope::Workspaces ? workspaces_ : Json::array()},
          {"dismissSerial", dismissSerial_},
          {"background", background_},
          {"drag", drag_},
          {"clients", clients_},
          {"workspace", workspace_},
          {"dragging", dragging_},
          {"launcherSerial", launcherSerial_},
          {"launcherOpen", launcherOpen_}};
}
void WindowSwitcher::publish() {
  if (!feedbackTimer_) {
    writeSnapshot();
    return;
  }
  if (!feedbackPending_) {
    feedbackPending_ = true;
    if (wl_event_source_timer_update(feedbackTimer_, 16) < 0) {
      feedbackPending_ = false;
      writeSnapshot();
    }
  }
}
void WindowSwitcher::writeSnapshot() {
  if (channelPath_.empty())
    return;
  const auto data = jsonCompact(snapshot());
  if (data == lastPublished_)
    return;
  if (writeFileAtomically(channelPath_, data, nullptr,
                          std::filesystem::perms::owner_read |
                              std::filesystem::perms::owner_write))
    lastPublished_ = data;
}
void WindowSwitcher::captureBackground(
    const ProcessEnvironment &environment) {
  if (!captureDirectory_.isValid() || !captureRunner_) {
    ready_ = true;
    publish();
    return;
  }
  // Keep one private snapshot. Closing a session never waits for a capture.
  const std::string path =
      captureDirectory_.filePath(std::to_string(serial_) + ".png");
  std::error_code error;
  for (const auto &entry :
       std::filesystem::directory_iterator(captureDirectory_.path(), error))
    if (entry.is_regular_file(error) && entry.path().extension() == ".png")
      std::filesystem::remove(entry.path(), error);
  const bool started = captureRunner_->run(
      "grim", {"-s", "0.5", path},
      [this, path](bool success, const std::string &) {
        if (!active_)
          return;
        std::error_code sizeError;
        if (success && std::filesystem::file_size(path, sizeError) > 0 &&
            !sizeError)
          background_ = fileUrl(path);
        ready_ = true;
        publish();
      },
      350, environment);
  if (!started && !captureRunner_->busy()) {
    ready_ = true;
    publish();
  }
}
} // namespace LunaDash
