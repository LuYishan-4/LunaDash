#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/SurfaceText.hpp"
#include "compositor/window/WindowSwitcher.hpp"

#include "compositor/window/animation/WindowAnimation.hpp"
#include "compositor/capture/ScreenCapture.hpp"
#include "compositor/client/ClientWindow.hpp"
#include "compositor/input/Keyboard.hpp"
#include "compositor/ipc/ControlServer.hpp"
#include "compositor/plugins/ExtensionHooks.hpp"
#include "compositor/plugins/PluginManager.hpp"
#include "compositor/session/SessionActions.hpp"
#include "compositor/session/SessionEnvironment.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "compositor/window/WindowRules.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "compositor/xwayland/XWaylandSupport.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "config/localization/Localization.hpp"
#include "config/plugins/ExtensionConfiguration.hpp"
#include "core/templates/WaylandSlot.hpp"
#include "desktop/app/DefaultApplications.hpp"
#include "desktop/audio/AudioSettings.hpp"
#include "desktop/browser/Browser.hpp"
#include "desktop/display/BrightnessSettings.hpp"
#include "desktop/display/DdcBrightnessSettings.hpp"
#include "desktop/input/InputSettings.hpp"
#include "desktop/network/NetworkStatus.hpp"
#include "desktop/power/PowerSettings.hpp"
#include "desktop/shortcuts/ShortcutSettings.hpp"
#include "desktop/system/SystemStatus.hpp"
#include "desktop/system/SystemTools.hpp"
#include "desktop/system/UpdateChecker.hpp"
#include "desktop/wallpaper/WallpaperSettings.hpp"
#include "shell/modules/ShellModules.hpp"

#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <optional>

namespace LunaDash {
namespace {

using Templates::attachListener;
using Templates::detachListener;
using Templates::listenerOwner;
using Templates::WaylandSlot;

bool isUtilityWindow(const QString &appId) {
  return appId == QLatin1String("io.github.bugaevc.wl-clipboard");
}

std::optional<int> textWindowId(const QString &text) {
  bool ok = false;
  const int id = text.toInt(&ok);
  if (!ok || id <= 0)
    return std::nullopt;
  return id;
}

bool groupWindowIds(const QString &text, int *window, int *target) {
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(text.toUtf8(), &error);
  if (error.error != QJsonParseError::NoError || !document.isObject())
    return false;
  const auto object = document.object();
  const auto left = object.value("window");
  const auto right = object.value("target");
  if (!left.isDouble() || !right.isDouble())
    return false;
  const qint64 a = left.toInteger();
  const qint64 b = right.toInteger();
  if (a <= 0 || b <= 0 || a > INT_MAX || b > INT_MAX || a == b)
    return false;
  *window = static_cast<int>(a);
  *target = static_cast<int>(b);
  return true;
}

} // namespace

WaylandCompositor::WaylandCompositor(const QByteArray &socket, bool fullscreen,
                                     bool startShell,
                                     const QString &rendererPreference)
    : d(std::make_unique<Impl>(this, socket, fullscreen, rendererPreference)) {
  windowTemplate_ = &windowTemplateForKey("tiling");
  windowLayout_ = createWindowLayout(*windowTemplate_);
  windowSwitcher_ = new WindowSwitcher(this);
  screenCapture_ = new ScreenCapture(this);
  connect(screenCapture_, &ScreenCapture::completed, this,
          [this](const QString &path, const QString &error) {
            if (!path.isEmpty())
              lastCapture_ = path;
            captureError_ = error;
          });
  brightnessSettings_ = new BrightnessSettings(this);
  ddcBrightnessSettings_ = new DdcBrightnessSettings(this);
  shellModules_ = new ShellModules(this);
  systemStatus_ = new SystemStatus(this);
  audioSettings_ = new AudioSettings(this);
  powerSettings_ = new PowerSettings(this);
  sessionActions_ = new SessionActions(this);
  shortcutSettings_ = new ShortcutSettings;
  updateChecker_ = new UpdateChecker(this);
  networkStatus_ = new NetworkStatus(this);
  pluginManager_ = new PluginManager(this);
  pluginManager_->loadEnabled();
  windowLayout_->setPlacementFilter([this](auto workspace, auto area,
                                           const auto &placements) {
    return pluginWindowPlacements(*pluginManager_, *windowTemplate_, workspace,
                                  area, placements);
  });

  if (!d->initialize())
    return;

  connect(
      pluginManager_, &PluginManager::changed, this, [this] { arrange(); },
      Qt::QueuedConnection);
  windowAnimations_ = std::make_unique<SceneWindowAnimationTemplate>();
  const auto initialAppearance = desktopPreferences();
  windowAnimations_->configure(windowAnimationProfile(
      *pluginManager_, initialAppearance, *windowTemplate_));

  controlPath_ =
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + "/" +
      QString::fromUtf8(socket) + "-control";
  windowSwitcher_->setChannelPath(controlPath_ + "-interaction.json");
  controlServer_ = new ControlServer(
      controlPath_,
      [this](const QJsonObject &request) { return control(request); }, this);

  clientEnvironment_ =
      createClientEnvironment(QString::fromUtf8(socket), controlPath_,
                              QCoreApplication::applicationDirPath());
  publishClientEnvironment(clientEnvironment_);

  xwayland_ = new XWaylandSupport(this);
  if (qEnvironmentVariableIntValue("LUDASH_DISABLE_XWAYLAND") != 1) {
    auto environment = clientEnvironment_;
    environment.insert(
        "XCURSOR_SIZE",
        QString::number(desktopPreferences().value("cursorSize").toInt()));
    if (!xwayland_->start(environment, d->outputSize()))
      qWarning("XWayland could not be prepared; X11 clients are unavailable.");
    // Do not start the rootful XWayland server during desktop startup.
    // Explicit X11 launches show its root; native input helpers keep it hidden.
  }

  publishSessionActivationEnvironment();

  if (startShell &&
      qEnvironmentVariableIntValue("LUNADASH_PUBLISH_ACTIVATION_ENV") == 1) {
    const QString agent =
        QStandardPaths::findExecutable("lunadash-polkit-agent");
    if (!agent.isEmpty())
      spawn({}, agent, false);
  }

  if (startShell &&
      qEnvironmentVariableIntValue("LUNADASH_PUBLISH_ACTIVATION_ENV") == 1 &&
      qEnvironmentVariableIntValue("LUNADASH_DISABLE_FCITX") != 1) {
    const QString fcitx = QStandardPaths::findExecutable("fcitx5");
    if (!fcitx.isEmpty())
      spawn({"--replace"}, fcitx, false);
  }

  QObject::connect(shellModules_, &ShellModules::changed, this,
                   [this] { arrange(); });

  if (startShell) {
    if (auto *process = spawn({"--session"}, {}, false)) {
      connect(process, &QProcess::finished, this,
              [this](int code, QProcess::ExitStatus status) {
                if (shuttingDown_ || testStopping_)
                  return;
                if (status == QProcess::NormalExit && code == 0) {
                  requestShutdown();
                  return;
                }
                qWarning("LunaDash shell exited unexpectedly; restarting.");
                QTimer::singleShot(250, this, [this] {
                  if (!shuttingDown_ && !testStopping_)
                    spawn({"--session", "--no-welcome"}, {}, false);
                });
              });
    }
  }

  if (startShell && setupComplete()) {
    QTimer::singleShot(1200, this, [this] {
      if (testStopping_ || shuttingDown_)
        return;
      for (const auto &app :
           desktopPreferences().value("startupApps").toArray())
        spawn({"--app", app.toString()});
    });
  }

  qInfo().noquote() << "LunaDash compositor backend: wlroots"
                    << "socket:" << socket << "input: libinput/xkbcommon"
                    << "QtWayland compositor: disabled";
}

WaylandCompositor::~WaylandCompositor() {
  shuttingDown_ = true;
  delete screenCapture_;
  screenCapture_ = nullptr;
  if (windowAnimations_)
    windowAnimations_->clear();
  if (xwayland_) {
    xwayland_->stop();
    delete xwayland_;
    xwayland_ = nullptr;
  }
  for (auto *process : processes_) {
    if (!process || process->state() == QProcess::NotRunning)
      continue;
    process->terminate();
    if (!process->waitForFinished(500)) {
      process->kill();
      process->waitForFinished(500);
    }
  }
  delete shortcutSettings_;
  shortcutSettings_ = nullptr;
  d.reset();
}

QProcess *WaylandCompositor::spawn(const QStringList &arguments,
                                   const QString &program, bool required) {
  auto environment = clientEnvironment_;
  if (xwayland_)
    xwayland_->applyEnvironment(environment);
  else
    environment.remove("DISPLAY");

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
                  << "code" << code;
            }
          });

  if (program.isEmpty() && arguments.contains("--session")) {
    connect(process, &QProcess::started, this,
            [this, process] { shellProcessIds_.insert(process->processId()); });
    const QString sourceConfig =
        QStringLiteral(LUDASH_QML_SOURCE_DIR) + "/shell.qml";
    // Use assets next to the installed executable before any build checkout.
    // This also supports a staged/relocated installation in CI.
    QString config = QDir(QCoreApplication::applicationDirPath())
                         .absoluteFilePath("../share/lunadash/shell/shell.qml");
    if (!QFileInfo::exists(config))
      config = QFileInfo::exists(sourceConfig)
                   ? sourceConfig
                   : QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                            "lunadash/shell/shell.qml");
    if (config.isEmpty())
      config = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                      "ludash/shell/shell.qml");
    const QString quickshell = QStandardPaths::findExecutable("quickshell");
    if (quickshell.isEmpty()) {
      if (required)
        processFailure_ = true;
      qWarning("Quickshell is unavailable.");
      process->deleteLater();
      return nullptr;
    }
    environment.insert("LUNADASH_WALLPAPER", wallpaperImageUrl());
    // The shell draws its own theme. A blocking portal Settings query here
    // would prevent even the startup splash from reaching its first frame.
    environment.insert("QT_QPA_PLATFORMTHEME", "generic");
    environment.remove("GTK_USE_PORTAL");
    qInfo().noquote() << "LunaDash shell config:" << config;
    process->setProcessEnvironment(environment);
    process->start(quickshell, {"--path", config, "--no-color"});
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

