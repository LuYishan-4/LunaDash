#pragma once

#include "compositor/tiling/TilingLayout.hpp"
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

class ShellModules;
class AudioSettings;
class PowerSettings;
class SystemStatus;
class XWaylandSupport;
class NetworkStatus;
class PluginManager;
class ControlServer;
class SessionActions;
class SceneWindowAnimations;
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

  ShellModules *shellModules_ = nullptr;
  AudioSettings *audioSettings_ = nullptr;
  PowerSettings *powerSettings_ = nullptr;
  SessionActions *sessionActions_ = nullptr;
  ShortcutSettings *shortcutSettings_ = nullptr;
  UpdateChecker *updateChecker_ = nullptr;
  SceneWindowAnimations *windowAnimations_ = nullptr;
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
  ScrollableTilingLayout tiling_;
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
  void launchExternalCommand(QStringList command);
  void resendKeyboardModifiers();
  QString nextCapturePath() const;
  void configure(ClientWindow *client, const QRect &rectangle);
  void arrange();
  QRect workArea() const;
  QJsonObject state() const;
  QJsonObject control(const QJsonObject &request);
  void focus(ClientWindow *client);
  void focusNext(int direction);
  void synchronizeTilingFocus();
  void updateClientMetadata(ClientWindow *client);
  void removeClient(ClientWindow *client);
  void handleShortcut(const QString &action);
  void applyKeyboardConfiguration();
};

} // namespace LunaDash
