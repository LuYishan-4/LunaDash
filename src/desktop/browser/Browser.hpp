#pragma once
#include <QStringList>
#include <QUrl>
namespace LunaDash {
class Browser final {
public:
  static QStringList defaultCommand();
  static QStringList commandForUrl(const QUrl &url, QString *error = nullptr);
};
} // namespace LunaDash
