#include "compositor/WaylandCompositor/WaylandCompositor.hpp"
#include "compositor/ClientWindow/ClientWindow.hpp"
#include "compositor/LuDashUtils/LuDashUtils.hpp"
#include "compositor/ResizeGuideItem/ResizeGuideItem.hpp"
#include "compositor/SessionActions/SessionActions.hpp"
#include "compositor/SessionEnvironment/SessionEnvironment.hpp"
#include "compositor/ShellModules/ShellModules.hpp"
#include "compositor/SystemStatus/SystemStatus.hpp"
#include "compositor/UpdateChecker/UpdateChecker.hpp"
#include "compositor/WindowFrame/WindowFrame.hpp"
#include "compositor/WindowRules/WindowRules.hpp"
#include "compositor/input/InputMethodSupport/InputMethodSupport.hpp"
#include "compositor/ipc/ControlServer/ControlServer.hpp"
#include "compositor/plugins/PluginManager/PluginManager.hpp"
#include "compositor/protocols/LayerShell/LayerShell.hpp"
#include "compositor/protocols/ProtocolExtensions/ProtocolExtensions.hpp"
#include "compositor/protocols/ScreenCapture/ScreenCapture.hpp"
#include "compositor/render/BlurItem/BlurItem.hpp"
#include "compositor/render/ShellRenderer/ShellRenderer.hpp"
#include "compositor/render/WallpaperItem/WallpaperItem.hpp"
#include "compositor/render/WindowAnimations/WindowAnimations.hpp"
#include "compositor/tiling/TilingLayout/TilingLayout.hpp"
#include "compositor/xwayland/XWaylandSupport/XWaylandSupport.hpp"
#include "config/DesktopPreferences/DesktopPreferences.hpp"
#include "config/Localization/Localization.hpp"
#include "desktop/AudioSettings/AudioSettings.hpp"
#include "desktop/DefaultApplications/DefaultApplications.hpp"
#include "desktop/DisplaySettings/DisplaySettings.hpp"
#include "desktop/InputSettings/InputSettings.hpp"
#include "desktop/NetworkStatus/NetworkStatus.hpp"
#include "desktop/PowerSettings/PowerSettings.hpp"
#include "desktop/ShortcutSettings/ShortcutSettings.hpp"
#include "desktop/SystemTools/SystemTools.hpp"
#include "desktop/WallpaperSettings/WallpaperSettings.hpp"
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQuickPaintedItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QtWaylandCompositor/QWaylandClient>
#include <QtWaylandCompositor/QWaylandKeymap>
#include <QtWaylandCompositor/QWaylandQuickCompositor>
#include <QtWaylandCompositor/QWaylandQuickOutput>
#include <QtWaylandCompositor/QWaylandQuickShellSurfaceItem>
#include <QtWaylandCompositor/QWaylandSeat>
#include <QtWaylandCompositor/QWaylandViewporter>
#include <QtWaylandCompositor/QWaylandXdgDecorationManagerV1>
#include <QtWaylandCompositor/QWaylandXdgShell>
#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>

