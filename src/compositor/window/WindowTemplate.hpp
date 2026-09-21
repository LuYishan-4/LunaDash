#pragma once

#include "compositor/layout/WindowLayout.hpp"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <memory>

namespace LunaDash {

enum class WindowPointerTemplate { Tiling, Freeform };
enum class WindowActivationTemplate { ToggleMaximize, FocusOnly };
using WindowLayoutFactory = std::unique_ptr<WindowLayout> (*)();

struct WindowTemplate {
  QString key;
  WindowLayoutFactory createLayout = nullptr;
  WindowPointerTemplate pointer = WindowPointerTemplate::Tiling;
  WindowActivationTemplate activation = WindowActivationTemplate::ToggleMaximize;
  bool clientMoveResize = false;
  bool allowOverlap = false;
  QJsonObject layoutSettingsSchema;
  QJsonObject layoutSettingsDefaults;
  QJsonObject animation;
};

QList<WindowTemplate> windowTemplates();
const WindowTemplate &windowTemplateForKey(const QString &key);
std::unique_ptr<WindowLayout>
createWindowLayout(const WindowTemplate &windowTemplate);
bool validateWindowLayoutSettings(const WindowTemplate &windowTemplate,
                                  const QJsonObject &settings,
                                  QString *error = nullptr);

} // namespace LunaDash
