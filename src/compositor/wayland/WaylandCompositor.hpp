#pragma once

#include "compositor/layout/WindowLayout.hpp"
#include "compositor/window/animation/WindowAnimation.hpp"
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRect>
#include <QSet>
#include <functional>
#include <memory>
#include <vector>

namespace LunaDash {

class WindowSwitcher;
struct WindowTemplate;
class ScreenCapture;
class BrightnessSettings;
class DdcBrightnessSettings;
class ShellModules;
class AudioSettings;
class PowerSettings;
class SystemStatus;
class XWaylandSupport;
class NetworkStatus;
class PluginManager;
class ControlServer;
class SessionActions;
class ShortcutSettings;
class UpdateChecker;
struct ClientWindow;

class WaylandCompositor final : public QObject {
public:
  WaylandCompositor(const QByteArray &socket, bool fullscreen, bool startShell,
                    const QString &rendererPreference);
  ~WaylandCompositor() override;

  QProcess *spawn(const QStringList &arguments, const QString &program = {},
                  bool required = true);
  bool saveScreenshot(const QString &path);
  void saveState(const QString &path);
  bool hasProcessFailure() const;
  void closeTestSession(const std::function<void(bool)> &finished);

private:
  class Impl;
  std::unique_ptr<Impl> d;

  WindowSwitcher *windowSwitcher_ = nullptr;
  ScreenCapture *screenCapture_ = nullptr;
  BrightnessSettings *brightnessSettings_ = nullptr;
  DdcBrightnessSettings *ddcBrightnessSettings_ = nullptr;
  ShellModules *shellModules_ = nullptr;
  AudioSettings *audioSettings_ = nullptr;
  PowerSettings *powerSettings_ = nullptr;
  SessionActions *sessionActions_ = nullptr;
  ShortcutSettings *shortcutSettings_ = nullptr;
  UpdateChecker *updateChecker_ = nullptr;
  std::unique_ptr<SceneWindowAnimationTemplate> windowAnimations_;
  SystemStatus *systemStatus_ = nullptr;
  NetworkStatus *networkStatus_ = nullptr;
  XWaylandSupport *xwayland_ = nullptr;
  PluginManager *pluginManager_ = nullptr;
  ControlServer *controlServer_ = nullptr;

  QString controlPath_;
  QProcessEnvironment clientEnvironment_;
  std::vector<std::unique_ptr<ClientWindow>> clients_;
  QList<QProcess *> processes_;
  QSet<qint64> shellProcessIds_;
  ClientWindow *focused_ = nullptr;

  int nextWindowId_ = 1;
  int workspace_ = 0;
  int settingsSerial_ = 0;
  QString settingsPage_ = "general";
  int pickerSerial_ = 0;
  const WindowTemplate *windowTemplate_ = nullptr;
  std::unique_ptr<WindowLayout> windowLayout_;
  QHash<int, int> resizeOriginalWidths_;
  bool shuttingDown_ = false;
  bool logoutPending_ = false;
  bool testStopping_ = false;
  bool processFailure_ = false;
  bool shortcutCapture_ = false;
  bool activationEnvironmentPublished_ = false;
  QString activationEnvironmentError_;
  QString lastCapture_;
  QString captureError_;
  int modifierResends_ = 0;
  int keypadKeyForwards_ = 0;

  void requestShutdown();
  void publishSessionActivationEnvironment();
  void captureScreen();
  bool launchExternalCommand(QStringList command, QString *error,
                             bool x11Helper = false);
  void resendKeyboardModifiers();
  QString nextCapturePath() const;
  void configure(ClientWindow *client, const QRect &rectangle);
  void arrange();
  QRect workArea() const;
  QJsonObject currentWindowLayoutSettings() const;
  bool updateWindowLayoutSettings(const QJsonObject &changes, QString *error);
  QJsonObject state() const;
  QJsonObject control(const QJsonObject &request);
  void focus(ClientWindow *client);
  void raiseWithDialogs(ClientWindow *client);
  void activateTask(int window);
  void beginWindowSwitch(int direction);
  void selectWorkspace(int workspace);
  void publishWindowLayout();
  void captureWorkspaceThumbnail(int serial, QList<int> windows);
  void finishWindowSwitch(bool accept);
  void setMaximized(ClientWindow *client, bool maximized);
  void focusNext(int direction);
  void synchronizeWindowFocus();
  void updateClientMetadata(ClientWindow *client);
  void removeClient(ClientWindow *client);
  void handleShortcut(const QString &action);
  void applyKeyboardConfiguration();
};

} // namespace LunaDash
