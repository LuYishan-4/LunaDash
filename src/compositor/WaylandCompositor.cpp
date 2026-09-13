#include <LuDash/renderer/WallpaperItem.h>
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
    wallpaper_ = new WallpaperItem(graphics, renderState_, window_.contentItem());
    wallpaper_->setSize(window_.size()); wallpaper_->setZ(-200);
    wallpaper_->setPalette(QSettings().value("appearance/wallpaper", 0).toInt());
    window_.setColor(QColor("#171c36")); window_.installEventFilter(this);
    compositor_.setSocketName(socket);
    shell_ = new QWaylandXdgShell(&compositor_);
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
    if (startShell) {
        auto* process = spawn({"--session"});
        QTimer::singleShot(400, this, [this] { spawn({"--app", "welcome"}); });
        connect(process, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
            if (shuttingDown_ || testStopping_) return;
            // Restore the panel while applications finish their save/discard dialogs.
            requestShutdown();
            if (!clients_.empty()) spawn({"--session", "--no-welcome"});
        });
    }
    qInfo().noquote() << "LuDash Wayland socket:" << socket;
}

WaylandCompositor::~WaylandCompositor() {
    shuttingDown_ = true;
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

QProcess* WaylandCompositor::spawn(const QStringList& arguments) {
    auto* process = new QProcess(this);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("LUDASH_BIN_DIR", QCoreApplication::applicationDirPath());
    environment.insert("LUDASH_CONTROL", controlPath_);
    environment.insert("QSG_RHI_BACKEND", "opengl");
    environment.insert("WAYLAND_DISPLAY", QString::fromUtf8(compositor_.socketName()));
    environment.insert("QT_IM_MODULE", "wayland");
    environment.insert("QT_QPA_PLATFORM", "wayland"); environment.insert("XDG_SESSION_TYPE", "wayland");
    environment.insert("XDG_CURRENT_DESKTOP", "LuDash"); environment.remove("DISPLAY");
    process->setProcessEnvironment(environment); process->setProcessChannelMode(QProcess::ForwardedChannels);
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
        if (shuttingDown_) return;
        processFailure_ = true;
        qWarning().noquote() << "LuDash child process error:" << process->errorString();
    });
    connect(process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (!shuttingDown_ && (code != 0 || status != QProcess::NormalExit)) processFailure_ = true;
    });
    if (arguments.contains("--session")) {
        connect(process, &QProcess::started, this, [this, process] { shellProcessIds_.insert(process->processId()); });
    }
    if (arguments.contains("--session")) {
        auto config = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "ludash/shell/shell.qml");
        if (config.isEmpty()) config = QStringLiteral(LUDASH_QML_SOURCE_DIR) + "/shell.qml";
        process->start(QStandardPaths::findExecutable("quickshell"), {"--path", config, "--no-color"});
    } else process->start(QCoreApplication::applicationDirPath() + "/ludash-desktop", arguments);
    processes_ << process;
    return process;
}

bool WaylandCompositor::saveScreenshot(const QString& path) { return window_.grabWindow().save(path); }