bool WaylandCompositor::launchExternalCommand(QStringList command,
                                              QString *error, bool x11Helper) {
  if (command.isEmpty() || command.first().isEmpty()) {
    *error = "Expected an application command.";
    return false;
  }

  // Application/toolkit renderer selection belongs to the client. Do not
  // identify Chromium/Electron applications by executable names or rewrite
  // their ANGLE, Vulkan, GPU, Ozone or IME flags here. Besides being brittle,
  // scanning every command argument can misclassify URLs and document paths.
  //
  // XWayland is likewise opt-in for the launch request. Native Wayland clients
  // may still use DISPLAY from the prepared session environment when they
  // intentionally create an X11 helper, without LunaDash special-casing an
  // application such as Discord.
  if (x11Helper && (!xwayland_ || !xwayland_->startServer(error))) {
    if (error->isEmpty())
      *error =
          "This application's helper requires XWayland. Install and enable "
          "it, then restart LunaDash.";
    return false;
  }

  const QString executable = command.takeFirst();
  spawn(command, executable, false);
  return true;
}

bool WaylandCompositor::saveScreenshot(const QString &path) {
  if (!d || !d->primaryOutput || !path.startsWith('/'))
    return false;
  const QString grim = QStandardPaths::findExecutable("grim");
  if (grim.isEmpty()) {
    qWarning("Screenshot requested but grim is not installed.");
    return false;
  }

  wlr_output_schedule_frame(d->primaryOutput);
  QProcess process;
  process.setProcessEnvironment(clientEnvironment_);
  process.start(grim, {path});
  QElapsedTimer timer;
  timer.start();
  while (process.state() != QProcess::NotRunning && timer.elapsed() < 4000) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    process.waitForFinished(10);
  }
  if (process.state() != QProcess::NotRunning)
    process.kill();
  return process.exitStatus() == QProcess::NormalExit &&
         process.exitCode() == 0 && QFileInfo::exists(path);
}

void WaylandCompositor::saveState(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return;
  file.write(QJsonDocument(state()).toJson(QJsonDocument::Indented));
}

bool WaylandCompositor::hasProcessFailure() const { return processFailure_; }

