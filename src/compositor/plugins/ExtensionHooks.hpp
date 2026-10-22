#pragma once
#include "compositor/layout/WindowLayout.hpp"
#include <QJsonObject>
namespace LunaDash {
class PluginManager;
struct WindowTemplate;
QJsonObject windowAnimationProfile(PluginManager &plugins,
                                   const QJsonObject &preferences,
                                   const WindowTemplate &windowTemplate);
int shellAnimationDuration(PluginManager &plugins, int fallback);
QJsonObject initialWindowRule(PluginManager &plugins,
                              const QJsonObject &context, int workspace,
                              bool maximized, int workspaceCount);
QList<WindowPlacement>
pluginWindowPlacements(PluginManager &plugins,
                       const WindowTemplate &windowTemplate,
                       LayoutWorkspaceId workspace, QRect area,
                       const QList<WindowPlacement> &placements);
} // namespace LunaDash
