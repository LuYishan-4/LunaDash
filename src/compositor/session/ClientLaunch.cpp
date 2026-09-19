#include "compositor/session/ClientLaunch.hpp"
#include <QFileInfo>
#include <algorithm>

namespace LunaDash {
bool isChromiumApplication(const QString &program) {
  const QString name = QFileInfo(program).fileName().toLower();
  return name == "chrome" || name == "google-chrome" ||
         name == "google-chrome-stable" || name == "chromium" ||
         name == "chromium-browser" || name == "brave" ||
         name == "brave-browser" || name == "vivaldi" || name == "opera";
}

bool isDiscordApplicationCommand(const QStringList &command) {
  return std::any_of(
      command.cbegin(), command.cend(), [](const QString &argument) {
        const QString lower = argument.toLower();
        const QString name = QFileInfo(argument).fileName().toLower();
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
}

void ensureDiscordWaylandFlags(QStringList &command) {
  QStringList disabled;
  int featureIndex = -1;
  for (auto it = command.begin(); it != command.end();) {
    if (it->startsWith("--disable-features=")) {
      if (featureIndex < 0)
        featureIndex = static_cast<int>(it - command.begin());
      disabled.append(it->mid(QStringLiteral("--disable-features=").size())
                          .split(',', Qt::SkipEmptyParts));
      it = command.erase(it);
    } else {
      if (*it == "--use-angle=vulkan")
        *it = "--use-angle=gl";
      ++it;
    }
  }
  disabled.append({"Vulkan", "DefaultANGLEVulkan", "VulkanFromANGLE"});
  disabled.removeDuplicates();
  command.insert(featureIndex < 0 ? command.size() : featureIndex,
                 "--disable-features=" + disabled.join(','));
  if (std::none_of(command.cbegin(), command.cend(), [](const auto &arg) {
        return arg.startsWith("--use-angle=");
      }))
    command.append("--use-angle=gl");
  if (!command.contains("--enable-wayland-ime"))
    command.append("--enable-wayland-ime");
  if (std::none_of(command.cbegin(), command.cend(), [](const auto &arg) {
        return arg.startsWith("--wayland-text-input-version=");
      }))
    command.append("--wayland-text-input-version=3");
  // Feature switches alone did not prevent Discord's GPU process from
  // crashing on the reported NVIDIA/Wayland session. Keep this fallback
  // local to Discord; users can explicitly opt back into GPU rendering.
  if (qEnvironmentVariable("LUNADASH_DISCORD_GPU") != "1" &&
      !command.contains("--disable-gpu"))
    command.append("--disable-gpu");
}

} // namespace LunaDash