QRect WaylandCompositor::workArea() const {
  QRect area = d ? d->usableArea : QRect(0, 0, 1440, 900);
  const int gap = desktopPreferences().value("gap").toInt();
  area.adjust(gap, gap, -gap, -gap);
  if (area.width() < 1)
    area.setWidth(1);
  if (area.height() < 1)
    area.setHeight(1);
  return area;
}

void WaylandCompositor::updateClientMetadata(ClientWindow *client) {
  if (!client || !client->toplevel)
    return;
  client->title = safeUtf8(client->toplevel->title);
  client->appId = safeUtf8(client->toplevel->app_id);
  client->utility =
      isUtilityWindow(client->appId) ||
      (xwayland_ && xwayland_->isHelperSurface(client->processId));
  client->iconName = client->utility
                         ? QString()
                         : windowIconName(client->appId, client->title);
  if (!client->initialRuleApplied) {
    client->floating = client->toplevel->parent != nullptr;
  } else if (client->toplevel->parent) {
    client->floating = true;
  }
}

void WaylandCompositor::configure(ClientWindow *client,
                                  const QRect &rectangle) {
  if (!client || !client->toplevel || !client->surface ||
      !client->surface->initialized || !client->sceneTree)
    return;
  if (windowAnimations_)
    windowAnimations_->relayout(
        client->sceneTree, client->geometry, rectangle, d->animationLayer,
        client->mapped && client->sceneTree->node.enabled &&
            !client->manualResize && !d->pointerWindow);
  else
    wlr_scene_node_set_position(&client->sceneTree->node, rectangle.x(),
                                rectangle.y());
  client->geometry = rectangle;
  wlr_box geometry{};
#if WLR_VERSION_MINOR >= 19
  geometry = client->surface->geometry;
#else
  wlr_xdg_surface_get_geometry(client->surface, &geometry);
#endif
  wlr_box clip{geometry.x, geometry.y, rectangle.width(), rectangle.height()};
  wlr_scene_node *child;
  wl_list_for_each(child, &client->sceneTree->children, link) {
    const bool popup = std::any_of(
        d->xdgPopups.begin(), d->xdgPopups.end(), [child](const auto *state) {
          return state->sceneTree && &state->sceneTree->node == child;
        });
    // Clip client content, never popup menus attached beside that content.
    if (!popup)
      wlr_scene_subsurface_tree_set_clip(child,
                                         client->floating ? nullptr : &clip);
  }
  const QSize size(std::max(1, rectangle.width()),
                   std::max(1, rectangle.height()));
  if (client->lastSize != size) {
    client->lastSize = size;
    wlr_xdg_toplevel_set_size(client->toplevel, size.width(), size.height());
  }
  wlr_xdg_toplevel_set_maximized(client->toplevel, client->maximized);
}

void WaylandCompositor::arrange() {
  if (!d || !d->scene)
    return;
  const auto preferences = desktopPreferences();
  const auto &selectedTemplate =
      windowTemplateForKey(pluginManager_->windowTemplateKey());
  const bool templateChanged =
      !windowTemplate_ || windowTemplate_->key != selectedTemplate.key;
  if (templateChanged) {
    if (d->pointerWindow)
      d->finishWindowPointer(false);
    windowTemplate_ = &selectedTemplate;
    windowLayout_ = createWindowLayout(*windowTemplate_);
    windowLayout_->setPlacementFilter(
        [this](auto workspace, auto area, const auto &placements) {
          return pluginWindowPlacements(*pluginManager_, *windowTemplate_,
                                        workspace, area, placements);
        });
  }
  if (windowAnimations_)
    windowAnimations_->configure(windowAnimationProfile(
        *pluginManager_, preferences, *windowTemplate_));
  const int count = preferences.value("workspaceCount").toInt();
  workspace_ = std::clamp(workspace_, 0, std::max(0, count - 1));
  const QRect area = workArea();
  const int configuredGap =
      configuredBuiltinSettings("window-layout").value("gap").toInt(-1);
  windowLayout_->setGap(configuredGap < 0 ? preferences.value("gap").toInt()
                                          : configuredGap);

  for (const auto &client : clients_) {
    client->workspace = std::min(client->workspace, count - 1);
    const bool managed = windowUsesManagedLayout(*client);
    if (managed) {
      wlr_box initialGeometry{};
#if WLR_VERSION_MINOR >= 19
      initialGeometry = client->surface->geometry;
#else
      wlr_xdg_surface_get_geometry(client->surface, &initialGeometry);
#endif
      if (!client->preferredFloatingSize.isValid() &&
          initialGeometry.width > 0 && initialGeometry.height > 0)
        client->preferredFloatingSize =
            QSize(initialGeometry.width, initialGeometry.height);
      windowLayout_->insert(client->workspace, client->id,
                            client->preferredFloatingSize);
      windowLayout_->moveToWorkspace(client->id, client->workspace);
      windowLayout_->setMinimized(client->id, client->minimized);
      if (client->maximized)
        windowLayout_->setMaximized(client->id, true);
    } else {
      windowLayout_->remove(client->id);
    }
  }

  if (templateChanged && focused_ && focused_->mapped && !focused_->minimized)
    windowLayout_->focus(focused_->id);
  QList<ClientWindow *> revealed;
  QHash<int, LayoutWindowId> maximizedWindows;
  for (const auto &client : clients_) {
    if (!maximizedWindows.contains(client->workspace))
      maximizedWindows.insert(
          client->workspace,
          windowLayout_->snapshot(client->workspace).maximizedWindow);
    const auto maximized = maximizedWindows.value(client->workspace);
    if (!client->floating)
      client->maximized = maximized == static_cast<LayoutWindowId>(client->id);
    bool inMaximizedFamily = client->id == static_cast<int>(maximized);
    auto *parent = client->toplevel ? client->toplevel->parent : nullptr;
    for (std::size_t depth = 0; parent && depth < clients_.size(); ++depth) {
      const auto owner = std::find_if(
          clients_.begin(), clients_.end(),
          [parent](const auto &entry) { return entry->toplevel == parent; });
      if (owner != clients_.end() &&
          (*owner)->id == static_cast<int>(maximized))
        inMaximizedFamily = true;
      parent = parent->parent;
    }
    client->hiddenByMaximize =
        windowHiddenByMaximize(*windowTemplate_, maximized, inMaximizedFamily,
                               *client);
    const bool visible = client->mapped && !client->minimized &&
                         client->workspace == workspace_ && !client->utility &&
                         !client->hiddenByMaximize;
    if (client->sceneTree) {
      const bool wasVisible = client->sceneTree->node.enabled;
      if (!visible && wasVisible && client->mapped && windowAnimations_)
        windowAnimations_->close(client->sceneTree, d->animationLayer);
      if (visible && !wasVisible && client->geometry.isValid())
        revealed.append(client.get());
      if (!visible && windowAnimations_)
        windowAnimations_->cancel(client->sceneTree);
      wlr_scene_node_set_enabled(&client->sceneTree->node, visible);
    }

    if (!visible)
      continue;
    if (client->floating) {
      const QSize preferred = client->preferredFloatingSize.isValid()
                                  ? client->preferredFloatingSize
                                  : QSize(720, 500);
      const QSize bounded(std::min(preferred.width(), area.width()),
                          std::min(preferred.height(), area.height()));
      const QRect defaultGeometry(
          area.x() + (area.width() - bounded.width()) / 2,
          area.y() + (area.height() - bounded.height()) / 2, bounded.width(),
          bounded.height());
      configure(client.get(), client->maximized ? area
                              : client->manualGeometry.isValid()
                                  ? client->manualGeometry
                                  : defaultGeometry);
    }
  }

  const auto placements = windowLayout_->presentation(workspace_, area);
  if (windowTemplate_->key != pluginManager_->windowTemplateKey()) {
    arrange();
    return;
  }
  for (const auto &placement : placements) {
    auto found = std::find_if(
        clients_.begin(), clients_.end(), [&placement](const auto &client) {
          return client->id == static_cast<int>(placement.window);
        });
    if (found == clients_.end() || (*found)->minimized ||
        placement.hiddenByMaximize || (*found)->workspace != workspace_)
      continue;
    configure(found->get(), placement.geometry);
  }

  if (windowAnimations_)
    for (auto *client : revealed)
      windowAnimations_->open(client->sceneTree, client->geometry);

  if (focused_ && focused_->sceneTree && focused_->mapped &&
      !focused_->minimized && focused_->workspace == workspace_)
    raiseWithDialogs(focused_);

  d->updateBackground();
  publishWindowLayout();
}

