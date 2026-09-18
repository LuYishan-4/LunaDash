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

inline void ensureWaylandChromiumFlags(QStringList &command) {
  if (!command.contains("--ozone-platform=wayland"))
    command.append("--ozone-platform=wayland");
  if (!command.contains("--enable-features=UseOzonePlatform"))
    command.append("--enable-features=UseOzonePlatform");
}

} // namespace LuDash
