#pragma once

#include <QString>

namespace LuDash {

struct InitialWindowPolicy {
  bool maximized = false;
};

InitialWindowPolicy initialWindowPolicy(const QString &appId,
                                        const QString &title);
QString windowIconName(const QString &appId, const QString &title);

} // namespace LuDash
