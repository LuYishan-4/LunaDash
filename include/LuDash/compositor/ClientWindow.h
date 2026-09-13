#pragma once
#include <QPointer>
#include <QSize>
class QWaylandXdgToplevel;
class QWaylandQuickShellSurfaceItem;
namespace LuDash {
class WindowFrame;
struct ClientWindow {
    QPointer<QWaylandXdgToplevel> toplevel;
    QPointer<QWaylandQuickShellSurfaceItem> item;
    WindowFrame* frame = nullptr;
    int workspace = 0;
    int id = 0;
    bool floating = false;
    bool desktop = false;
    bool mapped = false;
    bool minimized = false;
    bool maximized = false;
    QSize lastSize;
};
}
