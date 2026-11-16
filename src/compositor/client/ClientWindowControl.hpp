#pragma once

#include <string>

namespace LunaDash {

struct ClientWindow;

// Protocol-level operations shared by xdg-shell and XWayland windows.

// True once the client's surface can receive configure and focus requests.
bool clientReady(const ClientWindow *client);
void setClientActivated(ClientWindow *client, bool active);
void closeClient(ClientWindow *client);
void setClientMinimizedHint(ClientWindow *client, bool minimized);
// Helper surfaces (clipboard bridges) that never appear as windows.
bool isUtilityWindow(const std::string &appId);

} // namespace LunaDash
