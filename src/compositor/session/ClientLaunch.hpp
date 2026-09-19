#pragma once
#include <QStringList>
namespace LunaDash {
bool isChromiumApplication(const QString &program);
bool isDiscordApplicationCommand(const QStringList &command);
bool isChromiumApplicationCommand(const QStringList &command);
void ensureWaylandChromiumFlags(QStringList &command);
} // namespace LunaDash
