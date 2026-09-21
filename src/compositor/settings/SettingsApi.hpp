#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <functional>
namespace LunaDash {
class PluginManager;
class ShellModules;
// Native effects and visual plugins expose the same settings contract.
QJsonArray settingsApiTargets(const QJsonObject &extensions,
                              const QJsonObject &modules,
                              const QJsonObject &layout);
bool updateSettingsApi(PluginManager &plugins, ShellModules &modules,
                       const QJsonObject &layout, const QJsonObject &request,
                       const std::function<bool(const QJsonObject &, QString *)> &setLayout,
                       QString *error);
}
