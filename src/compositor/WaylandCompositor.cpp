#include <LuDash/renderer/WallpaperItem.h>
#include <LuDash/blur/BlurItem.h>
#include <LuDash/animation/WindowAnimations.h>
#include <LuDash/xwayland/XWaylandSupport.h>
#include <QtWaylandCompositor/QWaylandViewporter>
#include <LuDash/system_status/SystemStatus.h>
#include <LuDash/configuration/DesktopPreferences.h>
#include <LuDash/network/NetworkStatus.h>
#include <LuDash/wallpaper/WallpaperSettings.h>
#include <QUrl>
#include <LuDash/input_method/InputMethodSupport.h>
#include <LuDash/plugins/PluginManager.h>
#include <LuDash/compositor/WaylandCompositor.h>
#include <LuDash/window_frame/WindowFrame.h>
#include <LuDash/layer_shell/LayerShell.h>
#include <LuDash/ipc/ControlServer.h>
#include <LuDash/localization/Localization.h>
#include <QStandardPaths>
#include <QSettings>
#include <QDir>
#include <LuDash/compositor/ClientWindow.h>
#include <LuDash/tiling/TilingLayout.h>
#include <QGuiApplication>
#include <QCommandLineParser>
#include <QQuickWindow>
#include <QQuickPaintedItem>
#include <QSGRendererInterface>
#include <QPainter>
#include <QKeyEvent>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QPointer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QtWaylandCompositor/QWaylandQuickCompositor>
#include <QtWaylandCompositor/QWaylandQuickOutput>
#include <QtWaylandCompositor/QWaylandQuickShellSurfaceItem>
#include <QtWaylandCompositor/QWaylandXdgShell>
#include <QtWaylandCompositor/QWaylandXdgDecorationManagerV1>
#include <QtWaylandCompositor/QWaylandSeat>
#include <QtWaylandCompositor/QWaylandClient>
#include <memory>

namespace LuDash {
WaylandCompositor::WaylandCompositor(const QByteArray& socket, bool fullscreen, bool startShell, GraphicsApi graphics) {
    window_.setTitle("LuDash Wayland · workspace 1"); window_.resize(1440, 900); window_.setMinimumSize({960, 640});
    renderState_ = std::make_shared<RenderState>();
    connect(&window_, &QQuickWindow::sceneGraphError, this,
            [this](QQuickWindow::SceneGraphError, const QString& message) {
        renderState_->failed = true;
        qCritical().noquote() << "LuDash graphics initialization failed:" << message;
        qCritical("Requires OpenGL 3.3 Core or OpenGL ES 3.0. Check the driver and MESA_GL_VERSION_OVERRIDE.");
        QTimer::singleShot(0, this, [] { QCoreApplication::exit(2); });
    });
    wallpaper_ = new WallpaperItem(graphics, renderState_, window_.contentItem());
    wallpaper_->setSize(window_.size()); wallpaper_->setZ(-200);
    wallpaper_->setPalette(QSettings().value("appearance/wallpaper", 0).toInt());
    window_.setColor(QColor("#171c36")); window_.installEventFilter(this);
    compositor_.setSocketName(socket);
    shell_ = new QWaylandXdgShell(&compositor_);
    new QWaylandViewporter(&compositor_);
    animations_ = new WindowAnimations(this);
    blurHealth_ = std::make_shared<BlurHealth>();
    auto* decorations = new QWaylandXdgDecorationManagerV1;
    decorations->setParent(&compositor_);
    decorations->setExtensionContainer(&compositor_);
    decorations->initialize();
    decorations->setPreferredMode(QWaylandXdgToplevel::ServerSideDecoration);
    output_ = new QWaylandQuickOutput(&compositor_, &window_);
    output_->setSizeFollowsWindow(true);
    output_->setManufacturer("LuDash"); output_->setModel("LuDash desktop");
    connect(shell_, &QWaylandXdgShell::toplevelCreated, this, &WaylandCompositor::addWindow);
    installInputMethodProtocols(&compositor_);
    compositor_.create();
    layerShell_ = new LayerShell(&compositor_, output_, &window_);
    controlPath_ = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + "/" + QString::fromUtf8(socket) + "-control";
    controlServer_ = new ControlServer(controlPath_, [this](const QJsonObject& request) { return control(request); }, this);
    systemStatus_ = new SystemStatus(this);
    networkStatus_ = new NetworkStatus(this);
    pluginManager_ = new PluginManager(this);
    pluginManager_->loadEnabled();
    for (const auto& error : pluginManager_->errors()) qWarning().noquote() << error;
    connect(&window_, &QQuickWindow::widthChanged, this, [this] { arrange(); });
    connect(&window_, &QQuickWindow::heightChanged, this, [this] { arrange(); });
    connect(compositor_.defaultSeat(), &QWaylandSeat::keyboardFocusChanged, this, [this](QWaylandSurface* surface, QWaylandSurface*) {
        for (const auto& client : clients_) {
            client->frame->focused = client->item && client->item->surface() == surface;
            if (client->frame->focused && !client->desktop) focused_ = client.get();
            client->frame->update();
        }

    });
    if (fullscreen) window_.showFullScreen(); else window_.show();
    xwayland_ = new XWaylandSupport(this);
    if (qEnvironmentVariableIntValue("LUDASH_DISABLE_XWAYLAND") != 1) {
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("WAYLAND_DISPLAY", QString::fromUtf8(socket));
        environment.insert("LUDASH_CONTROL", controlPath_);
        environment.insert("LUDASH_BIN_DIR", QCoreApplication::applicationDirPath());
        xwayland_->start(environment);
    }
    if (startShell) {
        auto* process = spawn({"--session"});
        connect(process, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
            if (shuttingDown_ || testStopping_) return;
            if (processFailure_) { QCoreApplication::exit(2); return; }
            // Restore the panel while applications finish their save/discard dialogs.
            requestShutdown();
            if (!clients_.empty()) spawn({"--session", "--no-welcome"});
        });
    }
    qInfo().noquote() << "LuDash Wayland socket:" << socket;
}