void WaylandCompositor::raiseWithDialogs(ClientWindow *client) {
  if (!client || !client->sceneTree)
    return;
  wlr_scene_node_raise_to_top(&client->sceneTree->node);
  for (const auto &dialog : clients_) {
    if (!dialog->floating || !dialog->sceneTree ||
        !dialog->sceneTree->node.enabled || !dialog->toplevel)
      continue;
    auto *parent = dialog->toplevel->parent;
    for (std::size_t depth = 0; parent && depth < clients_.size(); ++depth) {
      if (parent == client->toplevel) {
        wlr_scene_node_raise_to_top(&dialog->sceneTree->node);
        break;
      }
      parent = parent->parent;
    }
  }
}

void WaylandCompositor::focus(ClientWindow *client) {
  if (!d || !client || !client->mapped || client->minimized ||
      client->utility || client->workspace != workspace_ || !client->surface ||
      !client->surface->initialized)
    return;

  if (focused_ && focused_ != client && focused_->toplevel &&
      focused_->surface && focused_->surface->initialized)
    wlr_xdg_toplevel_set_activated(focused_->toplevel, false);

  const bool changed = focused_ != client;
  focused_ = client;

  if (client->floating && client->hiddenByMaximize) {
    for (const auto &other : clients_)
      if (!other->floating && other->workspace == client->workspace &&
          other->maximized)
        setMaximized(other.get(), false);
    arrange();
  }
  if (windowLayout_->mode() == WindowLayoutMode::Tiling && !client->floating &&
      !client->maximized &&
      windowLayout_->snapshot(client->workspace).maximizedWindow)
    setMaximized(client, true);
  if (!client->floating)
    windowLayout_->focus(client->id);
  // Selecting a tiled window must also scroll its column into view. Avoid
  // relayout for repeated clicks in the same window (including popup grabs).
  if (changed && !client->floating)
    arrange();
  if (client->sceneTree)
    raiseWithDialogs(client);
  if (changed && windowAnimations_ && client->sceneTree)
    windowAnimations_->focus(client->sceneTree, client->geometry);
  if (changed)
    wlr_xdg_toplevel_set_activated(client->toplevel, true);
  d->focusSurface(client->surface->surface);
  publishWindowLayout();
}

void WaylandCompositor::focusNext(int direction) {
  QList<ClientWindow *> visible;
  for (const auto &client : clients_)
    if (client->mapped && !client->minimized && !client->utility &&
        client->workspace == workspace_)
      visible << client.get();
  if (visible.isEmpty()) {
    focused_ = nullptr;
    if (d && d->seat)
      wlr_seat_keyboard_notify_clear_focus(d->seat);
    return;
  }
  const qsizetype index = visible.indexOf(focused_);
  focus(visible[(index + direction + visible.size()) % visible.size()]);
}

void WaylandCompositor::setMaximized(ClientWindow *client, bool maximized) {
  if (!client || client->maximized == maximized)
    return;
  if (!client->floating) {
    if (maximized) {
      for (const auto &other : clients_) {
        if (other.get() == client || other->workspace != client->workspace ||
            other->floating || !other->maximized)
          continue;
        other->maximized = false;
        if (other->toplevel && other->surface && other->surface->initialized)
          wlr_xdg_toplevel_set_maximized(other->toplevel, false);
      }
    }
    windowLayout_->setMaximized(client->id, maximized);
  }
  client->maximized = maximized;
  if (client->toplevel)
    wlr_xdg_toplevel_set_maximized(client->toplevel, maximized);
}

void WaylandCompositor::synchronizeWindowFocus() {
  const auto target = windowLayout_->snapshot(workspace_).focusedWindow;
  if (target) {
    auto found = std::find_if(clients_.begin(), clients_.end(),
                              [target](const auto &client) {
                                return client->id == static_cast<int>(target) &&
                                       client->mapped && !client->minimized;
                              });
    if (found != clients_.end()) {
      focus(found->get());
      return;
    }
  }
  focusNext(1);
}

