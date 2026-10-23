#pragma once

#include "core/process/ProcessEnvironment.hpp"
#include "core/process/ProcessRunner.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

#include <memory>

namespace LunaDash {
// Owns one selection session and its private, event-driven shell feedback file.
class WindowSwitcher final {
public:
  enum class Scope { Windows, Workspaces };
  explicit WindowSwitcher(wl_event_loop *eventLoop = nullptr);
  ~WindowSwitcher();
  void setChannelPath(const QString &path);
  bool begin(const QJsonArray &windows, int focused, int direction,
             const ProcessEnvironment &environment,
             Scope scope = Scope::Workspaces);
  static QJsonArray windowsForWorkspace(const QJsonArray &clients, int workspace);
  bool active() const;
  Scope scope() const;
  void dismissPopups();
  void step(int direction);
  bool select(int window);
  int finish(bool accept);
  void remove(int window);
  void setDrag(const QJsonObject &drag);
  QJsonObject snapshot() const;
  int serial() const;
  QString thumbnailPath(int window) const;
  void setThumbnail(int serial, int window, const QString &path);
  void setLayout(const QJsonArray &clients, int workspace, bool dragging);
  void setLauncherState(int serial, bool open);

private:
  QString channelPath_;
  QByteArray lastPublished_;
  QJsonArray workspaces_;
  Scope scope_ = Scope::Workspaces;
  int dismissSerial_ = 0;
  QJsonObject drag_;
  QJsonArray clients_;
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
  QString background_;
  QTemporaryDir captureDirectory_;
  std::unique_ptr<ProcessRunner> captureRunner_;
  static int handleFeedbackTimer(void *data);
  void publish();
  void writeSnapshot();
  void captureBackground(const ProcessEnvironment &environment);
};
} // namespace LunaDash