WaylandCompositor::~WaylandCompositor() {
    shuttingDown_ = true;
    delete xwayland_; xwayland_ = nullptr;
    disconnect(compositor_.defaultSeat(), nullptr, this, nullptr);
    delete layerShell_; layerShell_ = nullptr;
    // Destroy rendering items while their Wayland surfaces and output still exist.
    for (auto& client : clients_) {
        if (client->toplevel) disconnect(client->toplevel, nullptr, this, nullptr);
        if (client->item && client->item->surface()) disconnect(client->item->surface(), nullptr, this, nullptr);
        delete client->frame;
    }
    clients_.clear();
    for (auto* process : processes_) if (process->state() != QProcess::NotRunning) { process->terminate(); if (!process->waitForFinished(800)) { process->kill(); process->waitForFinished(800); } }
}

QProcess* WaylandCompositor::spawn(const QStringList& arguments, const QString& program) {
    auto* process = new QProcess(this);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("LUDASH_BIN_DIR", QCoreApplication::applicationDirPath());
    environment.insert("LUDASH_CONTROL", controlPath_);
    environment.insert("QSG_RHI_BACKEND", "opengl");
    environment.insert("WAYLAND_DISPLAY", QString::fromUtf8(compositor_.socketName()));
    environment.insert("QT_IM_MODULE", "wayland");
    environment.insert("QT_QPA_PLATFORM", "wayland"); environment.insert("XDG_SESSION_TYPE", "wayland");
    environment.insert("XDG_CURRENT_DESKTOP", "LuDash");
    if (xwayland_) xwayland_->applyEnvironment(environment); else environment.remove("DISPLAY");
    process->setProcessEnvironment(environment); process->setProcessChannelMode(QProcess::ForwardedChannels);
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
        if (shuttingDown_) return;
        processFailure_ = true;
        qWarning().noquote() << "LuDash child process error:" << process->errorString();
    });
    connect(process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (!shuttingDown_ && (code != 0 || status != QProcess::NormalExit)) processFailure_ = true;
    });
    if (program.isEmpty() && arguments.contains("--session")) {
        connect(process, &QProcess::started, this, [this, process] { shellProcessIds_.insert(process->processId()); });
    }
    if (program.isEmpty() && arguments.contains("--session")) {
        auto config = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "ludash/shell/shell.qml");
        if (config.isEmpty()) config = QStringLiteral(LUDASH_QML_SOURCE_DIR) + "/shell.qml";
        process->start(QStandardPaths::findExecutable("quickshell"), {"--path", config, "--no-color"});
    } else process->start(program.isEmpty() ? QCoreApplication::applicationDirPath() + "/ludash-desktop" : program, arguments);
    processes_ << process;
    return process;
}