void WaylandCompositor::removeClient(ClientWindow *client) {
  if (!client)
    return;
  if (focused_ == client)
    focused_ = nullptr;
  windowSwitcher_->remove(client->id);
  windowLayout_->remove(client->id);
  std::erase_if(clients_,
                [client](const auto &entry) { return entry.get() == client; });
  arrange();
  synchronizeWindowFocus();
  if (logoutPending_ && clients_.empty())
    QCoreApplication::exit(hasProcessFailure() ? 2 : 0);
}

void WaylandCompositor::applyKeyboardConfiguration() {
  if (d)
    d->applyKeyboardConfig();
}

void WaylandCompositor::resendKeyboardModifiers() {
  if (!d || !d->seat)
    return;
  if (auto *keyboard = wlr_seat_get_keyboard(d->seat)) {
    wlr_seat_keyboard_notify_modifiers(d->seat, &keyboard->modifiers);
    ++modifierResends_;
  }
}

void WaylandCompositor::handleShortcut(const QString &action) {
  if (action.startsWith("workspace")) {
    bool ok = false;
    const int target =
        action.mid(QStringLiteral("workspace").size()).toInt(&ok);
    if (ok && target > 0 &&
        target <= desktopPreferences().value("workspaceCount").toInt()) {
      selectWorkspace(target - 1);
    }
    return;
  }

  if (action.startsWith("moveToWorkspace")) {
    bool ok = false;
    const int target =
        action.mid(QStringLiteral("moveToWorkspace").size()).toInt(&ok);
    if (ok && focused_ && target > 0 &&
        target <= desktopPreferences().value("workspaceCount").toInt()) {
      focused_->workspace = target - 1;
      windowLayout_->moveToWorkspace(focused_->id, focused_->workspace);
      arrange();
      synchronizeWindowFocus();
    }
    return;
  }

  if (focused_ && focused_->maximized &&
      (action == "reorderLeft" || action == "reorderRight" ||
       action == "groupLeft" || action == "groupRight" ||
       action == "expelWindow" || action == "centerColumn" ||
       action == "widenColumn" || action == "narrowColumn")) {
    setMaximized(focused_, false);
    arrange();
  }

  if (action == "focusLeft")
    windowLayout_->focusLeft(workspace_);
  else if (action == "focusRight")
    windowLayout_->focusRight(workspace_);
  else if (action == "focusUp")
    windowLayout_->focusUp(workspace_);
  else if (action == "focusDown")
    windowLayout_->focusDown(workspace_);
  else if (action == "reorderLeft" && focused_)
    windowLayout_->reorder(focused_->id, -1);
  else if (action == "reorderRight" && focused_)
    windowLayout_->reorder(focused_->id, 1);
  else if ((action == "groupLeft" || action == "groupRight") && focused_) {
    windowLayout_->focus(focused_->id);
    const bool found = action == "groupLeft"
                           ? windowLayout_->focusLeft(workspace_)
                           : windowLayout_->focusRight(workspace_);
    if (found) {
      const auto target = windowLayout_->snapshot(workspace_).focusedWindow;
      windowLayout_->groupWith(focused_->id, target);
      windowLayout_->focus(focused_->id);
    }
  } else if (action == "expelWindow" && focused_)
    windowLayout_->expel(focused_->id);
  else if (action == "centerColumn" && focused_)
    windowLayout_->center(focused_->id, workArea());
  else if (action == "widenColumn" && focused_)
    windowLayout_->resize(
        focused_->id, std::min(workArea().width(),
                               std::max(120, focused_->geometry.width() + 80)));
  else if (action == "narrowColumn" && focused_)
    windowLayout_->resize(focused_->id,
                          std::max(120, focused_->geometry.width() - 80));
  else if (action == "maximizeWindow" && focused_) {
    setMaximized(focused_, !focused_->maximized);
  } else if ((action == "closeWindow" || action == "closeWindowAlternate") &&
             focused_ && focused_->toplevel)
    wlr_xdg_toplevel_send_close(focused_->toplevel);
  else if (action == "minimizeWindow" && focused_) {
    focused_->minimized = true;
    windowLayout_->setMinimized(focused_->id, true);
  } else if (action == "launchTerminal" || action == "launchTerminalAlternate")
    control({{"method", "launch-default"}, {"value", "terminal"}});
  else if (action == "launchFiles")
    control({{"method", "launch-default"}, {"value", "files"}});
  else if (action == "launchLauncher")
    spawn({"--app", "launcher"});
  else if (action == "screenshot") {
    captureScreen();
    // A repeated shortcut must leave keyboard focus on the active selector so
    // Escape can still cancel it.
    return;
  }

  arrange();
  synchronizeWindowFocus();
}

