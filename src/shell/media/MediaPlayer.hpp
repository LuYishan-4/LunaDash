#pragma once

#include <QJsonObject>
#include <QString>

namespace LunaDash {
QJsonObject mediaStatus(const QString &preferredService = {},
                        const QString &preferredBus = {});
QJsonObject mediaAction(const QString &action, const QString &service,
                        const QString &bus, const QString &value = {},
                        const QString &trackId = {});
} // namespace LunaDash
