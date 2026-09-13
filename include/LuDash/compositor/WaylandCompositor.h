#pragma once
#include <QObject>
#include <LuDash/renderer/RenderBackend.h>
#include <QQuickWindow>
#include <QSet>
#include <QJsonObject>
#include <QProcess>
#include <QtWaylandCompositor/QWaylandQuickCompositor>
#include <memory>
#include <vector>
#include <functional>
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
class ControlServer;
struct ClientWindow;
class WaylandCompositor final : public QObject {
public:
    WaylandCompositor(const QByteArray& socket, bool fullscreen, bool startShell, GraphicsApi graphics = GraphicsApi::Auto);
    ~WaylandCompositor() override;
    QProcess* spawn(const QStringList& arguments, const QString& program = {});
    bool saveScreenshot(const QString& path);
    void saveState(const QString& path);
    bool hasProcessFailure() const;
    void closeTestSession(const std::function<void(bool)>& finished);
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    std::shared_ptr<RenderState> renderState_;
    WallpaperItem* wallpaper_ = nullptr;
    QWaylandQuickCompositor compositor_;
    QQuickWindow window_;
    ShellModules* shellModules_ = nullptr;
    AudioSettings* audioSettings_ = nullptr;
    PowerSettings* powerSettings_ = nullptr;
    int settingsSerial_ = 0;
    QString settingsPage_ = "general";
    SystemStatus* systemStatus_ = nullptr;
    NetworkStatus* networkStatus_ = nullptr;
    WindowAnimations* animations_ = nullptr;
    XWaylandSupport* xwayland_ = nullptr;
    std::shared_ptr<BlurHealth> blurHealth_;
    PluginManager* pluginManager_ = nullptr;
    LayerShell* layerShell_ = nullptr;
    ControlServer* controlServer_ = nullptr;
    QString controlPath_;
    int nextWindowId_ = 1;
    QWaylandXdgShell* shell_ = nullptr;
    QWaylandQuickOutput* output_ = nullptr;
    std::vector<std::unique_ptr<ClientWindow>> clients_;
    QList<QProcess*> processes_;
    QSet<qint64> shellProcessIds_;
    ClientWindow* focused_ = nullptr;
    int workspace_ = 0;
    double ratio_ = .56;
    QSet<int> consumedKeys_;
    bool shuttingDown_ = false;
    bool logoutPending_ = false;
    bool testStopping_ = false;
    bool processFailure_ = false;
    void requestShutdown();
    void addWindow(QWaylandXdgToplevel* toplevel, QWaylandXdgSurface* surface);
    void configure(ClientWindow* client, const QRect& rectangle);
    void arrange();
    QJsonObject state() const;
    QJsonObject control(const QJsonObject& request);
    void focus(ClientWindow* client);
    void focusNext(int direction);
};
}
