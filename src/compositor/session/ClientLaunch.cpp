#include "compositor/session/ClientLaunch.hpp"
#include <QFileInfo>
#include <QSet>
#include <algorithm>

namespace LunaDash {
namespace {
bool chromiumLikeName(const QString &name) {
  static const QSet<QString> names = {
      "chrome",          "google-chrome",  "google-chrome-stable",
      "chromium",        "chromium-browser", "brave",
      "brave-browser",   "vivaldi",        "opera",
      "discord",         "discordcanary",  "discord-ptb",
      "codex",           "codex-desktop",  "openai-codex",
      "chatgpt",         "cursor",         "code",
      "code-insiders",   "codium",         "vscodium",
      "slack",           "obsidian",       "teams-for-linux"};
  return names.contains(name) || name.startsWith("electron");
}

bool chromiumLikeId(const QString &argument) {
  const QString lower = argument.toLower();
  static const QStringList fragments = {
      "com.google.chrome",       "org.chromium.chromium",
      "com.brave.browser",       "com.vivaldi.vivaldi",
      "com.discordapp.discord",  "com.openai.codex",
      "com.openai.chatgpt",      "com.visualstudio.code",
      "com.vscodium.codium",     "com.getcursor.cursor",
      "com.slack.slack",         "md.obsidian.obsidian"};
  return std::any_of(fragments.cbegin(), fragments.cend(),
                     [&lower](const QString &fragment) {
                       return lower.contains(fragment);
                     });
}

void mergeCommaFlag(QStringList &command, const QString &prefix,
                    const QStringList &required) {
  QStringList values;
  int insertion = -1;
  for (auto it = command.begin(); it != command.end();) {
    if (it->startsWith(prefix)) {
      if (insertion < 0)
        insertion = static_cast<int>(it - command.begin());
      values.append(it->mid(prefix.size()).split(',', Qt::SkipEmptyParts));
      it = command.erase(it);
    } else {
      ++it;
    }
  }
  values.append(required);
  values.removeDuplicates();
  command.insert(insertion < 0 ? command.size() : insertion,
                 prefix + values.join(','));
}

void preferOpenGlAngle(QStringList &command) {
  for (auto &argument : command)
    if (argument == "--use-angle=vulkan")
      argument = "--use-angle=gl";
  if (std::none_of(command.cbegin(), command.cend(), [](const auto &argument) {
        return argument.startsWith("--use-angle=");
      }))
    command.append("--use-angle=gl");
}

void disableVulkanAngle(QStringList &command) {
  preferOpenGlAngle(command);
  mergeCommaFlag(command, "--disable-features=",
                 {"Vulkan", "DefaultANGLEVulkan", "VulkanFromANGLE"});
}
} // namespace

bool isChromiumApplication(const QString &program) {
  return chromiumLikeName(QFileInfo(program).fileName().toLower());
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
                       return isChromiumApplication(argument) ||
                              chromiumLikeId(argument);
                     }) ||
         isDiscordApplicationCommand(command);
}

void ensureWaylandChromiumFlags(QStringList &command) {
  if (!command.contains("--ozone-platform=wayland"))
    command.append("--ozone-platform=wayland");
  mergeCommaFlag(command, "--enable-features=", {"UseOzonePlatform"});
  if (!command.contains("--enable-wayland-ime"))
    command.append("--enable-wayland-ime");
  if (std::none_of(command.cbegin(), command.cend(), [](const auto &argument) {
        return argument.startsWith("--wayland-text-input-version=");
      }))
    command.append("--wayland-text-input-version=3");

  // Prefer the mature EGL/OpenGL ANGLE path while the native Vulkan path is
  // being validated across wlroots and vendor drivers. The escape hatch keeps
  // driver/compositor testing possible without changing desktop launchers.
  if (qEnvironmentVariable("LUNADASH_CHROMIUM_VULKAN") != "1")
    disableVulkanAngle(command);
}

void ensureDiscordWaylandFlags(QStringList &command) {
  ensureWaylandChromiumFlags(command);
  // Discord's helper/GPU-process combination needs the conservative path even
  // when generic Chromium Vulkan testing is enabled.
  disableVulkanAngle(command);
  if (qEnvironmentVariable("LUNADASH_DISCORD_GPU") != "1" &&
      !command.contains("--disable-gpu"))
    command.append("--disable-gpu");
}

} // namespace LunaDash
