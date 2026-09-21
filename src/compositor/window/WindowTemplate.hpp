#pragma once

#include "compositor/layout/WindowLayout.hpp"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <memory>

namespace LunaDash {

enum class WindowPointerTemplate { Tiling, Freeform };
enum class WindowActivationTemplate { ToggleMaximize, FocusOnly };

struct WindowTemplate {
  QString key;
  WindowLayoutMode layoutMode = WindowLayoutMode::Tiling;
  WindowPointerTemplate pointer = WindowPointerTemplate::Tiling;
  WindowActivationTemplate activation = WindowActivationTemplate::ToggleMaximize;
  bool clientMoveResize = false;
  QJsonObject animation;
};

QList<WindowTemplate> windowTemplates();
const WindowTemplate &windowTemplateForKey(const QString &key);
std::unique_ptr<WindowLayout>
createWindowLayout(const WindowTemplate &windowTemplate);

} // namespace LunaDash