namespace LuDash {

using namespace LuDash::Utils;

WaylandCompositor::WaylandCompositor(const QByteArray &socket, bool fullscreen,
                                     bool startShell, GraphicsApi graphics) {
  window_.setTitle("LunaDash Wayland · workspace 1");
  window_.resize(1440, 900);
  window_.setMinimumSize({960, 640});
  renderState_ = std::make_shared<RenderState>();
  connect(&window_, &QQuickWindow::sceneGraphError, this,
          [this](QQuickWindow::SceneGraphError, const QString &message) {
            renderState_->failed = true;
            qCritical().noquote()
                << "LunaDash graphics initialization failed:" << message;
            qCritical("Requires OpenGL 3.3 compatibility or OpenGL ES 3.0. "
                      "Check the driver and MESA_GL_VERSION_OVERRIDE.");
            QTimer::singleShot(0, this, [] { QCoreApplication::exit(2); });
          });
  wallpaper_ = new WallpaperItem(graphics, renderState_, window_.contentItem());
  wallpaper_->setSize(window_.size());
  wallpaper_->setZ(-200);
  wallpaper_->setPalette(QSettings().value("appearance/wallpaper", 0).toInt());
  resizeGuide_ = new ResizeGuideItem(window_.contentItem());
  window_.setColor(QColor("#171c36"));
  window_.installEventFilter(this);
  compositor_.setSocketName(socket);
  shell_ = new QWaylandXdgShell(&compositor_);
  new QWaylandViewporter(&compositor_);
  animations_ = new WindowAnimations(this);
  blurHealth_ = std::make_shared<BlurHealth>();
  auto *decorations = new QWaylandXdgDecorationManagerV1;
  decorations->setParent(&compositor_);
  decorations->setExtensionContainer(&compositor_);
  decorations->initialize();
  decorations->setPreferredMode(QWaylandXdgToplevel::ServerSideDecoration);
  output_ = new QWaylandQuickOutput(&compositor_, &window_);
  output_->setSizeFollowsWindow(true);
  output_->setManufacturer("LunaDash");
  output_->setModel("LunaDash desktop");
  connect(shell_, &QWaylandXdgShell::toplevelCreated, this,
          &WaylandCompositor::addWindow);
  installInputMethodProtocols(&compositor_);
  installCoreProtocolExtensions(&compositor_, output_, &window_);
  const QString platform = QGuiApplication::platformName();
  const bool bareMetal = platform == QLatin1String("eglfs") ||
                         platform == QLatin1String("linuxfb") ||
                         platform == QLatin1String("kms") ||
                         platform == QLatin1String("vkkhrdisplay");
  compositor_.setUseHardwareIntegrationExtension(bareMetal);
  compositor_.setRetainedSelectionEnabled(true);
  setKeyboardLedControlEnabled(bareMetal);

  // QT_WAYLAND_CLIENT_BUFFER_INTEGRATION accepts one plugin key, not a
  // semicolon-separated fallback list. In a real EGLFS/KMS login prefer Qt's
  // linux-dmabuf-v1 compositor integration: it advertises zwp_linux_dmabuf_v1
  // v4 (including default feedback), which Vulkan/WGPU clients and XWayland
  // need for modern GPU-backed buffers. Nested development sessions keep Qt's
  // default integration so they do not depend on the host DRM device.
  const QByteArray previousBufferIntegration =
      qgetenv("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION");
  if (bareMetal && previousBufferIntegration.isEmpty())
    qputenv("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION", "linux-dmabuf-v1");
  compositor_.create();
  if (bareMetal && previousBufferIntegration.isEmpty())
    qunsetenv("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION");
  else if (!previousBufferIntegration.isEmpty())
    qputenv("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION", previousBufferIntegration);
  layerShell_ = new LayerShell(&compositor_, output_, &window_);
  screenCapture_ = new ScreenCapture(&compositor_, output_, &window_);
  controlPath_ =
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + "/" +
      QString::fromUtf8(socket) + "-control";
  controlServer_ = new ControlServer(
      controlPath_,
      [this](const QJsonObject &request) { return control(request); }, this);
  clientEnvironment_ =
      createClientEnvironment(QString::fromUtf8(socket), controlPath_,
                              QCoreApplication::applicationDirPath());
  publishClientEnvironment(clientEnvironment_);
  shellModules_ = new ShellModules(this);
  connect(shellModules_, &ShellModules::changed, this,
          &WaylandCompositor::arrange);
  systemStatus_ = new SystemStatus(this);
  audioSettings_ = new AudioSettings(this);
  powerSettings_ = new PowerSettings(this);
  sessionActions_ = new SessionActions(this);
  shortcutSettings_ = new ShortcutSettings;
  updateChecker_ = new UpdateChecker(this);
  applyKeyboardPreferences(compositor_.defaultSeat(), desktopPreferences());
  networkStatus_ = new NetworkStatus(this);
  pluginManager_ = new PluginManager(this);
  pluginManager_->loadEnabled();
  for (const auto &error : pluginManager_->errors())
    qWarning().noquote() << error;
  connect(&window_, &QQuickWindow::widthChanged, this, [this] { arrange(); });
  connect(&window_, &QQuickWindow::heightChanged, this, [this] { arrange(); });
  connect(compositor_.defaultSeat(), &QWaylandSeat::keyboardFocusChanged, this,
          [this](QWaylandSurface *surface, QWaylandSurface *) {
            for (const auto &client : clients_) {
              client->frame->focused =
                  client->item && client->item->surface() == surface;
              if (client->frame->focused && !client->desktop)
                focused_ = client.get();
              client->frame->update();
            }
          });
  if (fullscreen)
    window_.showFullScreen();
  else
    window_.show();
  xwayland_ = new XWaylandSupport(this);
  if (qEnvironmentVariableIntValue("LUDASH_DISABLE_XWAYLAND") != 1) {
    auto environment = clientEnvironment_;
    environment.insert(
        "XCURSOR_SIZE",
        QString::number(desktopPreferences().value("cursorSize").toInt()));
    if (xwayland_->start(environment, window_.size()) &&
        !xwayland_->startServer()) {
      qWarning(
          "XWayland could not start; X11 clients will remain unavailable.");
    }
  }
  publishSessionActivationEnvironment();
  // Fcitx is a user-session D-Bus service, not a compositor child. Starting a
  // second fcitx5 here races with the already activated org.fcitx.Fcitx5
  // instance (and nested LunaDash shares that bus with the host desktop).
  // Clients use the existing service through QT/GTK_IM_MODULE=fcitx; if no
  // service exists, Fcitx's normal D-Bus activation owns starting it.
  if (qEnvironmentVariableIntValue("LUDASH_DISABLE_XWAYLAND") != 1 &&
      qEnvironmentVariableIntValue("LUNADASH_DISABLE_CLIPBOARD_BRIDGE") != 1 &&
      !QStandardPaths::findExecutable(QStringLiteral("wl-paste")).isEmpty() &&
      !QStandardPaths::findExecutable(QStringLiteral("wl-copy")).isEmpty() &&
      !QStandardPaths::findExecutable(QStringLiteral("xclip")).isEmpty()) {
    const QString bridge = clipboardBridgePath();
    if (!bridge.isEmpty())
      spawn({}, bridge, false);
  }
  if (startShell) {
    // The shell has dedicated restart handling below, so it must not mark the
    // compositor's generic required-child failure flag. Otherwise a Quickshell
    // crash sets processFailure_ before the restart handler runs and forces the
    // whole SDDM session to exit with status 2.
    if (auto *process = spawn({"--session"}, {}, false)) {
      connect(process, &QProcess::finished, this,
              [this](int code, QProcess::ExitStatus status) {
                if (shuttingDown_ || testStopping_)
                  return;
                if (processFailure_) {
                  QCoreApplication::exit(2);
                  return;
                }

                // A shell crash must not tear down the compositor and every
                // application in the desktop session. Normal shell termination
                // still means the user/session requested shutdown; an abnormal
                // exit is isolated and the shell is restarted after a short
                // delay so input-method/plugin crashes remain debuggable.
                if (status == QProcess::NormalExit && code == 0) {
                  requestShutdown();
                  return;
                }

                qWarning() << "LunaDash shell exited unexpectedly; restarting";
                QTimer::singleShot(250, this, [this] {
                  if (!shuttingDown_ && !testStopping_)
                    spawn({"--session", "--no-welcome"}, {}, false);
                });
              });
    }
  }
  if (startShell && setupComplete())
    QTimer::singleShot(1200, this, [this] {
      if (testStopping_ || shuttingDown_)
        return;
      for (const auto &app :
           desktopPreferences().value("startupApps").toArray())
        spawn({"--app", app.toString()});
    });
  qInfo().noquote() << "LunaDash Wayland socket:" << socket;
}

WaylandCompositor::~WaylandCompositor() {
  shuttingDown_ = true;
  delete screenCapture_;
  screenCapture_ = nullptr;
  delete xwayland_;
  xwayland_ = nullptr;
  disconnect(compositor_.defaultSeat(), nullptr, this, nullptr);
  delete layerShell_;
  layerShell_ = nullptr;
  for (auto &client : clients_) {
    if (client->toplevel)
      disconnect(client->toplevel, nullptr, this, nullptr);
    if (client->item && client->item->surface())
      disconnect(client->item->surface(), nullptr, this, nullptr);
    delete client->frame;
  }
  clients_.clear();
  for (auto *process : processes_)
    if (process->state() != QProcess::NotRunning) {
      process->terminate();
      if (!process->waitForFinished(800)) {
        process->kill();
        process->waitForFinished(800);
      }
    }
  delete shortcutSettings_;
  shortcutSettings_ = nullptr;
}

QProcess *WaylandCompositor::spawn(const QStringList &arguments,
                                   const QString &program, bool required) {
  auto environment = clientEnvironment_;
  if (xwayland_)
    xwayland_->applyEnvironment(environment);
  else
    environment.remove("DISPLAY");
  if (program.isEmpty() && arguments.contains("--session")) {
    // Quickshell's PanelWindow surfaces use wlr-layer-shell. Fcitx's Qt input
    // module renders its client-side candidate panel as a transient xdg_popup.
    // QtWaylandCompositor 6.9 rejects the NULL xdg parent required before
    // zwlr_layer_surface_v1.get_popup can attach that popup to a layer surface,
    // disconnecting the whole Quickshell Wayland client when the IM switches.
    // Keep Fcitx enabled for normal xdg-toplevel applications, but let the shell
    // use Qt's native Wayland text-input path until LunaDash has a complete
    // input-method-v2 bridge / layer-popup-compatible xdg-shell implementation.
    environment.remove("QT_IM_MODULE");
    environment.insert("QT_IM_MODULES", QStringLiteral("wayland"));

    if (!configureShellRendering(
            environment, QFile::exists("/proc/driver/nvidia/version"))) {
      processFailure_ = true;
      qCritical("LUDASH_SHELL_RENDERER must be auto, opengl or software.");
      QTimer::singleShot(0, this, [] { QCoreApplication::exit(2); });
      return nullptr;
    }
    qInfo().noquote() << "LunaDash shell renderer:"
                      << environment.value("LUDASH_SHELL_RENDERER");
  }
  auto *process = new QProcess(this);
  process->setProcessEnvironment(environment);
  process->setProcessChannelMode(QProcess::ForwardedChannels);
  connect(process, &QProcess::errorOccurred, this,
          [this, process, required](QProcess::ProcessError) {
            if (shuttingDown_)
              return;
            if (required)
              processFailure_ = true;
            qWarning().noquote()
                << "LunaDash child process error:" << process->errorString();
          });
  connect(process, &QProcess::finished, this,
          [this, process, required](int code, QProcess::ExitStatus status) {
            if (!shuttingDown_ &&
                (code != 0 || status != QProcess::NormalExit)) {
              if (required)
                processFailure_ = true;
              qWarning().noquote()
                  << "LunaDash child exited abnormally:" << process->program()
                  << "code" << code << "status" << status;
            }
          });
  if (program.isEmpty() && arguments.contains("--session")) {
    connect(process, &QProcess::started, this,
            [this, process] { shellProcessIds_.insert(process->processId()); });
  }
  if (program.isEmpty() && arguments.contains("--session")) {
    const QString sourceConfig =
        QStringLiteral(LUDASH_QML_SOURCE_DIR) + "/shell.qml";
    QString config =
        QFileInfo::exists(sourceConfig)
            ? sourceConfig
            : QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                     "lunadash/shell/shell.qml");
    if (config.isEmpty())
      config = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                      "ludash/shell/shell.qml");
    process->start(QStandardPaths::findExecutable("quickshell"),
                   {"--path", config, "--no-color"});
  } else {
    auto executable = program;
    if (executable.isEmpty()) {
      executable = QCoreApplication::applicationDirPath() + "/lunadash-desktop";
      if (!QFileInfo::exists(executable))
        executable = QCoreApplication::applicationDirPath() + "/ludash-desktop";
    }
    process->start(executable, arguments);
  }
  processes_ << process;
  return process;
}

bool WaylandCompositor::saveScreenshot(const QString &path) {
  return window_.grabWindow().save(path);
}