bool WaylandCompositor::saveScreenshot(const QString& path) { return window_.grabWindow().save(path); }

QJsonObject WaylandCompositor::state() const {
    QJsonArray entries;
    for (const auto& client : clients_) entries.append(QJsonObject{
        {"contentWidth", client->item->width()}, {"contentHeight", client->item->height()},
        {"contentVisible", client->item->isVisible()}, {"contentPaintEnabled", client->item->isPaintEnabled()},
        {"bufferWidth", client->item->surface() ? client->item->surface()->bufferSize().width() : 0},
        {"id", client->id}, {"title", client->toplevel ? client->toplevel->title() : ""}, {"desktop", client->desktop},
        {"workspace", client->workspace}, {"visible", client->frame->isVisible()}, {"focused", client.get() == focused_},
        {"x", client->frame->x()}, {"y", client->frame->y()}, {"width", client->frame->width()}, {"height", client->frame->height()}, {"mapped", client->mapped}});
    return {{"blurReady", blurHealth_->ready.load()}, {"blurFailed", blurHealth_->failed.load()}, {"blurFrames", static_cast<int>(blurHealth_->frames.load())},
            {"activeAnimations", animations_->activeCount()}, {"xwayland", xwayland_ ? xwayland_->snapshot() : QJsonObject{}}, {"appearance", desktopPreferences()}, {"setupComplete", setupComplete()}, {"network", networkStatus_->snapshot()},
            {"system", systemStatus_->snapshot()}, {"wallpaperImage", wallpaperImageUrl()},
            {"workspace", workspace_}, {"processFailure", processFailure_}, {"clients", entries},
            {"layerSurfaces", layerShell_ ? layerShell_->mappedCount() : 0}, {"language", selectedLanguage()}, {"translations", languageDictionary(selectedLanguage())},
            {"wallpaper", QSettings().value("appearance/wallpaper", 0).toInt()}, {"shutdown", testStopping_},
            {"graphicsApi", renderState_->isOpenGLES ? "OpenGL ES" : "OpenGL"}, {"shaderReady", renderState_->shaderReady.load()},
            {"graphicsFailed", renderState_->failed.load()}, {"graphicsMajor", renderState_->majorVersion.load()}, {"graphicsMinor", renderState_->minorVersion.load()}};
}
void WaylandCompositor::saveState(const QString& path) {
    QFile file(path); if (file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(state()).toJson());
}
QJsonObject WaylandCompositor::control(const QJsonObject& request) {
    const auto method = request.value("method").toString(); const auto value = request.value("value").toString();
    if (method == "status") return state();
    bool numberValid = false; const int number = value.toInt(&numberValid);
    if (method == "workspace" && numberValid && number >= 0 && number < 4) { workspace_ = number; arrange(); focusNext(1); }
    else if (method == "language" && (value == "en_US" || value == "zh_TW")) QSettings().setValue("appearance/language", value);
    else if (method == "appearance") {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(value.toUtf8(), &parseError);
        QString error;
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) return {{"error", "Expected a JSON object of desktop preferences."}};
        if (!updateDesktopPreferences(document.object(), &error)) return {{"error", error}};
        arrange();
    }
    else if (method == "launch-x11") {
        QString error;
        if (!xwayland_ || !xwayland_->launch(QProcess::splitCommand(value), &error)) return {{"error", error.isEmpty() ? "XWayland is unavailable." : error}};
    }
    else if (method == "finish-setup") setSetupComplete(true);
    else if (method == "setup") setSetupComplete(false);
    else if (method == "configure-network") {
        auto program = QStandardPaths::findExecutable("nm-connection-editor");
        QStringList arguments;
        if (program.isEmpty() && !QStandardPaths::findExecutable("nmtui").isEmpty()) {
            for (const auto& terminal : {"foot", "konsole", "alacritty"}) {
                program = QStandardPaths::findExecutable(terminal);
                if (!program.isEmpty()) { arguments = {"-e", "nmtui"}; break; }
            }
        }
        if (program.isEmpty()) return {{"error", "Install nm-connection-editor, or NetworkManager nmtui with foot, konsole or alacritty."}};
        spawn(arguments, program);
    }
    else if (method == "wallpaper-image") {
        QString error;
        const QUrl url(value);
        if (!setWallpaperImage(url.isLocalFile() ? url.toLocalFile() : value, &error)) return {{"error", error}};
    }
    else if (method == "wallpaper-default") {
        QSettings().remove("appearance/wallpaperImage"); QSettings().setValue("appearance/wallpaperMode", "image");
    }
    else if (method == "wallpaper" && numberValid && number >= 0 && number <= 1) { QSettings().setValue("appearance/wallpaper", number); QSettings().setValue("appearance/wallpaperMode", "shader"); wallpaper_->setPalette(number); }
    else if (method == "quit") QTimer::singleShot(0, this, &WaylandCompositor::requestShutdown);
    else if ((method == "focus" || method == "close" || method == "minimize") && numberValid) {
        for (const auto& client : clients_) if (client->id == number) {
            if (method == "close") client->toplevel->sendClose();
            else if (method == "minimize") { client->minimized = true; arrange(); focusNext(1); }
            else { workspace_ = client->workspace; client->minimized = false; arrange(); focus(client.get()); }
            return state();
        }
        return {{"error", "Unknown window"}};
    } else return {{"error", "Invalid command or value"}};
    return state();
}

