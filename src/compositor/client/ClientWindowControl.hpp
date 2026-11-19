#pragma once

#include "core/geometry/Geometry.hpp"

#include <cstddef>
#include <string>

namespace LunaDash {

struct ClientWindow;

bool clientReady(const ClientWindow *client);
bool clientIsTransientOf(const ClientWindow *client,
                         const ClientWindow *ancestor, std::size_t maxDepth);
void setClientActivated(ClientWindow *client, bool active);
void closeClient(ClientWindow *client);
void setClientMinimizedHint(ClientWindow *client, bool minimized);
void setClientMaximizedHint(ClientWindow *client, bool maximized);
void setClientFullscreenHint(ClientWindow *client, bool fullscreen);

void configureClientSurface(ClientWindow *client, const Rect &rectangle);

double clientContentScale(const ClientWindow &client);

Size clientLogicalSize(const ClientWindow &client, const Size &visual);

void applyClientContentScale(ClientWindow &client);

Size clientCommittedSize(const ClientWindow &client);

bool readClientProtocolMetadata(ClientWindow *client);

bool isUtilityWindow(const std::string &appId);

} // namespace LunaDash
