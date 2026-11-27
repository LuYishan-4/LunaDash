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

void applyClientContentScale(ClientWindow &client);

Size clientCommittedSize(const ClientWindow &client);
// The smallest size the client accepts (0 where it sets none).
Size clientMinimumSize(const ClientWindow &client);

bool readClientProtocolMetadata(ClientWindow *client);

bool isUtilityWindow(const std::string &appId);

} // namespace LunaDash
