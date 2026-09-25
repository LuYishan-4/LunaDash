#pragma once

#include "compositor/layout/WindowLayout.hpp"
#include "compositor/window/WindowTemplate.hpp"

#include <QString>

namespace LunaDash {

struct ClientWindow;

QString windowIconName(const QString &appId, const QString &title);
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