QJsonObject WaylandCompositor::state() const {
  QJsonArray entries;
  for (const auto &client : clients_)
    if (!client->utility)
      entries.append(QJsonObject{
          {"contentWidth", client->item->width()},
          {"contentHeight", client->item->height()},
          {"contentVisible", client->item->isVisible()},
          {"contentPaintEnabled", client->item->isPaintEnabled()},
          {"bufferWidth", client->item->surface()
                              ? client->item->surface()->bufferSize().width()
                              : 0},
          {"id", client->id},
          {"title", client->toplevel ? client->toplevel->title() : ""},
          {"appId", client->appId},
          {"icon", client->iconName},
          {"desktop", client->desktop},
          {"workspace", client->workspace},
          {"visible", client->frame->isVisible()},
          {"focused", client.get() == focused_},
          {"x", client->frame->x()},
          {"y", client->frame->y()},
          {"width", client->frame->width()},
          {"height", client->frame->height()},
          {"minimized", client->minimized},
          {"maximized", client->maximized},
          {"manualResize", client->manualResize},
          {"mapped", client->mapped}});
  const auto tilingSnapshot =
      tiling_.snapshot(static_cast<TilingWorkspaceId>(workspace_));
  QJsonArray tilingColumns;
  for (const auto &column : tilingSnapshot.columns)
    tilingColumns.append(
        QJsonObject{{"window", static_cast<qint64>(column.window)},
                    {"width", column.width},
                    {"minimized", column.minimized},
                    {"focused", column.focused},
                    {"columnIndex", column.columnIndex},
                    {"rowIndex", column.rowIndex},
                    {"x", column.geometry.x()},
                    {"y", column.geometry.y()},
                    {"height", column.geometry.height()}});
  QJsonArray tilingGroups;
  QSet<int> emittedGroups;
  for (const auto &column : tilingSnapshot.columns) {
    if (emittedGroups.contains(column.columnIndex))
      continue;
    emittedGroups.insert(column.columnIndex);
    QJsonArray members;
    bool groupFocused = false;
    for (const auto memberId : column.columnMembers) {
      const auto placement = std::find_if(
          tilingSnapshot.columns.begin(), tilingSnapshot.columns.end(),
          [memberId](const auto &entry) { return entry.window == memberId; });
      const auto client = std::find_if(
          clients_.begin(), clients_.end(), [memberId](const auto &entry) {
            return entry->id == static_cast<int>(memberId);
          });
      const bool memberFocused =
          placement != tilingSnapshot.columns.end() && placement->focused;
      groupFocused = groupFocused || memberFocused;
      members.append(QJsonObject{
          {"window", static_cast<qint64>(memberId)},
          {"title", client != clients_.end() && (*client)->toplevel
                        ? (*client)->toplevel->title()
                        : QString{}},
          {"appId", client != clients_.end() ? (*client)->appId : QString{}},
          {"icon", client != clients_.end()
                       ? (*client)->iconName
                       : QStringLiteral("application-x-executable")},
          {"minimized", placement != tilingSnapshot.columns.end()
                            ? placement->minimized
                            : false},
          {"focused", memberFocused}});
    }
    tilingGroups.append(QJsonObject{{"index", column.columnIndex},
                                    {"focused", groupFocused},
                                    {"width", column.width},
                                    {"members", members}});
  }
  return {
      {"defaultApps", defaultApplications()},
      {"shellModules", shellModules_->snapshot()},
      {"panelExtent", shellModules_->panelExtent(
                          desktopPreferences().value("panelHeight").toInt())},
      {"panelAtBottom", shellModules_->panelAtBottom()},
      {"audio", audioSettings_->snapshot()},
      {"power", powerSettings_->snapshot()},
      {"sessionActions", sessionActions_->snapshot()},
      {"shortcuts", shortcutSettings_->snapshot()},
      {"update", updateChecker_->snapshot()},
      {"settingsTools", systemSettingsTools()},
      {"display", describeDisplay(&window_)},
      {"settingsSerial", settingsSerial_},
      {"settingsPage", settingsPage_},
      {"pickerSerial", pickerSerial_},
      {"input",
       QJsonObject{{"layout", compositor_.defaultSeat()->keymap()->layout()},
                   {"repeatRate",
                    static_cast<int>(
                        compositor_.defaultSeat()->keyboard()->repeatRate())},
                   {"repeatDelay",
                    static_cast<int>(
                        compositor_.defaultSeat()->keyboard()->repeatDelay())},
                   {"modifierResends", modifierResends_},
                   {"keypadKeyForwards", keypadKeyForwards_}}},
      {"blurReady", blurHealth_->ready.load()},
      {"blurFailed", blurHealth_->failed.load()},
      {"blurFrames", static_cast<int>(blurHealth_->frames.load())},
      {"activeAnimations", animations_->activeCount()},
      {"xwayland", xwayland_ ? xwayland_->snapshot() : QJsonObject{}},
      {"screenCapture",
       QJsonObject{
           {"protocol", "zwlr_screencopy_manager_v1"},
           {"version", 3},
           {"frames", screenCapture_ ? screenCapture_->capturedFrames() : 0},
           {"lastCapture", lastCapture_},
           {"error", captureError_}}},
      {"activationEnvironment",
       QJsonObject{{"published", activationEnvironmentPublished_},
                   {"error", activationEnvironmentError_}}},
      {"appearance", desktopPreferences()},
      {"setupComplete", setupComplete()},
      {"network", networkStatus_->snapshot()},
      {"system", systemStatus_->snapshot()},
      {"wallpaperImage", wallpaperImageUrl()},
      {"workspace", workspace_},
      {"processFailure", processFailure_},
      {"clients", entries},
      {"tiling",
       QJsonObject{
           {"scrollOffset", tilingSnapshot.scrollOffset},
           {"focusedWindow", static_cast<qint64>(tilingSnapshot.focusedWindow)},
           {"columns", tilingColumns},
           {"groups", tilingGroups}}},
      {"layerSurfaces", layerShell_ ? layerShell_->mappedCount() : 0},
      {"language", selectedLanguage()},
      {"translations", languageDictionary(selectedLanguage())},
      {"wallpaper", QSettings().value("appearance/wallpaper", 0).toInt()},
      {"shutdown", testStopping_},
      {"graphicsApi", renderState_->isOpenGLES ? "OpenGL ES" : "OpenGL"},
      {"shaderReady", renderState_->shaderReady.load()},
      {"graphicsFailed", renderState_->failed.load()},
      {"graphicsMajor", renderState_->majorVersion.load()},
      {"graphicsMinor", renderState_->minorVersion.load()}};
}

void WaylandCompositor::resendKeyboardModifiers() {
  auto *seat = compositor_.defaultSeat();
  auto *keyboard = seat ? seat->keyboard() : nullptr;
  auto *surface = seat ? seat->keyboardFocus() : nullptr;
  if (!keyboard || !surface)
    return;
  auto *client =
      QWaylandClient::fromWlClient(&compositor_, surface->waylandClient());
  if (!client)
    return;
  keyboard->sendKeyModifiers(client, compositor_.nextSerial());
  ++modifierResends_;
}

void WaylandCompositor::publishSessionActivationEnvironment() {
  if (qEnvironmentVariableIntValue("LUNADASH_PUBLISH_ACTIVATION_ENV") != 1)
    return;
  auto environment = clientEnvironment_;
  if (xwayland_)
    xwayland_->applyEnvironment(environment);
  QString error;
  if (publishActivationEnvironment(environment, &error)) {
    activationEnvironmentPublished_ = true;
    qInfo("LunaDash published the display variables to the session D-Bus "
          "activation environment.");
    return;
  }
  activationEnvironmentError_ =
      error.isEmpty()
          ? QStringLiteral("dbus-update-activation-environment failed.")
          : error;
  qWarning().noquote()
      << "LunaDash could not publish the session D-Bus activation environment:"
      << activationEnvironmentError_;
}

void WaylandCompositor::captureScreen() {
  const auto path = nextCapturePath();
  if (path.isEmpty() || !saveScreenshot(path)) {
    captureError_ = "Could not write the screenshot.";
    qWarning().noquote() << "LunaDash could not save a screenshot.";
    return;
  }
  lastCapture_ = path;
  captureError_.clear();
  qInfo().noquote() << "LunaDash screenshot:" << lastCapture_;
}

QString WaylandCompositor::nextCapturePath() const {
  auto directory =
      QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
  if (directory.isEmpty())
    directory = QDir::homePath();
  directory += QStringLiteral("/Screenshots");
  if (directory.isEmpty() || !QDir().mkpath(directory))
    return {};
  const auto stamp =
      QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
  QString path =
      directory + QStringLiteral("/lunadash-") + stamp + QStringLiteral(".png");
  for (int index = 1; QFileInfo::exists(path) && index < 1000; ++index)
    path = directory + QStringLiteral("/lunadash-") + stamp +
           QStringLiteral("-") + QString::number(index) +
           QStringLiteral(".png");
  return path;
}

void WaylandCompositor::saveState(const QString &path) {
  QFile file(path);
  if (file.open(QIODevice::WriteOnly))
    file.write(QJsonDocument(state()).toJson());
}

