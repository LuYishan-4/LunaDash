#include "compositor/WindowRules/WindowRules.hpp"

namespace LuDash {

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
  const auto identity = (appId + " " + title).toLower();
  if (identity.contains("terminal") || identity.contains("console"))
    return "utilities-terminal";
  if (identity.contains("file") || identity.contains("nautilus") ||
      identity.contains("dolphin"))
    return "system-file-manager";
  if (identity.contains("setting") || identity.contains("control-center"))
    return "preferences-system";
  if (identity.contains("monitor"))
    return "utilities-system-monitor";
  if (appId == "lunadash-app")
    return "lunadash";
  if (appId == "org.freedesktop.Xwayland")
    return "application-x-executable";
  return appId.isEmpty() ? QStringLiteral("application-x-executable") : appId;
}

} // namespace LuDash
