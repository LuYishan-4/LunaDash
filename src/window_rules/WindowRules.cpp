#include <LuDash/window_rules/WindowRules.h>

namespace LuDash {

InitialWindowPolicy initialWindowPolicy(const QString &appId,
                                        const QString &title) {
  Q_UNUSED(appId);
  Q_UNUSED(title);
  return {.maximized = true};
}

QString windowIconName(const QString &appId, const QString &title) {
  const auto identity = (appId + " " + title).toLower();
  if (identity.contains("kitty") || identity.contains("terminal") ||
      identity.contains("console"))
    return "kitty";
  if (identity.contains("file") || identity.contains("nautilus") ||
      identity.contains("dolphin"))
    return "system-file-manager";
  if (identity.contains("setting") || identity.contains("control-center"))
    return "preferences-system";
  if (identity.contains("monitor"))
    return "utilities-system-monitor";
  if (identity.contains("image-picker"))
    return "image-x-generic";
  if (appId == "lunadah-app")
    return "lunadah";
  return appId.isEmpty() ? QStringLiteral("application-x-executable") : appId;
}

} // namespace LuDash
