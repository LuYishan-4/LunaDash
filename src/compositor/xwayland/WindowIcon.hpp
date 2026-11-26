#pragma once

#include <string>

struct wlr_xwayland_surface;

namespace LunaDash {

bool writeXWaylandIcon(wlr_xwayland_surface *surface, const std::string &path);

} // namespace LunaDash