QJsonObject WaylandCompositor::control(const QJsonObject &request) {
  const auto method = request.value("method").toString();
  const auto value = request.value("value").toString();
  if (method == "status")
    return state();
  bool numberValid = false;
  const int number = value.toInt(&numberValid);
  if (method == "workspace" && numberValid && number >= 0 &&
      number < desktopPreferences().value("workspaceCount").toInt()) {
    workspace_ = number;
    arrange();
    synchronizeTilingFocus();
  } else if (method == "language" && (value == "en_US" || value == "zh_TW"))
    QSettings().setValue("appearance/language", value);
  else if (method == "shortcut-capture" &&
           (value == "true" || value == "false")) {
    shortcutCapture_ = value == "true";
  } else if (method == "shortcuts") {
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    QString error;
    if (!document.isObject() ||
        !shortcutSettings_->apply(document.object(), &error))
      return {{"error", error.isEmpty() ? "Expected shortcut JSON." : error}};
  } else if (method == "reset-shortcuts") {
    shortcutSettings_->reset();
  } else if (method == "capture") {
    if (!value.startsWith(QLatin1Char('/')))
      return {{"error", "Capture requires an absolute path."}};
    if (value.size() > 4096)
      return {{"error", "Capture path is too long."}};
    if (QFileInfo::exists(value))
      return {{"error", "Refusing to overwrite " + value}};
    if (!saveScreenshot(value))
      return {{"error", "Could not write " + value}};
    lastCapture_ = value;
    captureError_.clear();
    return {{"path", value}};
  } else if (method == "screenshot") {
    captureScreen();
    if (!captureError_.isEmpty())
      return {{"error", captureError_}};
    return {{"path", lastCapture_}};
  } else if (method == "check-update") {
    updateChecker_->check();
  } else if (method == "send-key") {
    const int key = value == "copy"        ? Qt::Key_C
                    : value == "paste"     ? Qt::Key_V
                    : value == "cut"       ? Qt::Key_X
                    : value == "selectAll" ? Qt::Key_A
                                           : 0;
    if (key == 0)
      return {{"error", "Unknown key action."}};
    auto *seat = compositor_.defaultSeat();
    if (!seat || !seat->keyboardFocus())
      return {{"error", "No application has keyboard focus."}};
    seat->sendKeyEvent(Qt::Key_Control, true);
    seat->sendKeyEvent(key, true);
    seat->sendKeyEvent(key, false);
    seat->sendKeyEvent(Qt::Key_Control, false);
  } else if (method == "open-settings") {
    if (!value.isEmpty() &&
        !QStringList{"general", "appearance", "windows", "shortcuts", "display",
                     "input", "sound", "network", "bluetooth", "power",
                     "applications", "privacy", "system", "devices", "about",
                     "modules"}
             .contains(value))
      return {{"error", "Unknown settings page."}};
    settingsPage_ = value.isEmpty() ? "general" : value;
    settingsSerial_ = (settingsSerial_ + 1) % 1000000;
  } else if (method == "module-validate" || method == "module-save" ||
             method == "module-reset" || method == "module-code-trust" ||
             method == "module-template") {
    QString error;
    bool ok = false;
    if (method == "module-validate")
      ok = shellModules_->validate(value.toUtf8(), &error);
    else if (method == "module-save")
      ok = shellModules_->apply(value.toUtf8(), &error);
    else if (method == "module-reset")
      ok = shellModules_->reset(&error);
    else if (method == "module-template")
      ok = shellModules_->installTemplate(value, &error);
    else if (value == "true" || value == "false")
      ok = shellModules_->setCodeTrusted(value == "true", &error);
    if (!ok)
      return {{"error", error.isEmpty() ? "Invalid module command." : error}};
  } else if (method == "module-error") {
    const auto error = QJsonDocument::fromJson(value.toUtf8()).object();
    shellModules_->reportError(error.value("id").toString(),
                               error.value("error").toString());
  } else if (method == "default-apps") {
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    QString error;
    if (!document.isObject() ||
        !setDefaultApplications(document.object(), &error))
      return {{"error", error.isEmpty() ? "Expected a JSON object." : error}};
  } else if (method == "launch-default" &&
             (value == "terminal" || value == "files")) {
    QString error;
    auto command = defaultApplicationCommand(value, &error);
    if (!error.isEmpty())
      return {{"error", error}};
    if (command.isEmpty())
      spawn({"--app", value, "--builtin"});
    else {
      const auto program = command.takeFirst();
      spawn(command, program);
    }
  } else if (method == "system-tool") {
    auto command = systemSettingsCommand(value);
    if (command.isEmpty())
      return {{"error", "This settings tool is unavailable. Install the "
                        "package shown in Settings."}};
    const auto program = command.takeFirst();
    spawn(command, program);
  } else if (method == "audio") {
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    QString error;
    if (!document.isObject() ||
        !audioSettings_->apply(document.object(), &error))
      return {{"error", error.isEmpty() ? "Invalid audio setting." : error}};
  } else if (method == "network") {
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    QString error;
    if (!document.isObject() ||
        !networkStatus_->execute(document.object(), &error))
      return {{"error", error.isEmpty() ? "Invalid network request." : error}};
  } else if (method == "power-profile") {
    QString error;
    if (!powerSettings_->apply(value, &error))
      return {{"error", error}};
  } else if (method == "session-action") {
    QString error;
    if (!isValidSessionAction(value) ||
        !sessionActions_->execute(value, &error))
      return {{"error", error.isEmpty() ? "Invalid session action." : error}};
  } else if (method == "desktop-size") {
    QString error;
    if (!resizeNestedDesktop(&window_, value, &error))
      return {{"error", error}};
  } else if (method == "reset-preferences") {
    QSettings settings;
    settings.remove("desktop");
    settings.sync();
    applyKeyboardPreferences(compositor_.defaultSeat(), desktopPreferences());
    arrange();
  } else if (method == "appearance") {
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(value.toUtf8(), &parseError);
    QString error;
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
      return {{"error", "Expected a JSON object of desktop preferences."}};
    if (!updateDesktopPreferences(document.object(), &error))
      return {{"error", error}};
    applyKeyboardPreferences(compositor_.defaultSeat(), desktopPreferences());
    arrange();
  } else if (method == "launch-x11") {
    QString error;
    if (!xwayland_ || !xwayland_->launch(QProcess::splitCommand(value), &error))
      return {{"error", error.isEmpty() ? "XWayland is unavailable." : error}};
  } else if (method == "launch-command") {
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    if (!document.isArray() || document.array().isEmpty())
      return {{"error", "Expected a non-empty command array."}};
    QStringList command;
    for (const auto &entry : document.array()) {
      if (!entry.isString() || entry.toString().size() > 1024)
        return {{"error", "Command arguments must be short strings."}};
      command << entry.toString();
    }
    const QStringList originalCommand = command;
    if (isChromiumApplication(command.first()))
      ensureWaylandChromiumFlags(command);
    const auto executable = command.takeFirst();
    if (auto *process = spawn(command, executable, false)) {
      auto launchedPid = std::make_shared<qint64>(0);
      connect(process, &QProcess::started, this,
              [process, launchedPid] { *launchedPid = process->processId(); });
      connect(
          process, &QProcess::finished, this,
          [this, process, originalCommand,
           launchedPid](int code, QProcess::ExitStatus status) {
            if (shuttingDown_ || testStopping_ || !xwayland_)
              return;
            // A crash is a real application failure, not evidence that its
            // Wayland backend is unsupported. Do not immediately launch a
            // second copy under XWayland after SIGSEGV/SIGABRT/coredump.
            // Compatibility retry is only for a clean startup refusal
            // (normal process exit with a non-zero code before a surface).
            if (status != QProcess::NormalExit || code == 0)
              return;
            const qint64 pid = *launchedPid;
            const bool createdSurface = std::any_of(
                clients_.cbegin(), clients_.cend(), [pid](const auto &client) {
                  return pid > 0 && client->item && client->item->surface() &&
                         client->item->surface()->client() &&
                         client->item->surface()->client()->processId() == pid;
                });
            if (createdSurface)
              return;
            QString error;
            if (!xwayland_->launch(originalCommand, &error))
              qWarning().noquote()
                  << "LunaDash compatibility retry failed for"
                  << process->program() << ":"
                  << (error.isEmpty() ? "unknown XWayland error" : error);
            else
              qInfo().noquote()
                  << "LunaDash retried failed native application through "
                     "XWayland:"
                  << process->program();
          });
    }
  } else if (method == "finish-setup")
    setSetupComplete(true);
  else if (method == "setup")
    setSetupComplete(false);
  else if (method == "configure-network") {
    auto program = QStandardPaths::findExecutable("nm-connection-editor");
    QStringList arguments;
    if (program.isEmpty() &&
        !QStandardPaths::findExecutable("nmtui").isEmpty()) {
      for (const auto &terminal : {"konsole", "alacritty", "foot"}) {
        program = QStandardPaths::findExecutable(terminal);
        if (!program.isEmpty()) {
          arguments = {"-e", "nmtui"};
          break;
        }
      }
    }
    if (program.isEmpty())
      return {{"error", "Install nm-connection-editor, or NetworkManager nmtui "
                        "with foot, konsole or alacritty."}};
    spawn(arguments, program);
  } else if (method == "choose-wallpaper") {
    if (!value.isEmpty())
      return {{"error", "choose-wallpaper does not accept a value."}};
    settingsPage_ = "appearance";
    settingsSerial_ = (settingsSerial_ + 1) % 1000000;
    pickerSerial_ = (pickerSerial_ + 1) % 1000000;
  } else if (method == "wallpaper-image") {
    QString error;
    const QUrl url(value);
    if (!setWallpaperImage(url.isLocalFile() ? url.toLocalFile() : value,
                           &error))
      return {{"error", error}};
  } else if (method == "wallpaper-default") {
    QSettings().remove("appearance/wallpaperImage");
    QSettings().setValue("appearance/wallpaperMode", "image");
  } else if (method == "wallpaper" && numberValid && number >= 0 &&
             number <= 1) {
    QSettings().setValue("appearance/wallpaper", number);
    QSettings().setValue("appearance/wallpaperMode", "shader");
    wallpaper_->setPalette(number);
  } else if (method == "group-window") {
    int window = 0;
    int target = 0;
    if (!groupWindowIds(value, &window, &target))
      return {
          {"error", "Expected bounded JSON {\"window\":ID,\"target\":ID}."}};
    if (!tiling_.groupWith(window, target))
      return {{"error",
               "Windows must be distinct tiled windows in the same "
               "workspace, and the target column must have capacity."}};
    const auto groupedClient = std::find_if(
        clients_.begin(), clients_.end(),
        [window](const auto &entry) { return entry->id == window; });
    if (groupedClient != clients_.end()) {
      (*groupedClient)->item->setPrimary();
      focus(groupedClient->get());
    }
    arrange();
    QTimer::singleShot(0, this, [this] { arrange(); });
    QTimer::singleShot(260, this, [this, window, target] {
      for (const auto &client : clients_)
        if (client->id == window || client->id == target)
          client->lastSize = QSize();
      arrange();
    });
  } else if (method == "expel-window") {
    const auto window = textWindowId(value);
    if (!window)
      return {{"error", "expel-window requires a positive integer window ID."}};
    if (!tiling_.expel(*window))
      return {{"error", "Window must be a member of a tiled group."}};
    arrange();
    synchronizeTilingFocus();
  } else if (method == "quit")
    QTimer::singleShot(0, this, &WaylandCompositor::requestShutdown);
  else if (method == "focus" || method == "close" || method == "minimize") {
    const auto window = textWindowId(value);
    if (!window)
      return {{"error", "Window command requires a positive integer ID."}};
    for (const auto &client : clients_)
      if (client->id == *window) {
        if (method == "close") {
          if (client->toplevel)
            client->toplevel->sendClose();
        } else if (method == "minimize") {
          client->minimized = true;
          tiling_.setMinimized(client->id, true);
          arrange();
          synchronizeTilingFocus();
        } else {
          if (!client->floating && !client->desktop &&
              !tiling_.setMinimized(client->id, false))
            return {
                {"error",
                 "The tiled group has no capacity to restore this window."}};
          workspace_ = client->workspace;
          client->minimized = false;
          if (!client->floating && !client->desktop)
            tiling_.focus(client->id);
          arrange();
          focus(client.get());
        }
        return state();
      }
    return {{"error", "Unknown window"}};
  } else
    return {{"error", "Invalid command or value"}};
  return state();
}

