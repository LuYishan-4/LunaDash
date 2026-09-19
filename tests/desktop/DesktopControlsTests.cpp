#include "DdcBrightnessTests.hpp"
#include "compositor/session/ClientLaunch.hpp"
#include "compositor/session/SessionEnvironment.hpp"
#include "compositor/window/WindowRules.hpp"
#include "desktop/display/BrightnessSettings.hpp"
#include "desktop/shortcuts/ShortcutSettings.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <cstdio>

namespace LunaDash {
void check(bool condition, const char *message) {
  if (!condition)
    qFatal("%s", message);
}
void runTests() {
  for (const auto &title : {"Files", "Terminal settings", "Monitor console"}) {
    check(windowIconName("org.mozilla.firefox", title) == "org.mozilla.firefox",
          "Document titles must not change external application identity");
    check(windowIconName("", title) == "application-x-executable",
          "Unknown application IDs must not borrow another app's icon");
  }
  check(windowIconName("lunadash-app", "LunaDash Console") ==
            "utilities-terminal",
        "Built-in application aliases are retained");
  qunsetenv("LUNADASH_DISCORD_GPU");
  QStringList discord{"flatpak", "run", "com.discordapp.Discord",
                      "--disable-features=ExistingFeature",
                      "--use-angle=vulkan"};
  check(isDiscordApplicationCommand(discord), "Recognize Flatpak Discord");
  ensureDiscordWaylandFlags(discord);
  check(discord.contains("--use-angle=gl") &&
            discord.contains("--disable-features=ExistingFeature,Vulkan,"
                             "DefaultANGLEVulkan,VulkanFromANGLE"),
        "Keep existing flags while disabling the incompatible Vulkan paths");
  check(discord.contains("--disable-gpu"), "Discord uses the scoped software fallback");
  const auto once = discord;
  ensureDiscordWaylandFlags(discord);
  check(discord == once, "Discord compatibility flags are idempotent");
  testDdcBrightness();
  check(parseBacklight("intel_backlight,backlight,12000,60%,20000\n")
                .value("percent")
                .toInt() == 60,
        "Read percentage field, not maximum");
  check(!parseBacklight("kbd_backlight,leds,1,33%,3\n")
             .value("available")
             .toBool(),
        "Ignore keyboard LEDs");
  check(!parseBacklight("broken,backlight,0,150%,10\n")
             .value("available")
             .toBool(),
        "Reject invalid percentages");
  check(parseBacklight("invalid\namdgpu_bl1,backlight,0,0%,255\n")
            .value("available")
            .toBool(),
        "Accept a zero-brightness device");
  QTemporaryDir config;
  check(config.isValid(), "Temporary config directory");
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
  ShortcutSettings initial;
  check(initial.actionFor(XKB_KEY_s, ShortcutMeta | ShortcutShift) ==
            "screenshot",
        "New screenshot shortcut");
  check(initial.actionFor(XKB_KEY_S, ShortcutMeta | ShortcutShift) ==
            "screenshot",
        "Shift-generated uppercase keysyms trigger screenshot");
  auto saved = initial.snapshot();
  saved["screenshot"] = "Alt+Shift+F5";
  QSettings().setValue("shortcuts/bindings", saved.toVariantMap());
  ShortcutSettings migrated;
  check(migrated.snapshot().value("screenshot") == "Meta+Shift+S",
        "Migrate old screenshot default");
  saved["screenshot"] = "Disabled";
  QSettings().setValue("shortcuts/bindings", saved.toVariantMap());
  ShortcutSettings disabled;
  check(disabled.snapshot().value("screenshot") == "Disabled",
        "Preserve disabled screenshot action");
  saved["screenshot"] = "Alt+Shift+F5";
  saved["launchTerminal"] = "Meta+Shift+S";
  QSettings().setValue("shortcuts/bindings", saved.toVariantMap());
  ShortcutSettings conflict;
  check(conflict.snapshot().value("launchTerminal") == "Meta+Shift+S",
        "Do not steal custom shortcuts");

  QTemporaryDir tools;
  QFile publisher(tools.filePath("dbus-update-activation-environment"));
  check(publisher.open(QIODevice::WriteOnly), "Create activation fixture");
  publisher.write("#!/bin/sh\nsleep 0.2\ncase \"$1\" in --systemd) exit 1;; "
                  "esac\nexit 0\n");
  publisher.close();
  publisher.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                           QFile::ExeOwner);
  const QByteArray oldPath = qgetenv("PATH");
  qputenv("PATH", tools.path().toUtf8() + ':' + oldPath);
  bool done = false, success = false;
  int turns = 0;
  QTimer heartbeat;
  QObject::connect(&heartbeat, &QTimer::timeout, [&] { ++turns; });
  heartbeat.start(10);
  QElapsedTimer elapsed;
  elapsed.start();
  publishActivationEnvironment(QProcessEnvironment::systemEnvironment(),
                               QCoreApplication::instance(),
                               [&](bool ok, const QString &) {
                                 success = ok;
                                 done = true;
                               });
  check(elapsed.elapsed() < 100,
        "Activation publication must return immediately");
  while (!done && elapsed.elapsed() < 4000) {
    QCoreApplication::processEvents();
    QThread::msleep(1);
  }
  check(done && success && turns > 20,
        "Event loop remains responsive during activation fallback");
  qputenv("PATH", oldPath);
  std::puts("Brightness parsing, shortcut migration and asynchronous startup "
            "passed.");
}
} // namespace LunaDash
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  app.setApplicationName("LunaDashControlsTest");
  app.setOrganizationName("LunaDashControlsTest");
  LunaDash::runTests();
}
