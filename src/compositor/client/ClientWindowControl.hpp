#pragma once

#include "core/geometry/Geometry.hpp"

#include <string>

namespace LunaDash {

struct ClientWindow;

// Protocol-level operations shared by xdg-shell and XWayland windows. Callers
// outside this file should not branch on ClientWindow::x11 to reach wlroots.

// True once the client's surface can receive configure and focus requests.
bool clientReady(const ClientWindow *client);
void setClientActivated(ClientWindow *client, bool active);
void closeClient(ClientWindow *client);
void setClientMinimizedHint(ClientWindow *client, bool minimized);
void setClientMaximizedHint(ClientWindow *client, bool maximized);
void setClientFullscreenHint(ClientWindow *client, bool fullscreen);
// Sends the size and state of `rectangle` to the client; scene placement is the
// caller's job. Requires clientReady(client).
void configureClientSurface(ClientWindow *client, const Rect &rectangle);
// The size a client asked for in its first commit, used as its floating size.
Size clientCommittedSize(const ClientWindow &client);
// Copies title, app id, pid, utility and dialog floating state from the
// protocol object. Returns false when the client has no protocol object yet.
bool readClientProtocolMetadata(ClientWindow *client);
// Helper surfaces (clipboard bridges) that never appear as windows.
bool isUtilityWindow(const std::string &appId);

} // namespace LunaDash
