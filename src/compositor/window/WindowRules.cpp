#include "compositor/window/WindowRules.hpp"

namespace LunaDash {

QString windowIconName(const QString &appId, const QString &title) {
  // External titles are document/tab names, not application identities.
  // Only our own shared executable uses its known window titles as aliases.
  if (appId == "lunadash-app") {
    const auto name = title.toLower();
    if (name.contains("terminal") || name.contains("console"))
      return "utilities-terminal";
    if (name.contains("file"))
      return "system-file-manager";
    if (name.contains("setting"))
      return "preferences-system";
    if (name.contains("monitor"))
      return "utilities-system-monitor";
    return "lunadash";
  }
  if (appId == "org.freedesktop.Xwayland")
    return "application-x-executable";
  return appId.isEmpty() ? QStringLiteral("application-x-executable") : appId;
}

} // namespace LunaDash
