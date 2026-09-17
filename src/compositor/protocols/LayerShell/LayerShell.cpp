#include "compositor/protocols/LayerSurface/LayerSurface.hpp"
#include <QQuickWindow>
#include <QTimer>
#include <QtWaylandCompositor/QWaylandQuickCompositor>
#include <QtWaylandCompositor/QWaylandQuickOutput>
#include <QtWaylandCompositor/QWaylandQuickItem>
#include <QtWaylandCompositor/QWaylandSurface>
#include <QtWaylandCompositor/QWaylandXdgShell>
#include <wayland-server-core.h>
#include "wlr-layer-shell-server.h"
#include <algorithm>
namespace LuDash {
LayerShell::LayerShell(QWaylandQuickCompositor* compositor, QWaylandQuickOutput* output, QQuickWindow* window)
    : QObject(compositor), compositor_(compositor), output_(output), window_(window),
      global_(wl_global_create(compositor->display(), &zwlr_layer_shell_v1_interface, 2, this, bind)) {
    connect(window, &QQuickWindow::widthChanged, this, &LayerShell::arrange);
    connect(window, &QQuickWindow::heightChanged, this, &LayerShell::arrange);
}
LayerShell::~LayerShell() {
    while (!surfaces_.isEmpty()) delete surfaces_.first();
    if (global_) wl_global_destroy(global_);
}
void LayerShell::bind(wl_client* client, void* data, uint32_t version, uint32_t id) {
    static const struct zwlr_layer_shell_v1_interface implementation = {createSurface};
    auto* resource = wl_resource_create(client, &zwlr_layer_shell_v1_interface, static_cast<int>(std::min(version, 2u)), id);
    if (!resource) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(resource, &implementation, data, nullptr);
}
void LayerShell::createSurface(wl_client* client, wl_resource* resource, uint32_t id, wl_resource* surfaceResource, wl_resource*, uint32_t layer, const char*) {
    auto* shell = static_cast<LayerShell*>(wl_resource_get_user_data(resource));
    auto* surface = QWaylandSurface::fromResource(surfaceResource);
    if (!surface || layer > 3) { wl_resource_post_error(resource, 1, "Invalid layer surface"); return; }
    static QWaylandSurfaceRole role("zwlr_layer_surface_v1");
    if (!surface->setRole(&role, resource, 0)) return;
    if (surface->hasContent()) { wl_resource_post_error(resource, 2, "Layer surface already has a buffer"); return; }
    auto* layerResource = wl_resource_create(client, &zwlr_layer_surface_v1_interface, wl_resource_get_version(resource), id);
    if (!layerResource) { wl_client_post_no_memory(client); return; }
    auto* layerSurface = new LayerSurface(shell, layerResource, surface, layer);
    shell->surfaces_ << layerSurface;
}
int LayerShell::mappedCount() const {
    int count = 0;
    for (const auto* surface : surfaces_) if (surface->surface_ && surface->surface_->hasContent()) ++count;
    return count;
}
void LayerShell::closeSurfaces() { for (const auto* surface : surfaces_) if (surface->resource_) zwlr_layer_surface_v1_send_closed(surface->resource_); }
void LayerShell::arrange() { for (auto* surface : surfaces_) surface->configure(); }
}