QJsonObject WaylandCompositor::state() const {
    QJsonArray entries;
    for (const auto& client : clients_) entries.append(QJsonObject{
        {"id", client->id}, {"title", client->toplevel ? client->toplevel->title() : ""}, {"desktop", client->desktop},
        {"workspace", client->workspace}, {"visible", client->frame->isVisible()}, {"focused", client.get() == focused_},
        {"x", client->frame->x()}, {"y", client->frame->y()}, {"width", client->frame->width()}, {"height", client->frame->height()}, {"mapped", client->mapped}});
    return {{"workspace", workspace_}, {"processFailure", processFailure_}, {"clients", entries},
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
    else if (method == "wallpaper" && numberValid && number >= 0 && number <= 1) { QSettings().setValue("appearance/wallpaper", number); wallpaper_->setPalette(number); }
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

bool WaylandCompositor::hasProcessFailure() const { return processFailure_ || renderState_->failed || !renderState_->shaderReady; }

void WaylandCompositor::closeTestSession(const std::function<void(bool)>& finished) {
    testStopping_ = true;
    for (const auto& client : clients_) if (client->toplevel) client->toplevel->sendClose();
    auto* timer = new QTimer(this);
    auto elapsed = std::make_shared<int>(0);
    connect(timer, &QTimer::timeout, this, [this, timer, elapsed, finished] {
        *elapsed += 50;
        const bool processesFinished = std::all_of(processes_.begin(), processes_.end(), [](const auto* process) { return process->state() == QProcess::NotRunning; });
        if ((clients_.empty() && processesFinished) || *elapsed >= 5000) {
            timer->stop(); timer->deleteLater();
            const bool clean = clients_.empty() && processesFinished && !processFailure_;
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
    if (!hasApplications) QCoreApplication::quit();
}

void WaylandCompositor::addWindow(QWaylandXdgToplevel* toplevel, QWaylandXdgSurface* surface) {
    auto client = std::make_unique<ClientWindow>();
    client->id = nextWindowId_++;
    client->toplevel = toplevel; client->workspace = workspace_;
    client->frame = new WindowFrame(window_.contentItem());
    client->frame->setVisible(false);
    client->item = new QWaylandQuickShellSurfaceItem(client->frame);
    client->item->setShellSurface(surface); client->item->setOutput(output_);
    client->item->setAutoCreatePopupItems(true);
    client->item->setFocusOnClick(true);
    auto* current = client.get(); clients_.push_back(std::move(client));
    current->frame->clicked = [this, current](bool close) { if (close && current->toplevel) current->toplevel->sendClose(); else focus(current); };
    connect(toplevel, &QWaylandXdgToplevel::titleChanged, this, [this, current] { current->frame->title = current->toplevel->title(); current->frame->update(); });
    connect(toplevel, &QWaylandXdgToplevel::appIdChanged, this, [this, current] {
        current->desktop = current->toplevel->appId() == "ludash-shell"
            && shellProcessIds_.contains(current->item->surface()->client()->processId());
        arrange();
    });
    connect(surface->surface(), &QWaylandSurface::hasContentChanged, this, [this, current] {
        const bool newlyMapped = !current->mapped && current->item->surface()->hasContent();
        current->mapped = current->item->surface()->hasContent(); arrange();
        if (newlyMapped && !current->desktop) { pluginManager_->windowOpened(current->frame); focus(current); }
    });
    connect(toplevel, &QWaylandXdgToplevel::parentToplevelChanged, this, [this, current] { current->floating = current->toplevel->parentToplevel(); arrange(); });
    connect(toplevel, &QWaylandXdgToplevel::setMinimized, this, [this, current] { current->minimized = true; arrange(); focusNext(1); });
    connect(toplevel, &QWaylandXdgToplevel::setMaximized, this, [this, current] { current->maximized = true; arrange(); });
    connect(toplevel, &QWaylandXdgToplevel::unsetMaximized, this, [this, current] { current->maximized = false; arrange(); });
    connect(toplevel, &QObject::destroyed, this, [this, current] {
        if (focused_ == current) focused_ = nullptr;
        if (current->item && current->item->surface()) disconnect(current->item->surface(), nullptr, this, nullptr);
        current->frame->deleteLater();
        std::erase_if(clients_, [current](const auto& entry) { return entry.get() == current; });
        arrange(); focusNext(1);
        if (logoutPending_) {
            const bool hasApplications = std::any_of(clients_.begin(), clients_.end(), [](const auto& entry) { return !entry->desktop; });
            if (!hasApplications) QCoreApplication::quit();
        }
    });
    arrange();
}

void WaylandCompositor::configure(ClientWindow* client, const QRect& rectangle) {
    const int border = client->desktop ? 0 : 1;
    const int title = client->desktop ? 0 : 30;
    client->frame->setPosition(rectangle.topLeft()); client->frame->setSize(rectangle.size());
    client->item->setPosition(QPointF(border, title));
    const QSize size(std::max(1, rectangle.width() - 2 * border), std::max(1, rectangle.height() - title - border));
    if (size != client->lastSize && client->toplevel) {
        client->lastSize = size;
        client->toplevel->sendConfigure(size, QList<QWaylandXdgToplevel::State>{});
    }
}

void WaylandCompositor::arrange() {
    if (wallpaper_) wallpaper_->setSize(window_.size());
    QList<ClientWindow*> tiled;
    for (const auto& client : clients_) {
        client->frame->setVisible(client->mapped && !client->minimized && (client->desktop || client->workspace == workspace_));
        if (client->desktop) {
            client->frame->setZ(-100); configure(client.get(), QRect(0, 0, window_.width(), window_.height()));
        } else if (client->workspace == workspace_ && !client->minimized) {
            client->frame->setZ(client->floating ? 10 : 1);
            if (client->floating) configure(client.get(), QRect((window_.width() - 720) / 2, (window_.height() - 500) / 2, 720, 500));
            else tiled << client.get();
        }
    }
    const auto rectangles = tileRectangles(QRect(16, 62, window_.width() - 32, window_.height() - 158), static_cast<int>(tiled.size()), ratio_);
    for (int i = 0; i < tiled.size(); ++i) configure(tiled[i], rectangles[i]);
    for (const auto& client : clients_) if (client->maximized && !client->minimized && client->workspace == workspace_) {
        configure(client.get(), QRect(16, 62, window_.width() - 32, window_.height() - 158)); client->frame->setZ(30);
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
