#pragma once

#include <QSize>
#include <QString>

namespace LunaDash {

struct InitialWindowPolicy {
  // Match niri's opening semantics: ordinary windows start tiled at the
  // configured default column width and are not maximized implicitly.
  bool maximized = false;
  bool floating = false;
  QSize floatingSize;
};

InitialWindowPolicy initialWindowPolicy(const QString &appId,
                                        const QString &title);
QString windowIconName(const QString &appId, const QString &title);

} // namespace LunaDash