QJsonObject WaylandCompositor::state() const {
  QJsonArray entries;
  for (const auto &client : clients_) {
    if (client->utility)
      continue;
    int bufferWidth = 0;
    int bufferHeight = 0;
    if (client->surface && client->surface->surface) {
      bufferWidth = client->surface->surface->current.width;
      bufferHeight = client->surface->surface->current.height;
    }
    entries.append(QJsonObject{
        {"contentWidth", bufferWidth},
        {"contentHeight", bufferHeight},
        {"contentVisible",
         client->sceneTree ? client->sceneTree->node.enabled : false},
        {"contentPaintEnabled", true},
        {"bufferWidth", bufferWidth},
        {"bufferHeight", bufferHeight},
        {"id", client->id},
        {"title", client->title},
        {"appId", client->appId},
        {"icon", client->iconName},
        {"desktop", client->desktop},
        {"workspace", client->workspace},
        {"visible",
         client->sceneTree ? client->sceneTree->node.enabled : false},
        {"focused", client.get() == focused_},
        {"x", client->geometry.x()},
        {"y", client->geometry.y()},
        {"width", client->geometry.width()},
        {"height", client->geometry.height()},
        {"minimized", client->minimized},
        {"maximized", client->maximized},
        {"hiddenByMaximize", client->hiddenByMaximize},
        {"manualResize", client->manualResize},
        {"mapped", client->mapped}});
  }

  const auto tilingSnapshot = windowLayout_->snapshot(workspace_);
  QJsonArray tilingColumns;
  QJsonArray tilingGroups;
  QSet<int> emittedGroups;
  for (const auto &column : tilingSnapshot.columns) {
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
    if (emittedGroups.contains(column.columnIndex))
      continue;
    emittedGroups.insert(column.columnIndex);
    QJsonArray members;
    bool groupFocused = false;
    for (auto id : column.columnMembers) {
      const auto found = std::find_if(
          clients_.cbegin(), clients_.cend(), [id](const auto &client) {
            return client->id == static_cast<int>(id);
          });
      if (found == clients_.cend())
        continue;
      const auto *member = found->get();
      const bool memberFocused = member == focused_ && !member->minimized;
      groupFocused = groupFocused || memberFocused;
      members.append(QJsonObject{{"window", static_cast<qint64>(id)},
                                 {"title", member->title},
                                 {"appId", member->appId},
                                 {"icon", member->iconName},
                                 {"focused", memberFocused},
                                 {"minimized", member->minimized}});
    }
    tilingGroups.append(QJsonObject{{"index", column.columnIndex},
                                    {"focused", groupFocused},
                                    {"width", column.width},
                                    {"members", members}});
  }

  auto preferences = desktopPreferences();
  const auto extensions = pluginManager_->snapshot();
  preferences["plugins"] = extensions.value("installed");
  return {
      {"layoutMode", windowLayout_->mode() == WindowLayoutMode::Stacking
                         ? "stacking"
                         : "tiling"},
      {"defaultApps", defaultApplications()},
      {"shellModules", shellModules_->snapshot()},
      {"extensions", extensions},
      {"shellAnimationDuration",
       shellAnimationDuration(*pluginManager_,
                              preferences.value("animationDuration").toInt())},
      {"panelExtent",
       shellModules_->panelExtent(preferences.value("panelHeight").toInt())},
      {"panelAtBottom", shellModules_->panelAtBottom()},
      {"audio", audioSettings_->snapshot()},
      {"power", powerSettings_->snapshot()},
      {"sessionActions", sessionActions_->snapshot()},
      {"shortcuts", shortcutSettings_->snapshot()},
      {"update", updateChecker_->snapshot()},
      {"settingsTools", systemSettingsTools()},
      {"display", d ? d->displaySnapshot() : QJsonObject{}},
      {"settingsSerial", settingsSerial_},
      {"settingsPage", settingsPage_},
      {"pickerSerial", pickerSerial_},
      {"input",
       QJsonObject{{"layout", keyboardLayoutPreference(preferences)},
                   {"repeatRate", keyboardRepeatRate()},
                   {"repeatDelay", keyboardRepeatDelay()},
                   {"modifierResends", modifierResends_},
                   {"keypadKeyForwards", keypadKeyForwards_},
                   {"seatProtocolVersion",
                    WlrootsCompat::expectedSeatProtocolVersion()},
                   {"dataDeviceProtocolVersion", 3},
                   {"backend", "wlroots"},
                   {"qtInput", false},
                   {"inputMethodBridge", d ? d->inputBridgeReady() : false}}},
      {"blurReady", false},
      {"blurFailed", false},
      {"blurFrames", 0},
      {"activeAnimations",
       windowAnimations_ ? windowAnimations_->activeCount() : 0},
      {"xwayland", xwayland_ ? xwayland_->snapshot() : QJsonObject{}},
      {"screenCapture", QJsonObject{{"protocol", "zwlr_screencopy_manager_v1"},
                                    {"version", 3},
                                    {"frames", 0},
                                    {"lastCapture", lastCapture_},
                                    {"error", captureError_},
                                    {"busy", screenCapture_->busy()},
                                    {"phase", screenCapture_->phase()}}},
      {"activationEnvironment",
       QJsonObject{{"published", activationEnvironmentPublished_},
                   {"error", activationEnvironmentError_}}},
      {"brightness", brightnessSettings_->snapshot()},
      {"ddcBrightness", ddcBrightnessSettings_->snapshot()},
      {"appearance", preferences},
      {"setupComplete", setupComplete()},
      {"network", networkStatus_->snapshot()},
      {"system", systemStatus_->snapshot()},
      {"wallpaperImage", wallpaperImageUrl()},
      {"workspace", workspace_},
      {"processFailure", processFailure_},
      {"clients", entries},
      {"pointerDrag", QJsonObject{{"window", d ? d->pointerWindow : 0},
                                  {"target", d ? d->pointerTarget : 0},
                                  {"edge", d ? d->pointerEdge : 0}}},
      {"tiling",
       QJsonObject{
           {"focusedWindow", static_cast<qint64>(tilingSnapshot.focusedWindow)},
           {"columns", tilingColumns},
           {"groups", tilingGroups}}},
      {"layerSurfaces", d ? d->mappedLayerCount() : 0},
      {"xdgPopupCount", d ? d->xdgPopups.size() : 0},
      {"language", selectedLanguage()},
      {"translations", languageDictionary(selectedLanguage())},
      {"wallpaper", QSettings().value("appearance/wallpaper", 0).toInt()},
      {"shutdown", testStopping_},
      {"graphicsApi", "wlroots"},
      {"shaderReady", d && d->renderer},
      {"graphicsFailed", processFailure_},
      {"graphicsMajor", 0},
      {"graphicsMinor", 0}};
}

