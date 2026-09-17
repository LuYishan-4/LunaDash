#pragma once
#include <LuDash/renderer/RenderBackend.h>
#include <LuDash/tiling/TilingLayout.h>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointF>
#include <QProcess>
#include <QQuickWindow>
#include <QRect>
#include <QSet>
#include <QtWaylandCompositor/QWaylandQuickCompositor>
#include <functional>
#include <memory>
#include <vector>
class QWaylandXdgShell;
class QWaylandQuickOutput;
class QWaylandXdgToplevel;
class QWaylandXdgSurface;
namespace LuDash {
class ShellModules;
class AudioSettings;
class PowerSettings;
class SystemStatus;
class WindowAnimations;
class XWaylandSupport;
struct BlurHealth;
class NetworkStatus;
class WallpaperItem;
class PluginManager;
class WindowFrame;
class LayerShell;
class ScreenCapture;
class ControlServer;
class SessionActions;
class ShortcutSettings;
class UpdateChecker;
class ResizeGuideItem;
struct ClientWindow;
class WaylandCompositor final : public QObject {
public:
  WaylandCompositor(const QByteArray &socket, bool fullscreen, bool startShell,
                    GraphicsApi graphics = GraphicsApi::Auto);
  ~WaylandCompositor() override;
  QProcess *spawn(const QStringList &arguments, const QString &program = {},
                  bool required = true);
  bool saveScreenshot(const QString &path);
  void saveState(const QString &path);
  bool hasProcessFailure() const;
  void closeTestSession(const std::function<void(bool)> &finished);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  std::shared_ptr<RenderState> renderState_;
  WallpaperItem *wallpaper_ = nullptr;
  QWaylandQuickCompositor compositor_;
  QQuickWindow window_;
  ShellModules *shellModules_ = nullptr;
  AudioSettings *audioSettings_ = nullptr;
  PowerSettings *powerSettings_ = nullptr;
  SessionActions *sessionActions_ = nullptr;
  ShortcutSettings *shortcutSettings_ = nullptr;
  UpdateChecker *updateChecker_ = nullptr;
  int settingsSerial_ = 0;
  QString settingsPage_ = "general";
  int pickerSerial_ = 0;
  SystemStatus *systemStatus_ = nullptr;
  NetworkStatus *networkStatus_ = nullptr;
  WindowAnimations *animations_ = nullptr;
  XWaylandSupport *xwayland_ = nullptr;
  std::shared_ptr<BlurHealth> blurHealth_;
  PluginManager *pluginManager_ = nullptr;
  LayerShell *layerShell_ = nullptr;
  ScreenCapture *screenCapture_ = nullptr;
  ControlServer *controlServer_ = nullptr;
  ResizeGuideItem *resizeGuide_ = nullptr;
  QString controlPath_;
  QProcessEnvironment clientEnvironment_;
  int nextWindowId_ = 1;
  QWaylandXdgShell *shell_ = nullptr;
  QWaylandQuickOutput *output_ = nullptr;
  std::vector<std::unique_ptr<ClientWindow>> clients_;
  QList<QProcess *> processes_;
  QSet<qint64> shellProcessIds_;
  ClientWindow *focused_ = nullptr;
  ClientWindow *resizing_ = nullptr;
  int workspace_ = 0;
  ScrollableTilingLayout tiling_;
  QSet<int> consumedKeys_;
  QHash<int, int> resizeOriginalWidths_;
  QPointF pointerPosition_;
  QPointF resizePointerStart_;
  QRect resizeStartGeometry_;
  QRect resizeGuideGeometry_;
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
  void resendKeyboardModifiers();
  QString nextCapturePath() const;
  void addWindow(QWaylandXdgToplevel *toplevel, QWaylandXdgSurface *surface);
  void configure(ClientWindow *client, const QRect &rectangle);
  void arrange();
  void beginInteractiveResize(ClientWindow *client, const QPointF &position);
  void updateInteractiveResize(const QPointF &position);
  void endInteractiveResize();
  void restoreResizeGuide(ClientWindow *client);
  QRect workArea() const;
  QJsonObject state() const;
  QJsonObject control(const QJsonObject &request);
  void focus(ClientWindow *client);
  void focusNext(int direction);
  void synchronizeTilingFocus();
  ClientWindow *clientAt(const QPointF &position) const;
};
} // namespace LuDash
