#include <LuDash/layer_shell/LayerShell.h>
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
      global_(wl_global_create(compositor->display(), &zwlr_layer_shell_v1_interface, 1, this, bind)) {
    connect(window, &QQuickWindow::widthChanged, this, &LayerShell::arrange);
    connect(window, &QQuickWindow::heightChanged, this, &LayerShell::arrange);
}
LayerShell::~LayerShell() {
    while (!surfaces_.isEmpty()) delete surfaces_.first();
    if (global_) wl_global_destroy(global_);
}
void LayerShell::bind(wl_client* client, void* data, uint32_t, uint32_t id) {
    static const struct zwlr_layer_shell_v1_interface implementation = {createSurface};
    auto* resource = wl_resource_create(client, &zwlr_layer_shell_v1_interface, 1, id);
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
    auto* layerResource = wl_resource_create(client, &zwlr_layer_surface_v1_interface, 1, id);
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
LayerSurface::LayerSurface(LayerShell* shell, wl_resource* resource, QWaylandSurface* surface, uint32_t layer)
    : QObject(shell), shell_(shell), resource_(resource), surface_(surface), item_(new QWaylandQuickItem(shell->window_->contentItem())), layer_(layer) {
    static const struct zwlr_layer_surface_v1_interface implementation = {setSize, setAnchor, setZone, setMargin, setKeyboard, getPopup, acknowledge, destroy};
    wl_resource_set_implementation(resource, &implementation, this, resourceDestroyed);
    item_->setSurface(surface); item_->setOutput(shell->output_); item_->setFocusOnClick(false);
    item_->setZ(layer == 0 ? -100 : layer == 1 ? -50 : layer == 2 ? 100 : 200);
    item_->setVisible(false);
    QTimer::singleShot(0, this, &LayerSurface::configure);
    connect(surface, &QWaylandSurface::redraw, this, [this] {
        configure();
        const bool visible = surface_ && surface_->hasContent() && acknowledged_;
        const bool newlyMapped = visible && !item_->isVisible();
        item_->setVisible(visible);
        if (newlyMapped && keyboard_ == 1) item_->takeFocus();
    });
    connect(surface, &QObject::destroyed, this, [this] { if (resource_) wl_resource_destroy(resource_); });
}
LayerSurface::~LayerSurface() {
    if (surface_) disconnect(surface_, nullptr, this, nullptr);
    if (resource_) { wl_resource_set_user_data(resource_, nullptr); wl_resource_destroy(resource_); }
    shell_->surfaces_.removeAll(this); delete item_;
}
LayerSurface* LayerSurface::get(wl_resource* resource) { return static_cast<LayerSurface*>(wl_resource_get_user_data(resource)); }
void LayerSurface::configure() {
    if (!surface_ || !resource_) return;
    const int screenWidth = shell_->window_->width(), screenHeight = shell_->window_->height();
    const bool left = anchor_ & 4, right = anchor_ & 8, top = anchor_ & 1, bottom = anchor_ & 2;
    const int width = desired_.width() ? desired_.width() : (left && right ? screenWidth - margins_.left() - margins_.right() : 1);
    const int height = desired_.height() ? desired_.height() : (top && bottom ? screenHeight - margins_.top() - margins_.bottom() : 1);
    const QSize size(std::clamp(width, 1, screenWidth), std::clamp(height, 1, screenHeight));
    const int x = left ? margins_.left() : right ? screenWidth - size.width() - margins_.right() : (screenWidth - size.width()) / 2;
    const int y = top ? margins_.top() : bottom ? screenHeight - size.height() - margins_.bottom() : (screenHeight - size.height()) / 2;
    item_->setPosition(QPointF(x, y));
    if (size != configured_) {
        configured_ = size; serial_ = wl_display_next_serial(shell_->compositor_->display());
        zwlr_layer_surface_v1_send_configure(resource_, serial_, static_cast<uint32_t>(size.width()), static_cast<uint32_t>(size.height()));
    }
}
void LayerSurface::setSize(wl_client*, wl_resource* resource, uint32_t width, uint32_t height) {
    if (width > 16384 || height > 16384) { wl_resource_post_error(resource, 1, "Layer size exceeds limit"); return; }
    get(resource)->desired_ = QSize(static_cast<int>(width), static_cast<int>(height));
}
void LayerSurface::setAnchor(wl_client*, wl_resource* resource, uint32_t anchor) {
    if (anchor > 15) { wl_resource_post_error(resource, 0, "Invalid anchor"); return; }
    get(resource)->anchor_ = anchor;
}
void LayerSurface::setZone(wl_client*, wl_resource* resource, int32_t zone) { get(resource)->zone_ = zone; }
void LayerSurface::setMargin(wl_client*, wl_resource* resource, int32_t top, int32_t right, int32_t bottom, int32_t left) {
    get(resource)->margins_ = QMargins(std::clamp(left, -16384, 16384), std::clamp(top, -16384, 16384), std::clamp(right, -16384, 16384), std::clamp(bottom, -16384, 16384));
}
void LayerSurface::setKeyboard(wl_client*, wl_resource* resource, uint32_t keyboard) {
    if (keyboard > 1) { wl_resource_post_error(resource, 0, "Unsupported keyboard interactivity"); return; }
    get(resource)->keyboard_ = keyboard; get(resource)->item_->setFocusOnClick(keyboard != 0);
}
void LayerSurface::getPopup(wl_client*, wl_resource* resource, wl_resource*) {
    wl_resource_post_error(resource, 0, "Layer popups are not supported; use an overlay layer");
}
void LayerSurface::acknowledge(wl_client*, wl_resource* resource, uint32_t serial) {
    auto* surface = get(resource);
    if (serial == surface->serial_) surface->acknowledged_ = true;
}
void LayerSurface::destroy(wl_client*, wl_resource* resource) { wl_resource_destroy(resource); }
void LayerSurface::resourceDestroyed(wl_resource* resource) {
    if (auto* surface = get(resource)) { surface->resource_ = nullptr; delete surface; }
}
}