bool WaylandCompositor::hasProcessFailure() const {
  return processFailure_ || renderState_->failed || blurHealth_->failed ||
         !renderState_->shaderReady;
}

void WaylandCompositor::closeTestSession(
    const std::function<void(bool)> &finished) {
  testStopping_ = true;
  for (const auto &client : clients_)
    if (client->toplevel)
      client->toplevel->sendClose();
  auto *timer = new QTimer(this);
  auto elapsed = std::make_shared<int>(0);
  connect(timer, &QTimer::timeout, this, [this, timer, elapsed, finished] {
    *elapsed += 50;
    if (clients_.empty() && xwayland_)
      xwayland_->stop();
    const bool compatibilityFinished =
        (!xwayland_ || xwayland_->stopped()) && animations_->activeCount() == 0;
    const bool processesFinished = std::all_of(
        processes_.begin(), processes_.end(), [](const auto *process) {
          return process->state() == QProcess::NotRunning;
        });
    if ((clients_.empty() && processesFinished && compatibilityFinished) ||
        *elapsed >= 5000) {
      timer->stop();
      timer->deleteLater();
      const bool clean = clients_.empty() && processesFinished &&
                         compatibilityFinished && !processFailure_;
      if (!clean) {
        qWarning() << "LuDash test session did not shut down cleanly."
                   << "clients:" << clients_.size()
                   << "animations:" << animations_->activeCount()
                   << "layers:" << layerShell_->mappedCount()
                   << "XWayland stopped:"
                   << (!xwayland_ || xwayland_->stopped())
                   << "child failure:" << processFailure_;
        for (const auto *process : processes_)
          if (process->state() != QProcess::NotRunning)
            qWarning().noquote()
                << "Pending child:" << process->program()
                << process->arguments() << "PID:" << process->processId()
                << "state:" << process->state();
      }
      finished(clean);
    }
  });
  timer->start(50);
}

void WaylandCompositor::requestShutdown() {
  logoutPending_ = true;
  bool hasApplications = false;
  for (const auto &client : clients_)
    if (!client->desktop && client->toplevel) {
      hasApplications = true;
      client->toplevel->sendClose();
    }
  if (!hasApplications)
    QCoreApplication::exit(hasProcessFailure() ? 2 : 0);
}

void WaylandCompositor::addWindow(QWaylandXdgToplevel *toplevel,
                                  QWaylandXdgSurface *surface) {
  auto client = std::make_unique<ClientWindow>();
  client->id = nextWindowId_++;
  client->toplevel = toplevel;
  client->appId = toplevel->appId();
  client->utility = isUtilityWindow(client->appId);
  client->workspace = workspace_;
  client->frame = new WindowFrame(window_.contentItem());
  client->frame->title = toplevel->title();
  client->iconName = client->utility
                         ? QString()
                         : windowIconName(client->appId, client->frame->title);
  const auto initialPolicy =
      initialWindowPolicy(client->appId, client->frame->title);
  client->maximized = initialPolicy.maximized;
  client->floating = desktopPreferences().value("defaultFloating").toBool() ||
                     initialPolicy.floating;
  client->preferredFloatingSize = initialPolicy.floatingSize;
  client->initialRuleApplied = false;
  client->frame->setVisible(false);
  client->frame->setOpacity(.999);
  client->blur = new BlurItem(blurHealth_, client->frame);
  client->item = new QWaylandQuickShellSurfaceItem(client->frame);
  client->item->setShellSurface(surface);
  client->item->setOutput(output_);
  client->item->setAutoCreatePopupItems(true);
  client->item->setFocusOnClick(true);
  connect(client->item, &QWaylandQuickItem::surfaceDestroyed, client->item,
          [item = client->item] {
            if (item)
              item->setInputEventsEnabled(false);
          });
  auto *current = client.get();
  current->desktop = current->appId == "ludash-shell" &&
                     current->item->surface() &&
                     shellProcessIds_.contains(
                         current->item->surface()->client()->processId());
  clients_.push_back(std::move(client));
  current->frame->action = [this, current](WindowFrameAction action) {
    if (action == WindowFrameAction::Close) {
      if (current->toplevel)
        current->toplevel->sendClose();
    } else if (action == WindowFrameAction::Minimize) {
      current->minimized = true;
      tiling_.setMinimized(current->id, true);
      arrange();
      synchronizeTilingFocus();
    } else {
      focus(current);
    }
  };
  connect(toplevel, &QWaylandXdgToplevel::titleChanged, this, [this, current] {
    current->frame->title = current->toplevel->title();
    current->iconName =
        current->utility
            ? QString()
            : windowIconName(current->appId, current->frame->title);
    if (!current->mapped) {
      const auto policy =
          initialWindowPolicy(current->appId, current->frame->title);
      current->maximized = policy.maximized;
      current->floating =
          desktopPreferences().value("defaultFloating").toBool() ||
          policy.floating || current->toplevel->parentToplevel();
      current->preferredFloatingSize = policy.floatingSize;
      arrange();
    }
    current->frame->update();
  });
  connect(toplevel, &QWaylandXdgToplevel::appIdChanged, this, [this, current] {
    current->appId = current->toplevel->appId();
    current->utility = isUtilityWindow(current->appId);
    current->iconName =
        current->utility
            ? QString()
            : windowIconName(current->appId, current->frame->title);
    current->desktop = current->appId == "ludash-shell" &&
                       shellProcessIds_.contains(
                           current->item->surface()->client()->processId());
    if (!current->mapped) {
      const auto policy =
          initialWindowPolicy(current->appId, current->frame->title);
      current->maximized = policy.maximized;
      current->floating =
          desktopPreferences().value("defaultFloating").toBool() ||
          policy.floating || current->toplevel->parentToplevel();
      current->preferredFloatingSize = policy.floatingSize;
    }
    if (current->desktop)
      tiling_.remove(current->id);
    arrange();
  });
  connect(surface->surface(), &QWaylandSurface::hasContentChanged, this,
          [this, current] {
            const auto *attached = current->item->surface();
            const bool hasContent = attached && attached->hasContent();
            const bool newlyMapped = !current->mapped && hasContent;
            if (newlyMapped && !current->initialRuleApplied) {
              const auto policy =
                  initialWindowPolicy(current->appId, current->frame->title);
              current->maximized = policy.maximized;
              current->floating =
                  desktopPreferences().value("defaultFloating").toBool() ||
                  policy.floating || current->toplevel->parentToplevel();
              current->preferredFloatingSize = policy.floatingSize;
              current->initialRuleApplied = true;
            }
            current->mapped = hasContent;
            if (!hasContent)
              tiling_.remove(current->id);
            if (current->utility) {
              current->item->setInputEventsEnabled(false);
              current->frame->setVisible(false);
              return;
            }
            if (newlyMapped && !current->desktop && !current->revealed) {
              const QRect area = workArea();
              const int gap = desktopPreferences().value("gap").toInt();
              current->frame->setX(area.x() + area.width() + gap);
              current->frame->setY(area.y());
              current->frame->setSize(area.size());
              current->revealed = true;
            }
            arrange();
            if (newlyMapped && !current->desktop) {
              pluginManager_->windowOpened(current->frame);
              focus(current);
            }
          });
  connect(toplevel, &QWaylandXdgToplevel::parentToplevelChanged, this,
          [this, current] {
            const auto policy =
                initialWindowPolicy(current->appId, current->frame->title);
            current->floating =
                current->toplevel->parentToplevel() ||
                desktopPreferences().value("defaultFloating").toBool() ||
                policy.floating;
            if (current->floating)
              tiling_.remove(current->id);
            arrange();
          });
  connect(toplevel, &QWaylandXdgToplevel::setMinimized, this, [this, current] {
    current->minimized = true;
    tiling_.setMinimized(current->id, true);
    arrange();
    synchronizeTilingFocus();
  });

  connect(toplevel, &QObject::destroyed, this, [this, current] {
    if (resizing_ == current) {
      resizing_ = nullptr;
      resizeOriginalWidths_.clear();
      if (resizeGuide_)
        resizeGuide_->setVisible(false);
    }
    if (focused_ == current)
      focused_ = nullptr;
    tiling_.remove(current->id);
    if (current->item && current->item->surface())
      disconnect(current->item->surface(), nullptr, this, nullptr);
    current->frame->action = {};
    current->item->setInputEventsEnabled(false);
    auto *frame = current->frame;
    if (frame->isVisible())
      animations_->hide(frame, [frame] { frame->deleteLater(); });
    else
      frame->deleteLater();
    std::erase_if(clients_, [current](const auto &entry) {
      return entry.get() == current;
    });
    arrange();
    synchronizeTilingFocus();
    if (logoutPending_) {
      const bool hasApplications =
          std::any_of(clients_.begin(), clients_.end(),
                      [](const auto &entry) { return !entry->desktop; });
      if (!hasApplications)
        QCoreApplication::exit(hasProcessFailure() ? 2 : 0);
    }
  });
  arrange();
}

