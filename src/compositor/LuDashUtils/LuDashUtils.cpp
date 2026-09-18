#include "compositor/LuDashUtils/LuDashUtils.hpp"
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStandardPaths>
#include <algorithm>
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

bool isDiscordApplicationCommand(const QStringList &command) {
  return std::any_of(command.cbegin(), command.cend(), [](const QString &argument) {
    const auto lower = argument.toLower();
    const auto name = QFileInfo(argument).fileName().toLower();
    return name == "discord" || name == "discordcanary" ||
           name == "discord-ptb" ||
           lower.contains("com.discordapp.discord") ||
           lower.contains("com.discordapp.discordcanary") ||
           lower.contains("com.discordapp.discordptb");
  });
}

bool isChromiumApplicationCommand(const QStringList &command) {
  return std::any_of(command.cbegin(), command.cend(),
                     [](const QString &argument) {
                       return isChromiumApplication(argument);
                     }) ||
         isDiscordApplicationCommand(command);
}

void ensureWaylandChromiumFlags(QStringList &command) {
  if (!command.contains("--ozone-platform=wayland"))
    command.append("--ozone-platform=wayland");

  if (!command.contains("--enable-features=UseOzonePlatform"))
    command.append("--enable-features=UseOzonePlatform");

  // LunaDash does not yet provide a compositor-side input-method-v2 bridge.
  // Chromium's native text-input path therefore cannot reach Fcitx reliably.
  // Chromium (unlike Electron) can use its GTK4 frontend under Wayland, which
  // talks to the configured GTK_IM_MODULE=fcitx directly. Respect an explicit
  // Wayland-IME choice supplied by the desktop entry/user instead of combining
  // the two mutually exclusive input paths.
  const bool explicitWaylandIme =
      command.contains("--enable-wayland-ime") ||
      std::any_of(command.cbegin(), command.cend(), [](const QString &argument) {
        return argument.startsWith("--wayland-text-input-version=");
      });
  const bool explicitGtkVersion =
      std::any_of(command.cbegin(), command.cend(), [](const QString &argument) {
        return argument.startsWith("--gtk-version=");
      });
  if (!explicitWaylandIme && !explicitGtkVersion)
    command.append("--gtk-version=4");
}


void prepareXWaylandChromiumFlags(QStringList &command) {
  // Until LunaDash has a complete text-input-v3 <-> input-method bridge and
  // modern pointer/popup protocol coverage, Chromium is more reliable through
  // XWayland. Remove Wayland-only switches inherited from desktop entries and
  // select Ozone X11 explicitly; XMODIFIERS/GTK_IM_MODULE=fcitx remain in the
  // XWayland environment.
  command.erase(std::remove_if(command.begin(), command.end(),
                               [](const QString &argument) {
                                 return argument == "--enable-wayland-ime" ||
                                        argument.startsWith("--wayland-text-input-version=") ||
                                        argument.startsWith("--gtk-version=") ||
                                        argument.startsWith("--ozone-platform=");
                               }),
                command.end());
  command.append("--ozone-platform=x11");
}

} // namespace LuDash::Utils
