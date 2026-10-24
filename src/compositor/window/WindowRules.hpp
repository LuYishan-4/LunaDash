#pragma once

#include "compositor/layout/WindowLayout.hpp"
#include "compositor/window/WindowTemplate.hpp"


namespace LunaDash {

struct ClientWindow;

std::string windowIconName(const std::string &appId, const std::string &title);
bool windowUsesManagedLayout(const ClientWindow &client);
bool windowAllowsPointerInteraction(const WindowTemplate &windowTemplate,
                                    const ClientWindow &client);
bool windowAllowsClientMoveResize(const WindowTemplate &windowTemplate,
                                  const ClientWindow &client);
bool windowActivationTogglesMaximize(const WindowTemplate &windowTemplate,
                                     const ClientWindow &client);
bool windowHiddenByMaximize(const WindowTemplate &windowTemplate,
                            LayoutWindowId maximized, bool inMaximizedFamily,
                            const ClientWindow &client);

} // namespace LunaDash