bool WaylandCompositor::hasProcessFailure() const { return processFailure_ || renderState_->failed || blurHealth_->failed || !renderState_->shaderReady; }

void WaylandCompositor::closeTestSession(const std::function<void(bool)>& finished) {
    testStopping_ = true;
    for (const auto& client : clients_) if (client->toplevel) client->toplevel->sendClose();
    auto* timer = new QTimer(this);
    auto elapsed = std::make_shared<int>(0);
    connect(timer, &QTimer::timeout, this, [this, timer, elapsed, finished] {
        *elapsed += 50;
        if (clients_.empty() && xwayland_) xwayland_->stop();
        const bool compatibilityFinished = (!xwayland_ || xwayland_->stopped()) && animations_->activeCount() == 0;
        const bool processesFinished = std::all_of(processes_.begin(), processes_.end(), [](const auto* process) { return process->state() == QProcess::NotRunning; });
        if ((clients_.empty() && processesFinished && compatibilityFinished) || *elapsed >= 5000) {
            timer->stop(); timer->deleteLater();
            const bool clean = clients_.empty() && processesFinished && compatibilityFinished && !processFailure_;
            if (!clean) qWarning("LuDash test session did not shut down cleanly.");
            finished(clean);
        }
    });
    timer->start(50);
}

void WaylandCompositor::requestShutdown() {
    logoutPending_ = true;
    bool hasApplications = false;
    for (const auto& client : clients_) if (!client->desktop && client->toplevel) { hasApplications = true; client->toplevel->sendClose(); }
    if (!hasApplications) QCoreApplication::exit(hasProcessFailure() ? 2 : 0);
}

