#include <LuDash/window_rules/WindowRules.h>

namespace LuDash {

InitialWindowPolicy initialWindowPolicy(const QString &appId,
                                        const QString &title) {
  const bool kitty = appId.compare("kitty", Qt::CaseInsensitive) == 0 ||
                     appId.startsWith("kitty.", Qt::CaseInsensitive);
  const bool lunaDahTerminal =
      title.contains("LunaDah Terminal", Qt::CaseInsensitive);
  return {.maximized = kitty || lunaDahTerminal};
}

} // namespace LuDash
