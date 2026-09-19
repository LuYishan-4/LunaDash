#include "desktop/browser/Browser.hpp"
#include "desktop/app/DefaultApplications.hpp"
#include <QStandardPaths>

namespace LunaDash {
QStringList Browser::defaultCommand() {
  for (const auto &candidate : {"google-chrome-stable", "google-chrome",
                                "chromium", "chromium-browser"}) {
    if (!QStandardPaths::findExecutable(candidate).isEmpty())
      return {candidate};
  }
  return {};
}

QStringList Browser::commandForUrl(const QUrl &url, QString *error) {
  if (error)
    error->clear();
  if (!url.isValid() || (url.scheme() != "https" && url.scheme() != "http")) {
    if (error)
      *error = "Only valid HTTP and HTTPS URLs can be opened.";
    return {};
  }
  auto command = defaultApplicationCommand("browser", error);
  if (command.isEmpty())
    return {};
  command << url.toString(QUrl::FullyEncoded);
  return command;
}
} // namespace LunaDash