QJsonObject WaylandCompositor::control(const QJsonObject &request) {
  const QString method = request.value("method").toString();
  const QString value = request.value("value").toString();
  if (method == "status")
    return state();

  bool numberValid = false;
  const int number = value.toInt(&numberValid);

  if (method == "workspace" && numberValid && number >= 0 &&
      number < desktopPreferences().value("workspaceCount").toInt()) {
    selectWorkspace(number);
  } else if (method == "language" && isSupportedLanguage(value)) {
    QSettings().setValue("appearance/language", value);
  } else if (method == "shortcut-capture" &&
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
    if (!value.startsWith('/'))
      return {{"error", "Capture requires an absolute path."}};
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
    return {{"pending", screenCapture_->busy()},
            {"phase", screenCapture_->phase()}};
  } else if (method == "check-update") {
    updateChecker_->check();
  } else if (method == "send-key") {
    if (!d || !d->seat || !d->seat->keyboard_state.focused_surface)
      return {{"error", "No application has keyboard focus."}};
    auto *keyboard = wlr_seat_get_keyboard(d->seat);
    if (!keyboard || !keyboard->keymap)
      return {{"error", "No wlroots keyboard is available."}};
    const char *name = value == "copy"        ? "AC03"
                       : value == "paste"     ? "AB04"
                       : value == "cut"       ? "AB02"
                       : value == "selectAll" ? "AC01"
                                              : nullptr;
    if (!name)
      return {{"error", "Unknown key action."}};
    const xkb_keycode_t ctrl = xkb_keymap_key_by_name(keyboard->keymap, "LCTL");
    const xkb_keycode_t key = xkb_keymap_key_by_name(keyboard->keymap, name);
    if (!ctrl || !key)
      return {{"error", "Keymap does not contain the requested keys."}};
    const uint32_t now =
        static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch() & 0xffffffff);
    wlr_seat_keyboard_notify_key(d->seat, now, ctrl - 8,
                                 WL_KEYBOARD_KEY_STATE_PRESSED);
    wlr_seat_keyboard_notify_key(d->seat, now, key - 8,
                                 WL_KEYBOARD_KEY_STATE_PRESSED);
    wlr_seat_keyboard_notify_key(d->seat, now, key - 8,
                                 WL_KEYBOARD_KEY_STATE_RELEASED);
    wlr_seat_keyboard_notify_key(d->seat, now, ctrl - 8,
                                 WL_KEYBOARD_KEY_STATE_RELEASED);
  } else if (method == "open-settings") {
    static const QStringList pages{
        "general",      "appearance", "windows",   "shortcuts",   "display",
        "input",        "sound",      "network",   "bluetooth",   "power",
        "applications", "privacy",    "system",    "devices",     "about",
        "modules",      "plugins",    "dashboard", "input-method"};
    if (!value.isEmpty() && !pages.contains(value))
      return {{"error", "Unknown settings page."}};
    settingsPage_ = value.isEmpty() ? "general" : value;
    settingsSerial_ = (settingsSerial_ + 1) % 1000000;
  } else if (method == "extension-save") {
    QString error;
    if (!saveExtensionConfiguration(value.toUtf8(), &error))
      return {{"error", error}};
    pluginManager_->refresh();
    arrange();
  } else if (method == "extension-error") {
    const auto object = QJsonDocument::fromJson(value.toUtf8()).object();
    pluginManager_->reportError(object.value("id").toString(),
                                object.value("error").toString());
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
    const auto object = QJsonDocument::fromJson(value.toUtf8()).object();
    shellModules_->reportError(object.value("id").toString(),
                               object.value("error").toString());
  } else if (method == "default-apps") {
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    QString error;
    if (!document.isObject() ||
        !setDefaultApplications(document.object(), &error))
      return {{"error", error.isEmpty() ? "Expected a JSON object." : error}};
  } else if (method == "open-url") {
    QString error;
    auto command = Browser::commandForUrl(QUrl(value), &error);
    if (!error.isEmpty())
      return {{"error", error}};
    if (!launchExternalCommand(command, &error))
      return {{"error", error}};
  } else if (method == "launch-default" &&
             (value == "terminal" || value == "files" || value == "browser")) {
    QString error;
    auto command = defaultApplicationCommand(value, &error);
    if (!error.isEmpty())
      return {{"error", error}};
    if (command.isEmpty()) {
      if (value == "browser")
        return {{"error", "No browser is available."}};
      spawn({"--app", value, "--builtin"});
    } else {
      if (!launchExternalCommand(command, &error))
        return {{"error", error}};
    }
  } else if (method == "system-tool") {
    auto command = systemSettingsCommand(value);
    if (command.isEmpty())
      return {{"error", "This settings tool is unavailable."}};
    const QString program = command.takeFirst();
    spawn(command, program, false);
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
  } else if (method == "brightness") {
    bool ok = false;
    const int percent = value.toInt(&ok);
    QString error;
    if (!ok || !brightnessSettings_->setPercent(percent, &error))
      return {{"error",
               error.isEmpty() ? "Brightness must be an integer." : error}};
  } else if (method == "ddc-brightness") {
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    const auto request = document.object();
    const auto percent = request.value("percent");
    QString error;
    if (!document.isObject() || !percent.isDouble() ||
        percent.toDouble() != percent.toInt(-1) ||
        !ddcBrightnessSettings_->setPercent(request.value("id").toString(),
                                            percent.toInt(-1), &error))
      return {{"error",
               error.isEmpty() ? "Invalid DDC/CI brightness request." : error}};
  } else if (method == "ddc-refresh") {
    ddcBrightnessSettings_->refresh();
  } else if (method == "display-configure") {
    const auto changes = QJsonDocument::fromJson(value.toUtf8());
    QString error;
    if (!changes.isObject() || !d->configureDisplay(changes.object(), &error))
      return {{"error",
               error.isEmpty() ? "Invalid display configuration." : error}};
  } else if (method == "display-confirm") {
    d->confirmDisplay();
  } else if (method == "display-revert") {
    d->revertDisplay();
  } else if (method == "desktop-size") {
    QString error;
    if (!d || !d->resizePrimaryOutput(value, &error))
      return {{"error", error}};
  } else if (method == "reset-preferences") {
    QSettings settings;
    settings.remove("desktop");
    settings.sync();
    applyKeyboardConfiguration();
    arrange();
  } else if (method == "appearance") {
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(value.toUtf8(), &parseError);
    QString error;
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
      return {{"error", "Expected a JSON object of desktop preferences."}};
    if (!updateDesktopPreferences(document.object(), &error))
      return {{"error", error}};
    applyKeyboardConfiguration();
    if (d)
      d->updateBackground();
    arrange();
  } else if (method == "launch-x11") {
    QString error;
    if (!xwayland_ || !xwayland_->launch(QProcess::splitCommand(value), &error))
      return {{"error", error.isEmpty() ? "XWayland is unavailable." : error}};
    for (const auto &client : clients_)
      updateClientMetadata(client.get());
    arrange();
  } else if (method == "launch-command" || method == "launch-with-x11") {
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    if (!document.isArray() || document.array().isEmpty())
      return {{"error", "Expected a non-empty command array."}};
    QStringList command;
    for (const auto &entry : document.array()) {
      if (!entry.isString() || entry.toString().size() > 1024)
        return {{"error", "Command arguments must be short strings."}};
      command << entry.toString();
    }
    QString error;
    if (!launchExternalCommand(command, &error, method == "launch-with-x11"))
      return {{"error", error}};
  } else if (method == "finish-setup") {
    setSetupComplete(true);
  } else if (method == "setup") {
    setSetupComplete(false);
  } else if (method == "configure-network") {
    auto program = QStandardPaths::findExecutable("nm-connection-editor");
    QStringList arguments;
    if (program.isEmpty() &&
        !QStandardPaths::findExecutable("nmtui").isEmpty()) {
      for (const auto &terminal : {"kitty", "konsole", "alacritty", "foot"}) {
        program = QStandardPaths::findExecutable(terminal);
        if (!program.isEmpty()) {
          arguments = {"-e", "nmtui"};
          break;
        }
      }
    }
    if (program.isEmpty())
      return {{"error", "No network configuration utility is installed."}};
    spawn(arguments, program, false);
  } else if (method == "choose-wallpaper") {
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
    resetWallpaperImage();
  } else if (method == "wallpaper" && numberValid && number >= 0 &&
             number <= 1) {
    QSettings().setValue("appearance/wallpaper", number);
    QSettings().setValue("appearance/wallpaperMode", "shader");
    if (d)
      d->updateBackground();
  } else if (method == "group-window") {
    int window = 0;
    int target = 0;
    if (!groupWindowIds(value, &window, &target))
      return {{"error", "Expected bounded JSON window and target IDs."}};
    if (!windowLayout_->groupWith(window, target))
      return {{"error", "Windows cannot be grouped."}};
    arrange();
  } else if (method == "expel-window") {
    const auto window = textWindowId(value);
    if (!window || !windowLayout_->expel(*window))
      return {{"error", "Window must be a member of a tiled group."}};
    arrange();
    synchronizeWindowFocus();
  } else if (method == "quit") {
    QTimer::singleShot(0, this, &WaylandCompositor::requestShutdown);
  } else if (method == "switch-window") {
    const auto id = textWindowId(value);
    if (!id || !windowSwitcher_->select(*id))
      return {{"error", "Unknown selection"}};
  } else if (method == "switch-step") {
    windowSwitcher_->step(value == "previous" ? -1 : 1);
  } else if (method == "switch-accept" || method == "switch-cancel") {
    finishWindowSwitch(method == "switch-accept");
  } else if (method == "activate-window") {
    const auto id = textWindowId(value);
    if (!id)
      return {{"error", "Expected a window ID"}};
    activateTask(*id);
  } else if (method == "focus" || method == "close" || method == "minimize") {
    const auto id = textWindowId(value);
    if (!id)
      return {{"error", "Window command requires a positive integer ID."}};
    for (const auto &client : clients_) {
      if (client->id != *id)
        continue;
      if (method == "close") {
        if (client->toplevel)
          wlr_xdg_toplevel_send_close(client->toplevel);
      } else if (method == "minimize") {
        client->minimized = true;
        windowLayout_->setMinimized(client->id, true);
        arrange();
        synchronizeWindowFocus();
      } else {
        workspace_ = client->workspace;
        client->minimized = false;
        windowLayout_->setMinimized(client->id, false);
        windowLayout_->focus(client->id);
        arrange();
        focus(client.get());
      }
      return state();
    }
    return {{"error", "Unknown window"}};
  } else {
    return {{"error", "Invalid command or value"}};
  }

  return state();
}

