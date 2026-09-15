#include <LuDash/window_rules/WindowRules.h>

namespace LuDash {

InitialWindowPolicy initialWindowPolicy(const QString &appId,
                                        const QString &title) {
  Q_UNUSED(appId);
  Q_UNUSED(title);
  // Every window opens as its own full-width column. Column width already
  // equals the work area, so no separate maximized overlay is needed.
  return {.maximized = false};
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
