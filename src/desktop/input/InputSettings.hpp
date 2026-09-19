#pragma once

#include <QJsonObject>
#include <QString>

namespace LunaDash {

QString keyboardLayoutPreference(const QJsonObject &preferences);
int keyboardRepeatRate();
int keyboardRepeatDelay();

} // namespace LunaDash
