#pragma once

#include "compositor/layout/WindowLayout.hpp"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <memory>

namespace LunaDash {

using WindowLayoutFactory = std::unique_ptr<WindowLayout> (*)();

struct WindowTemplate {
  QString key;
  WindowLayoutFactory createLayout = nullptr;
  bool clientMoveResize = false;
  bool allowOverlap = false;
  bool activationTogglesMaximize = false;
  QJsonObject layoutSettingsSchema;
  QJsonObject layoutSettingsDefaults;
  QJsonObject layoutActions;
  QJsonObject animation;
};

QList<WindowTemplate> windowTemplates();
const WindowTemplate &windowTemplateForKey(const QString &key);
std::unique_ptr<WindowLayout>
createWindowLayout(const WindowTemplate &windowTemplate);
bool validateWindowLayoutSettings(const WindowTemplate &windowTemplate,
                                  const QJsonObject &settings,
                                  QString *error = nullptr);
bool windowTemplateSupportsAction(const WindowTemplate &windowTemplate,
                                  const QString &action);
bool performWindowLayoutAction(WindowLayout &layout,
                               const WindowTemplate &windowTemplate,
                               const QString &action,
                               const QJsonObject &payload = {});

} // namespace LunaDash
