#include "compositor/protocols/LayerSurface/LayerSurface.hpp"
#include <QQuickWindow>
#include <QTimer>
#include <QtWaylandCompositor/QWaylandQuickCompositor>
#include <QtWaylandCompositor/QWaylandQuickOutput>
#include <QtWaylandCompositor/QWaylandQuickItem>
#include <QtWaylandCompositor/QWaylandSurface>
#include <QtWaylandCompositor/QWaylandSeat>
#include <QtWaylandCompositor/QWaylandXdgShell>
#include <wayland-server-core.h>
#include "wlr-layer-shell-server.h"
#include <algorithm>
namespace LuDash {
LayerSurface::LayerSurface(LayerShell* shell, wl_resource* resource, QWaylandSurface* surface, uint32_t layer)
    : QObject(shell), shell_(shell), resource_(resource), surface_(surface), item_(new QWaylandQuickItem(shell->window_->contentItem())), layer_(layer) {
    static const struct zwlr_layer_surface_v1_interface implementation = {setSize, setAnchor, setZone, setMargin, setKeyboard, getPopup, acknowledge, destroy, setLayer};
    wl_resource_set_implementation(resource, &implementation, this, resourceDestroyed);
    item_->setSurface(surface); item_->setOutput(shell->output_); item_->setFocusOnClick(false);
    item_->setZ(layer == 0 ? -100 : layer == 1 ? -50 : layer == 2 ? 100 : 200);
    item_->setVisible(false);
    QTimer::singleShot(0, this, &LayerSurface::configure);
    connect(surface, &QWaylandSurface::redraw, this, [this] {
        configure();
        const bool visible = surface_ && surface_->hasContent() && acknowledged_;
        item_->setVisible(visible);
        auto *seat = shell_->compositor_->defaultSeat();
        if (visible && keyboard_ == 1) {
            // Layer-shell keyboard interactivity can change while the surface
            // remains mapped. Reassert the surface-level focus on redraw so a
            // newly focused QML TextField does not have to wait for another
            // physical key event before the client receives keyboard input.
            if (!seat || seat->keyboardFocus() != surface_)
                item_->takeFocus();
        } else if (!visible && seat && seat->keyboardFocus() == surface_) {
            seat->setKeyboardFocus(nullptr);
        }
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
    item_->setZ(layer_ == 0 ? -100 : layer_ == 1 ? -50 : layer_ == 2 ? 100 : 200);
    const int screenWidth = std::max(1, shell_->window_->width()), screenHeight = std::max(1, shell_->window_->height());
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
void LayerSurface::setLayer(wl_client*, wl_resource* resource, uint32_t layer) {
    if (layer > 3) { wl_resource_post_error(resource, 0, "Invalid layer"); return; }
    get(resource)->layer_ = layer;
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
    auto *surface = get(resource);
    if (!surface) return;
    surface->keyboard_ = keyboard;
    surface->item_->setFocusOnClick(keyboard != 0);

    auto *seat = surface->shell_->compositor_->defaultSeat();
    if (keyboard == 0) {
        if (seat && seat->keyboardFocus() == surface->surface_)
            seat->setKeyboardFocus(nullptr);
        return;
    }

    // Quickshell commonly switches None -> Exclusive without remapping the
    // layer surface. Focus it immediately instead of waiting for the next
    // redraw/key event. The zero-delay callback also lets the client's commit
    // containing keyboard_interactivity finish first.
    QTimer::singleShot(0, surface, [surface] {
        if (!surface->resource_ || !surface->surface_ || !surface->item_ ||
            !surface->item_->isVisible() || surface->keyboard_ != 1)
            return;
        auto *seat = surface->shell_->compositor_->defaultSeat();
        if (!seat || seat->keyboardFocus() != surface->surface_)
            surface->item_->takeFocus();
    });
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