void WaylandCompositor::addWindow(QWaylandXdgToplevel* toplevel, QWaylandXdgSurface* surface) {
    auto client = std::make_unique<ClientWindow>();
    client->id = nextWindowId_++;
    client->toplevel = toplevel; client->workspace = workspace_;
    client->frame = new WindowFrame(window_.contentItem());
    client->frame->setVisible(false);
    client->frame->setOpacity(.999);
    client->blur = new BlurItem(blurHealth_, client->frame);
    client->item = new QWaylandQuickShellSurfaceItem(client->frame);
    client->item->setShellSurface(surface); client->item->setOutput(output_);
    client->item->setAutoCreatePopupItems(true);
    client->item->setFocusOnClick(true);
    connect(client->item, &QWaylandQuickItem::surfaceDestroyed, client->item, [item = client->item] { if (item) { item->setBufferLocked(true); item->setInputEventsEnabled(false); } });
    auto* current = client.get(); clients_.push_back(std::move(client));
    current->frame->clicked = [this, current](bool close) { if (close && current->toplevel) current->toplevel->sendClose(); else focus(current); };
    connect(toplevel, &QWaylandXdgToplevel::titleChanged, this, [current] { current->frame->title = current->toplevel->title(); current->frame->update(); });
    connect(toplevel, &QWaylandXdgToplevel::appIdChanged, this, [this, current] {
        current->desktop = current->toplevel->appId() == "ludash-shell"
            && shellProcessIds_.contains(current->item->surface()->client()->processId());
        arrange();
    });
    connect(surface->surface(), &QWaylandSurface::hasContentChanged, this, [this, current] {
        const auto* attached = current->item->surface();
        const bool hasContent = attached && attached->hasContent();
        const bool newlyMapped = !current->mapped && hasContent;
        current->mapped = hasContent; arrange();
        if (newlyMapped && !current->desktop) { pluginManager_->windowOpened(current->frame); focus(current); }
    });
    connect(toplevel, &QWaylandXdgToplevel::parentToplevelChanged, this, [this, current] { current->floating = current->toplevel->parentToplevel(); arrange(); });
    connect(toplevel, &QWaylandXdgToplevel::setMinimized, this, [this, current] { current->minimized = true; arrange(); focusNext(1); });
    connect(toplevel, &QWaylandXdgToplevel::setMaximized, this, [this, current] { current->maximized = true; arrange(); });
    connect(toplevel, &QWaylandXdgToplevel::unsetMaximized, this, [this, current] { current->maximized = false; arrange(); });
    connect(toplevel, &QObject::destroyed, this, [this, current] {
        if (focused_ == current) focused_ = nullptr;
        if (current->item && current->item->surface()) disconnect(current->item->surface(), nullptr, this, nullptr);
        current->frame->clicked = {};
        current->item->setBufferLocked(true); current->item->setInputEventsEnabled(false);
        auto* frame = current->frame;
        if (frame->isVisible()) animations_->hide(frame, [frame] { frame->deleteLater(); });
        else frame->deleteLater();
        std::erase_if(clients_, [current](const auto& entry) { return entry.get() == current; });
        arrange(); focusNext(1);
        if (logoutPending_) {
            const bool hasApplications = std::any_of(clients_.begin(), clients_.end(), [](const auto& entry) { return !entry->desktop; });
            if (!hasApplications) QCoreApplication::exit(hasProcessFailure() ? 2 : 0);
        }
    });
    arrange();
}

void WaylandCompositor::configure(ClientWindow* client, const QRect& rectangle) {
    const int border = client->desktop ? 0 : 1;
    const int title = client->desktop ? 0 : 24;
    client->frame->setPosition(rectangle.topLeft()); client->frame->setSize(rectangle.size());
    client->blur->setSize(rectangle.size());
    client->item->setPosition(QPointF(border, title));
    const QSize size(std::max(1, rectangle.width() - 2 * border), std::max(1, rectangle.height() - title - border));
    if (size != client->lastSize && client->toplevel) {
        client->lastSize = size;
        client->toplevel->sendConfigure(size, QList<QWaylandXdgToplevel::State>{});
    }
}