void WaylandCompositor::configure(ClientWindow *client,
                                  const QRect &rectangle) {
  const int border = 0;
  const int title = 0;
  const bool animate = !client->desktop && client != resizing_ &&
                       desktopPreferences().value("animations").toBool();
  client->frame->moveTo(rectangle.topLeft(), animate);
  client->frame->setSize(rectangle.size());
  client->blur->setSize(rectangle.size());
  client->item->setPosition(QPointF(border, title));
  const QSize size(std::max(1, rectangle.width() - 2 * border),
                   std::max(1, rectangle.height() - title - border));
  client->item->setSize(QSizeF(size));
  const QRect area = workArea();
  const bool splitTile =
      !client->floating && !client->desktop &&
      (rectangle.width() < area.width() || rectangle.height() < area.height());
  client->frame->setClip(splitTile);
  const bool configuredMaximized =
      client->maximized && rectangle.size() == workArea().size();
  if ((size != client->lastSize ||
       configuredMaximized != client->lastConfiguredMaximized) &&
      client->toplevel) {
    client->lastSize = size;
    client->lastConfiguredMaximized = configuredMaximized;
    QList<QWaylandXdgToplevel::State> states;
    if (configuredMaximized)
      states.append(QWaylandXdgToplevel::MaximizedState);
    if (compositor_.defaultSeat()->keyboardFocus() == client->item->surface())
      states.append(QWaylandXdgToplevel::ActivatedState);
    client->toplevel->sendConfigure(size, states);
  }
}

QRect WaylandCompositor::workArea() const {
  const auto preferences = desktopPreferences();
  const int gap = preferences.value("gap").toInt();
  const int extent =
      shellModules_
          ? shellModules_->panelExtent(preferences.value("panelHeight").toInt())
          : preferences.value("panelHeight").toInt();
  const bool bottom = shellModules_ && shellModules_->panelAtBottom();
  const int top = (bottom ? 0 : extent) + gap;
  return QRect(
      gap, top, std::max(1, window_.width() - gap * 2),
      std::max(1, window_.height() - top - gap - (bottom ? extent : 0)));
}

void WaylandCompositor::beginInteractiveResize(ClientWindow *client,
                                               const QPointF &position) {
  if (!client || client->desktop || client->utility || client->minimized)
    return;
  resizing_ = client;
  resizePointerStart_ = position;
  resizeStartGeometry_ = QRect(static_cast<int>(client->frame->x()),
                               static_cast<int>(client->frame->y()),
                               static_cast<int>(client->frame->width()),
                               static_cast<int>(client->frame->height()));
  resizeGuideGeometry_ = client->resizeGuideGeometry.isValid()
                             ? client->resizeGuideGeometry
                             : resizeStartGeometry_;
  resizeOriginalWidths_.clear();
  if (!client->floating) {
    const auto snapshot = tiling_.snapshot(workspace_);
    QSet<int> seenColumns;
    for (const auto &entry : snapshot.columns) {
      if (seenColumns.contains(entry.columnIndex))
        continue;
      seenColumns.insert(entry.columnIndex);
      resizeOriginalWidths_.insert(static_cast<int>(entry.window), entry.width);
      if (entry.window == static_cast<TilingWindowId>(client->id))
        resizeGuideGeometry_ = entry.geometry;
    }
  }
  client->resizeGuideGeometry = resizeGuideGeometry_;
  client->manualResize = true;
  client->manualGeometry = resizeStartGeometry_;
  client->maximized = false;
  int visibleTiled = 0;
  for (const auto &entry : clients_)
    if (entry->mapped && !entry->minimized && !entry->desktop &&
        !entry->floating && entry->workspace == workspace_)
      ++visibleTiled;
  if (resizeGuide_ && visibleTiled > 1) {
    resizeGuide_->setGuide(
        resizeGuideGeometry_,
        QColor(desktopPreferences().value("accent").toString()));
    resizeGuide_->setVisible(true);
  }
  focus(client);
}

void WaylandCompositor::updateInteractiveResize(const QPointF &position) {
  if (!resizing_)
    return;
  const QPoint delta = (position - resizePointerStart_).toPoint();
  const QRect area = workArea();
  const int minimumWidth = 180;
  const int minimumHeight = 120;
  int width = std::max(minimumWidth, resizeStartGeometry_.width() + delta.x());
  int height =
      std::max(minimumHeight, resizeStartGeometry_.height() + delta.y());
  width =
      std::min(width, std::max(minimumWidth,
                               area.right() - resizeStartGeometry_.x() + 1));
  height =
      std::min(height, std::max(minimumHeight,
                                area.bottom() - resizeStartGeometry_.y() + 1));
  QRect desired(resizeStartGeometry_.topLeft(), QSize(width, height));
  resizing_->manualResize = true;
  resizing_->manualGeometry = desired;

  if (!resizing_->floating && !resizeOriginalWidths_.isEmpty()) {
    for (auto it = resizeOriginalWidths_.cbegin();
         it != resizeOriginalWidths_.cend(); ++it)
      tiling_.resize(it.key(), it.value());

    if (width > resizeGuideGeometry_.width()) {
      const int requestedExtra = width - resizeGuideGeometry_.width();
      int totalAvailable = 0;
      for (auto it = resizeOriginalWidths_.cbegin();
           it != resizeOriginalWidths_.cend(); ++it)
        if (it.key() != resizing_->id)
          totalAvailable += std::max(0, it.value() - minimumWidth);
      const int appliedExtra = std::min(requestedExtra, totalAvailable);
      desired.setWidth(resizeGuideGeometry_.width() + appliedExtra);
      resizing_->manualGeometry = desired;
      tiling_.resize(resizing_->id, desired.width());
      if (appliedExtra > 0 && totalAvailable > 0) {
        int remaining = appliedExtra;
        QList<int> neighbours;
        for (auto it = resizeOriginalWidths_.cbegin();
             it != resizeOriginalWidths_.cend(); ++it)
          if (it.key() != resizing_->id && it.value() > minimumWidth)
            neighbours.append(it.key());
        for (qsizetype index = 0; index < neighbours.size(); ++index) {
          const int id = neighbours[index];
          const int original = resizeOriginalWidths_.value(id);
          const int available = std::max(0, original - minimumWidth);
          const int shrink =
              index + 1 == neighbours.size()
                  ? remaining
                  : std::min(remaining, static_cast<int>(std::round(
                                            static_cast<double>(appliedExtra) *
                                            available / totalAvailable)));
          tiling_.resize(id, std::max(minimumWidth, original - shrink));
          remaining -= shrink;
        }
      }
    }
  }
  arrange();
}

void WaylandCompositor::restoreResizeGuide(ClientWindow *client) {
  if (!client)
    return;
  for (auto it = resizeOriginalWidths_.cbegin();
       it != resizeOriginalWidths_.cend(); ++it)
    tiling_.resize(it.key(), it.value());
  client->manualResize = false;
  client->manualGeometry = {};
  client->resizeGuideGeometry = {};
  arrange();
}

