#include "compositor/LuDashUtils/LuDashUtils.hpp"
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStandardPaths>
#include <cmath>
#include <limits>

namespace LuDash::Utils {

std::optional<int> jsonWindowId(const QJsonValue &value) {
  if (!value.isDouble())
    return std::nullopt;

  const double number = value.toDouble();

  if (!std::isfinite(number) || std::floor(number) != number || number < 1 ||
      number > std::numeric_limits<int>::max()) {
    return std::nullopt;
  }

  return static_cast<int>(number);
}

std::optional<int> textWindowId(const QString &value) {
  if (value.isEmpty() || value.size() > 10 || value.front() == '0')
    return std::nullopt;

  qint64 result = 0;

  for (const QChar character : value) {
    if (character < '0' || character > '9')
      return std::nullopt;

    result = result * 10 + character.digitValue();

    if (result > std::numeric_limits<int>::max())
      return std::nullopt;
  }

  return static_cast<int>(result);
}

bool groupWindowIds(const QString &value, int *window, int *target) {
  if (value.size() > 256 || value.toUtf8().size() > 256)
    return false;

  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(value.toUtf8(), &parseError);

  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    return false;
  }

  const auto object = document.object();

  if (object.size() != 2 || !object.contains("window") ||
      !object.contains("target")) {
    return false;
  }

  const auto parsedWindow = jsonWindowId(object.value("window"));
  const auto parsedTarget = jsonWindowId(object.value("target"));

  if (!parsedWindow || !parsedTarget || *parsedWindow == *parsedTarget) {
    return false;
  }

  *window = *parsedWindow;
  *target = *parsedTarget;
  return true;
}

QString clipboardBridgePath() {
  const QString beside = QCoreApplication::applicationDirPath() +
                         QStringLiteral("/lunadash-clipboard-bridge");

  if (QFileInfo::exists(beside))
    return beside;

  const QString installed = QStandardPaths::findExecutable(
      QStringLiteral("lunadash-clipboard-bridge"));

  if (!installed.isEmpty())
    return installed;

  const QString source = QStringLiteral(LUDASH_SCRIPT_SOURCE_DIR) +
                         QStringLiteral("/lunadash-clipboard-bridge");

  return QFileInfo::exists(source) ? source : QString();
}

bool isUtilityWindow(const QString &appId) {
  return appId == QLatin1String("io.github.bugaevc.wl-clipboard") ||
         appId == QLatin1String("org.freedesktop.Xwayland");
}

bool isChromiumApplication(const QString &program) {
  const auto name = QFileInfo(program).fileName().toLower();

  return name == "chrome" || name == "google-chrome" ||
         name == "google-chrome-stable" || name == "chromium" ||
         name == "chromium-browser" || name == "microsoft-edge" ||
         name == "brave" || name == "vivaldi" || name == "opera";
}

void ensureWaylandChromiumFlags(QStringList &command) {
  if (!command.contains("--ozone-platform=wayland"))
    command.append("--ozone-platform=wayland");

  if (!command.contains("--enable-features=UseOzonePlatform"))
    command.append("--enable-features=UseOzonePlatform");
}

} // namespace LuDash::Utils
