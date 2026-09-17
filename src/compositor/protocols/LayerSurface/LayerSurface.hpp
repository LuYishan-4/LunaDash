#pragma once
#include "compositor/protocols/LayerShell/LayerShell.hpp"
namespace LuDash {
class LayerSurface final : public QObject {
public:
    LayerSurface(LayerShell* shell, wl_resource* resource, QWaylandSurface* surface, uint32_t layer);
    ~LayerSurface() override;
    void configure();
private:
    friend class LayerShell;
    LayerShell* shell_;
    wl_resource* resource_;
    QPointer<QWaylandSurface> surface_;
    QWaylandQuickItem* item_;
    uint32_t layer_;
    QSize desired_{0, 0};
    QSize configured_;
    uint32_t anchor_ = 0;
    uint32_t keyboard_ = 0;
    int zone_ = 0;
    QMargins margins_;
    uint32_t serial_ = 0;
    bool acknowledged_ = false;
    static LayerSurface* get(wl_resource* resource);
    static void setLayer(wl_client*, wl_resource*, uint32_t);
    static void setSize(wl_client*, wl_resource*, uint32_t, uint32_t);
    static void setAnchor(wl_client*, wl_resource*, uint32_t);
    static void setZone(wl_client*, wl_resource*, int32_t);
    static void setMargin(wl_client*, wl_resource*, int32_t, int32_t, int32_t, int32_t);
    static void setKeyboard(wl_client*, wl_resource*, uint32_t);
    static void getPopup(wl_client*, wl_resource*, wl_resource*);
    static void acknowledge(wl_client*, wl_resource*, uint32_t);
    static void destroy(wl_client*, wl_resource*);
    static void resourceDestroyed(wl_resource*);
};
}
