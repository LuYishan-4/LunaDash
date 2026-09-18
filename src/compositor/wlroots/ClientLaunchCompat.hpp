#pragma once

#include <QFileInfo>
#include <QStringList>
#include <algorithm>

namespace LuDash {

inline bool isChromiumApplication(const QString &program) {
  const QString name = QFileInfo(program).fileName().toLower();
  return name == "chrome" || name == "google-chrome" ||
         name == "google-chrome-stable" || name == "chromium" ||
         name == "chromium-browser" || name == "brave" ||
         name == "brave-browser" || name == "vivaldi" || name == "opera";
}

inline bool isDiscordApplicationCommand(const QStringList &command) {
  return std::any_of(command.cbegin(), command.cend(),
                     [](const QString &argument) {
                       const QString lower = argument.toLower();
                       const QString name =
                           QFileInfo(argument).fileName().toLower();
                       return name == "discord" || name == "discordcanary" ||
                              name == "discord-ptb" ||
                              lower.contains("com.discordapp.discord") ||
                              lower.contains("com.discordapp.discordcanary") ||
                              lower.contains("com.discordapp.discordptb");
                     });
}

inline bool isChromiumApplicationCommand(const QStringList &command) {
  return std::any_of(command.cbegin(), command.cend(),
                     [](const QString &argument) {
                       return isChromiumApplication(argument);
                     }) ||
         isDiscordApplicationCommand(command);
}

inline void ensureWaylandChromiumFlags(QStringList &command) {
  if (!command.contains("--ozone-platform=wayland"))
    command.append("--ozone-platform=wayland");
  if (!command.contains("--enable-features=UseOzonePlatform"))
    command.append("--enable-features=UseOzonePlatform");
}

} // namespace LuDash
