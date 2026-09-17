// LuDashUtils.h
#pragma once

#include <QJsonValue>
#include <QString>
#include <QStringList>

#include <optional>

namespace LuDash::Utils {

std::optional<int> jsonWindowId(const QJsonValue &value);
std::optional<int> textWindowId(const QString &value);

bool groupWindowIds(const QString &value, int *window, int *target);

QString clipboardBridgePath();

bool isUtilityWindow(const QString &appId);

bool isChromiumApplication(const QString &program);
void ensureWaylandChromiumFlags(QStringList &command);

} // namespace LuDash::Utils