void WaylandCompositor::arrange() {
    if (wallpaper_) wallpaper_->setSize(window_.size());
    const auto preferences = desktopPreferences();
    animations_->setDuration(preferences.value("animations").toBool() ? preferences.value("animationDuration").toInt() : 0);
    const int gap = preferences.value("gap").toInt();
    const int top = preferences.value("panelHeight").toInt() + gap;
    const QRect workArea(gap, top, std::max(1, window_.width() - gap * 2), std::max(1, window_.height() - top - gap));
    QList<ClientWindow*> tiled;
    for (const auto& client : clients_) {
        client->frame->accent = QColor(preferences.value("accent").toString());
        client->frame->update();
        client->blur->setRadius(preferences.value("blur").toBool() && !client->desktop ? preferences.value("blurRadius").toInt() : 0);
        client->item->setOpacity(client->desktop ? 1 : preferences.value("windowOpacity").toInt() / 100.0);
        const bool visible = client->mapped && !client->minimized && (client->desktop || client->workspace == workspace_);
        if (visible != client->presented) {
            client->presented = visible;
            client->item->setInputEventsEnabled(visible);
            if (visible) animations_->show(client->frame);
            else animations_->hide(client->frame);
        }
        if (client->desktop) {
            client->frame->setZ(-100); configure(client.get(), QRect(0, 0, window_.width(), window_.height()));
        } else if (client->workspace == workspace_ && !client->minimized) {
            client->frame->setZ(client->floating ? 10 : 1);
            if (client->floating) configure(client.get(), QRect((window_.width() - 720) / 2, (window_.height() - 500) / 2, 720, 500));
            else tiled << client.get();
        }
    }
    const auto rectangles = tileRectangles(workArea, static_cast<int>(tiled.size()), ratio_, gap);
    for (qsizetype i = 0; i < std::min(tiled.size(), rectangles.size()); ++i) configure(tiled[i], rectangles[i]);
    for (const auto& client : clients_) if (client->maximized && !client->minimized && client->workspace == workspace_) {
        configure(client.get(), workArea); client->frame->setZ(30);
    }
    window_.setTitle(QString("LuDash Wayland · workspace %1").arg(workspace_ + 1));
}

void WaylandCompositor::focus(ClientWindow* client) {
    if (!client || !client->mapped || !client->item || !client->frame->isVisible()) return;
    focused_ = client; client->item->takeFocus();
    pluginManager_->windowFocused(client->frame);
    client->frame->setZ(client->floating ? 20 : 2);
}

void WaylandCompositor::focusNext(int direction) {
    QList<ClientWindow*> visible;
    for (const auto& client : clients_) if (!client->desktop && client->mapped && !client->minimized && client->workspace == workspace_) visible << client.get();
    if (visible.isEmpty()) {
        compositor_.defaultSeat()->setKeyboardFocus(nullptr);
        for (const auto& client : clients_) if (client->desktop) client->item->takeFocus();
        focused_ = nullptr; return;
    }
    const qsizetype index = visible.indexOf(focused_);
    focus(visible[(index + direction + visible.size()) % visible.size()]);
}

bool WaylandCompositor::eventFilter(QObject* watched, QEvent* event) {
    if (watched == &window_ && event->type() == QEvent::Close) {
        event->ignore(); requestShutdown(); return true;
    }
    if (watched == &window_ && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (event->type() == QEvent::KeyRelease && consumedKeys_.remove(key->key())) return true;
        if (event->type() == QEvent::KeyPress && key->modifiers().testFlag(Qt::MetaModifier)) {
            bool handled = true;
            if (key->key() >= Qt::Key_1 && key->key() <= Qt::Key_4) {
                const int target = key->key() - Qt::Key_1;
                if (key->modifiers().testFlag(Qt::ShiftModifier)) { if (focused_) focused_->workspace = target; }
                else workspace_ = target;
                arrange(); focusNext(1);
            } else switch (key->key()) {
                case Qt::Key_J: focusNext(1); break;
                case Qt::Key_K: focusNext(-1); break;
                case Qt::Key_H: ratio_ = std::max(.3, ratio_ - .05); arrange(); break;
                case Qt::Key_L: ratio_ = std::min(.7, ratio_ + .05); arrange(); break;
                case Qt::Key_F: if (focused_) { focused_->maximized = !focused_->maximized; arrange(); } break;
                case Qt::Key_M: if (focused_) { focused_->minimized = true; arrange(); focusNext(1); } break;
                case Qt::Key_Q: if (focused_ && focused_->toplevel) focused_->toplevel->sendClose(); break;
                case Qt::Key_Space: if (focused_) { focused_->floating = !focused_->floating; arrange(); } break;
                case Qt::Key_Return: spawn({"--app", "console"}); break;
                case Qt::Key_E: spawn({"--app", "files"}); break;
                case Qt::Key_D: spawn({"--app", "launcher"}); break;
                default: handled = false;
            }
            if (handled) { consumedKeys_.insert(key->key()); return true; }
        }
    }
    return QObject::eventFilter(watched, event);
}
}