void WaylandCompositor::publishSessionActivationEnvironment() {
  if (qEnvironmentVariableIntValue("LUNADASH_PUBLISH_ACTIVATION_ENV") != 1)
    return;
  auto environment = clientEnvironment_;
  if (xwayland_)
    xwayland_->applyEnvironment(environment);
  publishActivationEnvironment(environment, this,
                               [this](bool ok, const QString &error) {
                                 activationEnvironmentPublished_ = ok;
                                 activationEnvironmentError_ = error;
                               });
}

QString WaylandCompositor::nextCapturePath() const {
  auto directory =
      QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
  if (directory.isEmpty())
    directory = QDir::homePath() + "/Pictures";
  directory += "/Screenshots";
  if (!QDir().mkpath(directory))
    return {};
  return directory + "/LunaDash-" +
         QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss-zzz") + ".png";
}

void WaylandCompositor::captureScreen() {
  if (screenCapture_->busy())
    return;
  captureError_.clear();
  const QString path = nextCapturePath();
  if (path.isEmpty()) {
    captureError_ = "Could not create the screenshot directory.";
    return;
  }
  screenCapture_->selectRegion(clientEnvironment_, path, &captureError_);
}

void WaylandCompositor::closeTestSession(
    const std::function<void(bool)> &finished) {
  testStopping_ = true;
  for (const auto &client : clients_)
    if (client->toplevel)
      wlr_xdg_toplevel_send_close(client->toplevel);

  auto *timer = new QTimer(this);
  auto elapsed = std::make_shared<int>(0);
  connect(timer, &QTimer::timeout, this, [this, timer, elapsed, finished] {
    *elapsed += 50;
    if (clients_.empty() && xwayland_)
      xwayland_->stop();
    const bool xwaylandDone = !xwayland_ || xwayland_->stopped();
    const bool processesDone = std::all_of(
        processes_.cbegin(), processes_.cend(), [](const QProcess *process) {
          return !process || process->state() == QProcess::NotRunning;
        });
    if ((clients_.empty() && processesDone && xwaylandDone) ||
        *elapsed >= 5000) {
      timer->stop();
      timer->deleteLater();
      finished(clients_.empty() && processesDone && xwaylandDone &&
               !processFailure_);
    }
  });
  timer->start(50);
}

void WaylandCompositor::requestShutdown() {
  logoutPending_ = true;
  bool applications = false;
  for (const auto &client : clients_) {
    if (!client->toplevel)
      continue;
    applications = true;
    wlr_xdg_toplevel_send_close(client->toplevel);
  }
  if (!applications)
    QCoreApplication::exit(hasProcessFailure() ? 2 : 0);
}

} // namespace LunaDash
