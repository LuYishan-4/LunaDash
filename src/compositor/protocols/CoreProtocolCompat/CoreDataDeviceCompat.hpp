#pragma once

class QWaylandCompositor;

namespace LuDash {

// Replaces QtWaylandCompositor's wl_data_device_manager v1 global with a
// LunaDash-owned v3 implementation while preserving the compositor's public
// QWayland APIs. This is installed immediately after QWaylandCompositor::create
// and before the event loop can accept application requests.
void installCoreDataDeviceV3(QWaylandCompositor *compositor);

} // namespace LuDash
