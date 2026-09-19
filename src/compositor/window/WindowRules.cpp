#include "compositor/window/WindowRules.hpp"

namespace LunaDash {

InitialWindowPolicy initialWindowPolicy(const QString &appId,
                                        const QString &title) {
  InitialWindowPolicy policy;

  // Keep LunaDash's built-in rules aligned with niri's default configuration:
  // regular windows open tiled and non-maximized. Firefox picture-in-picture
  // is the default special case and opens floating.
  const QString normalizedAppId = appId.toLower();
  if (normalizedAppId.endsWith(QStringLiteral("firefox")) &&
      title == QStringLiteral("Picture-in-Picture")) {
    policy.floating = true;
  }

  return policy;
}

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
