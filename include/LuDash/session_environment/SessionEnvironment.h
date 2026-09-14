#pragma once

#include <QProcessEnvironment>
#include <QString>

namespace LuDash {
QProcessEnvironment createClientEnvironment(const QString &socketName,
                                            const QString &controlPath,
                                            const QString &binaryDirectory);
bool publishClientEnvironment(const QProcessEnvironment &environment);
} // namespace LuDash
