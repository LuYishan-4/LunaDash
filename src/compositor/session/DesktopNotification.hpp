#pragma once

#include <string>

namespace LunaDash {

// Shows a desktop notification through org.freedesktop.Notifications on the session bus
// (the shell's notification daemon). Asynchronous and best effort: when no daemon runs the
// notification is dropped. `summary` and `body` are shown as supplied by the caller.
void sendDesktopNotification(const std::string &summary, const std::string &body,
                             const std::string &icon);

} // namespace LunaDash
