#pragma once
#include <QObject>
#include <QPointer>
#include <QMargins>
#include <QSize>
#include <QList>
#include <cstdint>
class QQuickWindow;
class QWaylandQuickCompositor;
class QWaylandQuickOutput;
class QWaylandQuickItem;
class QWaylandSurface;
struct wl_global;
struct wl_resource;
struct wl_client;
namespace LuDash {
class LayerSurface;
class LayerShell final : public QObject {
public:
    LayerShell(QWaylandQuickCompositor* compositor, QWaylandQuickOutput* output, QQuickWindow* window);
    ~LayerShell() override;
    int mappedCount() const;
    void closeSurfaces();
    void arrange();
private:
    friend class LayerSurface;
    QWaylandQuickCompositor* compositor_;
    QWaylandQuickOutput* output_;
    QQuickWindow* window_;
    wl_global* global_;
    QList<LayerSurface*> surfaces_;
    static void bind(wl_client* client, void* data, uint32_t version, uint32_t id);
    static void createSurface(wl_client* client, wl_resource* resource, uint32_t id, wl_resource* surface, wl_resource* output, uint32_t layer, const char* name);
};
}