void WaylandCompositor::endInteractiveResize() {
  if (!resizing_)
    return;
  ClientWindow *client = resizing_;
  const QRect geometry = client->manualGeometry;
  const QRect guide = resizeGuideGeometry_;
  const bool nearGuide = guide.isValid() &&
                         std::abs(geometry.x() - guide.x()) <= 24 &&
                         std::abs(geometry.y() - guide.y()) <= 24 &&
                         std::abs(geometry.width() - guide.width()) <= 24 &&
                         std::abs(geometry.height() - guide.height()) <= 24;
  resizing_ = nullptr;
  if (resizeGuide_)
    resizeGuide_->setVisible(false);
  if (nearGuide)
    restoreResizeGuide(client);
  resizeOriginalWidths_.clear();
  resizeStartGeometry_ = {};
  resizeGuideGeometry_ = {};
}

void WaylandCompositor::arrange() {
  if (wallpaper_)
    wallpaper_->setSize(window_.size());
  const auto preferences = desktopPreferences();
  animations_->setDuration(preferences.value("animations").toBool()
                               ? preferences.value("animationDuration").toInt()
                               : 0);
  const int count = preferences.value("workspaceCount").toInt();
  workspace_ = std::min(workspace_, count - 1);
  for (const auto &client : clients_)
    client->workspace = std::min(client->workspace, count - 1);
  const QRect area = workArea();
  const int gap = preferences.value("gap").toInt();
  const int defaultColumnWidth =
      std::clamp(qRound(area.width() *
                        preferences.value("masterRatio").toInt() / 100.0),
                 1, std::max(1, area.width()));
  tiling_.setGap(gap);
  for (const auto &client : clients_) {
    const bool tiled = client->mapped && !client->desktop &&
                       !client->floating && !client->utility;
    if (tiled) {
      tiling_.insert(static_cast<TilingWorkspaceId>(client->workspace),
                     client->id, defaultColumnWidth);
      tiling_.moveToWorkspace(
          client->id, static_cast<TilingWorkspaceId>(client->workspace));
      tiling_.setMinimized(client->id, client->minimized);
      if (client->maximized)
        tiling_.resize(client->id, area.width());
    } else
      tiling_.remove(client->id);
  }
  for (const auto &client : clients_) {
    if (client->utility) {
      client->item->setInputEventsEnabled(false);
      client->frame->setVisible(false);
      client->presented = false;
      continue;
    }
    client->frame->accent = QColor(preferences.value("accent").toString());
    client->frame->update();
    client->blur->setRadius(preferences.value("blur").toBool() &&
                                    !client->desktop
                                ? preferences.value("blurRadius").toInt()
                                : 0);
    client->item->setOpacity(
        client->desktop ? 1
                        : preferences.value("windowOpacity").toInt() / 100.0);
    const bool visible = client->mapped && !client->minimized &&
                         (client->desktop || client->workspace == workspace_);
    if (visible != client->presented) {
      client->presented = visible;
      client->item->setInputEventsEnabled(visible);
      if (visible)
        animations_->show(client->frame);
      else
        animations_->hide(client->frame);
    }
    if (client->desktop) {
      client->frame->setZ(-100);
      configure(client.get(), QRect(0, 0, window_.width(), window_.height()));
    } else if (client->workspace == workspace_ && !client->minimized) {
      client->frame->setZ(client->floating ? 10 : 1);
      if (client->floating) {
        const QSize preferred =
            client->preferredFloatingSize.isValid()
                ? client->preferredFloatingSize
                : QSize(720, 500);
        const QSize bounded(std::min(preferred.width(), area.width()),
                            std::min(preferred.height(), area.height()));
        const QRect defaultFloatingGeometry(
            area.x() + (area.width() - bounded.width()) / 2,
            area.y() + (area.height() - bounded.height()) / 2,
            bounded.width(), bounded.height());
        const QRect floatingGeometry =
            client->manualResize && client->manualGeometry.isValid()
                ? client->manualGeometry
                : defaultFloatingGeometry;
        configure(client.get(), floatingGeometry);
      } else
        client->frame->setZ(1);
    }
  }
  const auto placements =
      tiling_.layout(static_cast<TilingWorkspaceId>(workspace_), area);
  for (const auto &placement : placements) {
    const auto found = std::find_if(
        clients_.begin(), clients_.end(), [&placement](const auto &client) {
          return client->id == static_cast<int>(placement.window);
        });
    if (found == clients_.end() || (*found)->minimized)
      continue;
    auto *client = found->get();
    if (!client->manualResize) {
      client->resizeGuideGeometry = placement.geometry;
      configure(client, placement.geometry);
    } else {
      if (!client->resizeGuideGeometry.isValid())
        client->resizeGuideGeometry = placement.geometry;
      QRect manual = client->manualGeometry.isValid() ? client->manualGeometry
                                                      : placement.geometry;
      manual.moveLeft(placement.geometry.x());
      manual.moveTop(placement.geometry.y());
      client->manualGeometry = manual;
      configure(client, manual);
    }
  }
  if (focused_ && focused_->frame && focused_->mapped && !focused_->minimized &&
      !focused_->desktop && focused_->workspace == workspace_)
    focused_->frame->setZ(focused_->floating ? 20 : 2);
  window_.setTitle(
      QString("LunaDash Wayland · workspace %1").arg(workspace_ + 1));
}

void WaylandCompositor::focus(ClientWindow *client) {
  if (!client || !client->mapped || !client->item ||
      !client->frame->isVisible())
    return;
  focused_ = client;
  if (!client->floating && !client->desktop)
    tiling_.focus(client->id);
  client->item->takeFocus();
  pluginManager_->windowFocused(client->frame);
  client->frame->setZ(client->maximized ? 30 : (client->floating ? 20 : 2));
}

void WaylandCompositor::focusNext(int direction) {
  QList<ClientWindow *> visible;
  for (const auto &client : clients_)
    if (!client->desktop && client->mapped && !client->minimized &&
        client->workspace == workspace_)
      visible << client.get();
  if (visible.isEmpty()) {
    compositor_.defaultSeat()->setKeyboardFocus(nullptr);
    for (const auto &client : clients_)
      if (client->desktop)
        client->item->takeFocus();
    focused_ = nullptr;
    return;
  }
  const qsizetype index = visible.indexOf(focused_);
  focus(visible[(index + direction + visible.size()) % visible.size()]);
}

void WaylandCompositor::synchronizeTilingFocus() {
  const auto target =
      tiling_.snapshot(static_cast<TilingWorkspaceId>(workspace_))
          .focusedWindow;
  if (target != 0) {
    const auto client = std::find_if(
        clients_.begin(), clients_.end(), [target](const auto &entry) {
          return entry->id == static_cast<int>(target) && entry->mapped &&
                 !entry->minimized && !entry->floating && !entry->desktop;
        });
    if (client != clients_.end()) {
      focus(client->get());
      return;
    }
  }
  focusNext(1);
}

ClientWindow *WaylandCompositor::clientAt(const QPointF &position) const {
  for (auto it = clients_.rbegin(); it != clients_.rend(); ++it) {
    auto *client = it->get();
    if (!client->frame || client->desktop || !client->mapped ||
        client->minimized || !client->frame->isVisible())
      continue;
    const QRectF geometry(client->frame->x(), client->frame->y(),
                          client->frame->width(), client->frame->height());
    if (geometry.contains(position))
      return client;
  }
  return nullptr;
}

