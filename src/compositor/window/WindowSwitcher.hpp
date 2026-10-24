#pragma once

#include "core/process/ProcessEnvironment.hpp"
#include "core/filesystem/TemporaryDirectory.hpp"
#include "core/json/JsonAccess.hpp"
#include "core/process/ProcessRunner.hpp"

#include <memory>
#include <string>

namespace LunaDash {
// Owns one selection session and its private, event-driven shell feedback file.
class WindowSwitcher final {
public:
  enum class Scope { Windows, Workspaces };
  explicit WindowSwitcher(wl_event_loop *eventLoop = nullptr);
  ~WindowSwitcher();
  void setChannelPath(const std::string &path);
  bool begin(const Json &windows, int focused, int direction,
             const ProcessEnvironment &environment,
             Scope scope = Scope::Workspaces);
  static Json windowsForWorkspace(const Json &clients, int workspace);
  bool active() const;
  Scope scope() const;
  void dismissPopups();
  void step(int direction);
  bool select(int window);
  int finish(bool accept);
  void remove(int window);
  void setDrag(const Json &drag);
  Json snapshot() const;
  int serial() const;
  std::string thumbnailPath(int window) const;
  void setThumbnail(int serial, int window, const std::string &path);
  void setLayout(const Json &clients, int workspace, bool dragging);
  void setLauncherState(int serial, bool open);

private:
  std::string channelPath_;
  std::string lastPublished_;
  Json workspaces_ = Json::array();
  Scope scope_ = Scope::Workspaces;
  int dismissSerial_ = 0;
  Json drag_ = Json::object();
  Json clients_ = Json::array();
  int workspace_ = 0;
  bool dragging_ = false;
  int launcherSerial_ = 0;
  bool launcherOpen_ = false;
  wl_event_source *feedbackTimer_ = nullptr;
  bool feedbackPending_ = false;
  int index_ = 0;
  int serial_ = 0;
  bool active_ = false;
  bool ready_ = false;
  std::string background_;
  TemporaryDirectory captureDirectory_;
  std::unique_ptr<ProcessRunner> captureRunner_;
  static int handleFeedbackTimer(void *data);
  void publish();
  void writeSnapshot();
  void captureBackground(const ProcessEnvironment &environment);
};
} // namespace LunaDash
