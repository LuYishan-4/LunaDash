#include "compositor/session/ClientLaunch.hpp"
#include <QtTest>
#include <algorithm>

namespace LunaDash {
class ClientLaunchTests final : public QObject {
  Q_OBJECT

private slots:
  void recognizesChromiumFamilies() {
    QVERIFY(isChromiumApplication("google-chrome"));
    QVERIFY(isChromiumApplication("/opt/Codex/codex"));
    QVERIFY(isChromiumApplication("/usr/bin/code"));
    QVERIFY(!isChromiumApplication("firefox"));
    QVERIFY(isChromiumApplicationCommand(
        {"flatpak", "run", "com.openai.Codex"}));
    QVERIFY(isChromiumApplicationCommand(
        {"flatpak", "run", "com.discordapp.Discord"}));
  }

  void preparesStableWaylandRendering() {
    qunsetenv("LUNADASH_CHROMIUM_VULKAN");
    QStringList command = {"codex", "--enable-features=Existing",
                           "--disable-features=ExistingDisabled",
                           "--use-angle=vulkan"};
    ensureWaylandChromiumFlags(command);

    QCOMPARE(command.count("--ozone-platform=wayland"), 1);
    QVERIFY(command.contains("--enable-wayland-ime"));
    QVERIFY(command.contains("--wayland-text-input-version=3"));
    QVERIFY(command.contains("--use-angle=gl"));

    const auto enabled = std::find_if(
        command.cbegin(), command.cend(), [](const QString &value) {
          return value.startsWith("--enable-features=");
        });
    QVERIFY(enabled != command.cend());
    QVERIFY(enabled->contains("Existing"));
    QVERIFY(enabled->contains("UseOzonePlatform"));

    const auto disabled = std::find_if(
        command.cbegin(), command.cend(), [](const QString &value) {
          return value.startsWith("--disable-features=");
        });
    QVERIFY(disabled != command.cend());
    QVERIFY(disabled->contains("ExistingDisabled"));
    QVERIFY(disabled->contains("Vulkan"));
    QVERIFY(disabled->contains("DefaultANGLEVulkan"));
    QVERIFY(disabled->contains("VulkanFromANGLE"));
  }

  void discordKeepsSoftwareFallback() {
    qunsetenv("LUNADASH_DISCORD_GPU");
    QStringList command = {"discord"};
    ensureDiscordWaylandFlags(command);
    QVERIFY(command.contains("--disable-gpu"));
    QVERIFY(command.contains("--ozone-platform=wayland"));
  }
};
} // namespace LunaDash

QTEST_GUILESS_MAIN(LunaDash::ClientLaunchTests)
#include "ClientLaunchTests.moc"