bool WaylandCompositor::eventFilter(QObject *watched, QEvent *event) {
  if (watched == &window_ && (event->type() == QEvent::MouseMove ||
                              event->type() == QEvent::HoverMove ||
                              event->type() == QEvent::MouseButtonPress ||
                              event->type() == QEvent::MouseButtonRelease)) {
    auto *mouse = static_cast<QMouseEvent *>(event);
    pointerPosition_ = mouse->position();
    if (event->type() == QEvent::MouseButtonPress &&
        desktopPreferences().value("altMouseResize").toBool() &&
        mouse->button() == Qt::RightButton &&
        mouse->modifiers().testFlag(Qt::AltModifier)) {
      if (auto *target = clientAt(mouse->position())) {
        beginInteractiveResize(target, mouse->position());
        if (resizing_) {
          event->accept();
          return true;
        }
      }
    }
    if (resizing_ && event->type() == QEvent::MouseMove &&
        mouse->buttons().testFlag(Qt::RightButton)) {
      updateInteractiveResize(mouse->position());
      event->accept();
      return true;
    }
    if (resizing_ && event->type() == QEvent::MouseButtonRelease &&
        mouse->button() == Qt::RightButton) {
      updateInteractiveResize(mouse->position());
      endInteractiveResize();
      event->accept();
      return true;
    }
  }
  if (watched == &window_ && event->type() == QEvent::Close) {
    event->ignore();
    requestShutdown();
    return true;
  }
  if (watched == &window_ && (event->type() == QEvent::KeyPress ||
                              event->type() == QEvent::KeyRelease)) {
    auto *key = static_cast<QKeyEvent *>(event);
    if (qEnvironmentVariableIntValue("LUNADASH_INPUT_DEBUG") == 1) {
      qInfo().noquote() << "LunaDash key event:"
                        << (event->type() == QEvent::KeyPress ? "press"
                                                              : "release")
                        << "key" << key->key() << "scan"
                        << key->nativeScanCode() << "mods"
                        << static_cast<int>(key->modifiers()) << "autoRepeat"
                        << key->isAutoRepeat();
    }
    handleKeyboardLockKey(key->key(), event->type() == QEvent::KeyPress,
                          key->isAutoRepeat());

    // Qt Wayland Compositor's modifier-repair path treats KeypadModifier like
    // Shift/Ctrl/Alt mismatch and can send a modifiers event with a zero locked
    // mask. That clears NumLock/CapsLock for the client. Forward keypad keys
    // directly from their native XKB scan code and consume the QKeyEvent so
    // QWaylandQuickItem does not forward a second copy.
    if (key->modifiers().testFlag(Qt::KeypadModifier)) {
      auto *seat = compositor_.defaultSeat();
      const auto scanCode = static_cast<uint>(key->nativeScanCode());
      if (seat && seat->keyboardFocus() && scanCode >= 8) {
        if (event->type() == QEvent::KeyPress)
          seat->keyboard()->sendKeyPressEvent(scanCode);
        else
          seat->keyboard()->sendKeyReleaseEvent(scanCode);
        ++keypadKeyForwards_;
        return true;
      }
    }

    const bool clearsLockedMask =
        key->key() == Qt::Key_NumLock || key->key() == Qt::Key_CapsLock ||
        key->key() == Qt::Key_ScrollLock || key->key() == Qt::Key_Meta ||
        key->key() == Qt::Key_AltGr;
    if (clearsLockedMask)
      QTimer::singleShot(0, this, &WaylandCompositor::resendKeyboardModifiers);
    if (event->type() == QEvent::KeyRelease && consumedKeys_.remove(key->key()))
      return true;
    if (shortcutCapture_)
      return QObject::eventFilter(watched, event);

    const bool modifierOnly =
        key->key() == Qt::Key_Shift || key->key() == Qt::Key_Control ||
        key->key() == Qt::Key_Alt || key->key() == Qt::Key_AltGr ||
        key->key() == Qt::Key_Meta || key->key() == Qt::Key_CapsLock ||
        key->key() == Qt::Key_NumLock || key->key() == Qt::Key_ScrollLock;

    // Modifier-only events belong to the focused client/input method. Do not
    // run them through global-shortcut normalization or synthesize a compositor
    // action. In particular, Shift is commonly used as an input-method hotkey,
    // but this rule is generic for every modifier-only key.
    if (modifierOnly || event->type() != QEvent::KeyPress ||
        key->isAutoRepeat())
      return QObject::eventFilter(watched, event);

    const QString action = shortcutSettings_->actionFor(*key);
    if (action.isEmpty())
      return QObject::eventFilter(watched, event);

    auto groupAdjacent = [this](int direction) {
      if (!focused_ || focused_->floating || focused_->desktop)
        return;
      const auto snapshot = tiling_.snapshot(workspace_);
      const auto current = std::find_if(
          snapshot.columns.begin(), snapshot.columns.end(),
          [this](const auto &entry) {
            return entry.window == static_cast<TilingWindowId>(focused_->id);
          });
      if (current == snapshot.columns.end())
        return;
      const int targetColumn = current->columnIndex + direction;
      const auto target =
          std::find_if(snapshot.columns.begin(), snapshot.columns.end(),
                       [targetColumn](const auto &entry) {
                         return entry.columnIndex == targetColumn;
                       });
      if (target != snapshot.columns.end())
        tiling_.groupWith(focused_->id, target->window);
    };

    if (action.startsWith("moveToWorkspace")) {
      const int target = action.mid(15).toInt() - 1;
      if (focused_ && target >= 0 &&
          target < desktopPreferences().value("workspaceCount").toInt())
        focused_->workspace = target;
      arrange();
      synchronizeTilingFocus();
    } else if (action.startsWith("workspace")) {
      const int target = action.mid(9).toInt() - 1;
      if (target >= 0 &&
          target < desktopPreferences().value("workspaceCount").toInt())
        workspace_ = target;
      arrange();
      synchronizeTilingFocus();
    } else if (action == "focusLeft" || action == "focusRight") {
      action == "focusLeft" ? tiling_.focusLeft(workspace_)
                            : tiling_.focusRight(workspace_);
      arrange();
      synchronizeTilingFocus();
    } else if (action == "focusUp" || action == "focusDown") {
      action == "focusUp" ? tiling_.focusUp(workspace_)
                          : tiling_.focusDown(workspace_);
      arrange();
      synchronizeTilingFocus();
    } else if (action == "groupLeft" || action == "groupRight") {
      groupAdjacent(action == "groupLeft" ? -1 : 1);
      arrange();
      synchronizeTilingFocus();
    } else if (action == "reorderLeft" || action == "reorderRight") {
      if (focused_ && !focused_->floating && !focused_->desktop)
        tiling_.reorder(focused_->id, action == "reorderLeft" ? -1 : 1);
      arrange();
      synchronizeTilingFocus();
    } else if (action == "widenColumn" || action == "narrowColumn") {
      if (focused_ && !focused_->floating) {
        const auto snapshot = tiling_.snapshot(workspace_);
        const auto column = std::find_if(
            snapshot.columns.begin(), snapshot.columns.end(),
            [this](const auto &entry) {
              return entry.window == static_cast<TilingWindowId>(focused_->id);
            });
        if (column != snapshot.columns.end())
          tiling_.resize(focused_->id,
                         column->width + (action == "narrowColumn" ? -60 : 60));
        arrange();
      }
    } else if (action == "centerColumn") {
      if (focused_ && !focused_->floating) {
        tiling_.center(focused_->id, workArea());
        arrange();
      }
    } else if (action == "maximizeWindow") {
      // Match niri's Mod+F maximize-column semantics. This changes the tiled
      // column width to 100% of the work area; it is not an overlay and it does
      // not implicitly maximize newly opened windows.
      auto *target = focused_;
      if (target && !target->floating && !target->desktop) {
        if (target->manualResize) {
          for (auto it = resizeOriginalWidths_.cbegin();
               it != resizeOriginalWidths_.cend(); ++it)
            tiling_.resize(it.key(), it.value());
          target->manualResize = false;
          target->manualGeometry = {};
          target->resizeGuideGeometry = {};
        }

        const auto snapshot = tiling_.snapshot(workspace_);
        const auto column = std::find_if(
            snapshot.columns.cbegin(), snapshot.columns.cend(),
            [target](const auto &entry) {
              return entry.window ==
                     static_cast<TilingWindowId>(target->id);
            });
        if (column != snapshot.columns.cend()) {
          const bool maximized = std::any_of(
              clients_.cbegin(), clients_.cend(),
              [column](const auto &client) {
                return column->columnMembers.contains(
                           static_cast<TilingWindowId>(client->id)) &&
                       client->maximized;
              });
          const int defaultWidth =
              std::clamp(qRound(workArea().width() *
                                desktopPreferences()
                                        .value("masterRatio")
                                        .toInt() /
                                    100.0),
                         1, std::max(1, workArea().width()));
          int restoreWidth = defaultWidth;
          if (maximized) {
            for (const auto &client : clients_)
              if (column->columnMembers.contains(
                      static_cast<TilingWindowId>(client->id)) &&
                  client->restoreColumnWidth > 0) {
                restoreWidth = client->restoreColumnWidth;
                break;
              }
          } else {
            restoreWidth = column->width;
          }

          tiling_.resize(target->id,
                         maximized ? restoreWidth : workArea().width());
          for (const auto &client : clients_)
            if (column->columnMembers.contains(
                    static_cast<TilingWindowId>(client->id))) {
              client->maximized = !maximized;
              client->restoreColumnWidth =
                  maximized ? 0 : restoreWidth;
            }
          arrange();
        }
      }
    } else if (action == "closeWindow" || action == "closeWindowAlternate") {
      if (focused_ && focused_->toplevel)
        focused_->toplevel->sendClose();
    } else if (action == "minimizeWindow") {
      if (focused_) {
        focused_->minimized = true;
        tiling_.setMinimized(focused_->id, true);
        arrange();
        synchronizeTilingFocus();
      }
    } else if (action == "toggleFloating") {
      if (focused_) {
        focused_->floating = !focused_->floating;
        if (focused_->floating)
          tiling_.remove(focused_->id);
        arrange();
      }
    } else if (action == "expelWindow") {
      if (focused_ && !focused_->floating && !focused_->desktop) {
        tiling_.expel(focused_->id);
        arrange();
        synchronizeTilingFocus();
      }
    } else if (action == "launchTerminal") {
      control({{"method", "launch-default"}, {"value", "terminal"}});
    } else if (action == "launchFiles") {
      control({{"method", "launch-default"}, {"value", "files"}});
    } else if (action == "launchLauncher") {
      spawn({"--app", "launcher"});
    } else if (action == "screenshot") {
      captureScreen();
    }
    consumedKeys_.insert(key->key());
    return true;
  }
  return QObject::eventFilter(watched, event);
}
} // namespace LuDash
