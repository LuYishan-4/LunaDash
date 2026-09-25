#pragma once
#include <QJsonArray>
#include <QString>
namespace LunaDash {
QJsonArray appearancePresets();
bool saveAppearancePreset(const QString &name, QString *error);
bool applyAppearancePreset(const QString &name, QString *error);
bool deleteAppearancePreset(const QString &name, QString *error);
} // namespace LunaDash
