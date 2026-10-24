#include "compositor/session/LaunchPolicy.hpp"

#include "support/TestHarness.hpp"

using namespace LunaDash;

class LaunchPolicyTests final {
public:
  void desktopIdentityRequestsX11Helper() {
    const auto identity = identifyLaunch(
        "com.discordapp.Discord.desktop",
        {"flatpak", "run", "com.discordapp.Discord"});
    VERIFY(identity.desktopId == "com.discordapp.Discord");
    VERIFY(identity.flatpakId == "com.discordapp.Discord");
    VERIFY(resolveLaunchCapabilities(
                "com.discordapp.Discord.desktop",
                {"flatpak", "run", "com.discordapp.Discord"})
                .x11Helper);
  }

  void flatpakIdentityWorksWithoutDesktopMetadata() {
    VERIFY(resolveLaunchCapabilities(
                {}, {"flatpak", "run", "--branch=stable",
                     "com.discordapp.Discord"})
                .x11Helper);
  }

  void arbitraryArgumentsDoNotImpersonateApplications() {
    VERIFY(!resolveLaunchCapabilities(
                 "org.example.Viewer.desktop",
                 {"viewer", "https://example.test/com.discordapp.Discord"})
                 .x11Helper);
  }

  void unknownApplicationStaysWaylandOnly() {
    VERIFY(!resolveLaunchCapabilities(
                 "org.example.Native.desktop",
                 {"native-app", "--ozone-platform=wayland"})
                 .x11Helper);
  }
};

int main(int argc, char **argv) {
  return LunaDash::Test::run<LaunchPolicyTests>(
      "LaunchPolicyTests",
      {
      {"desktopIdentityRequestsX11Helper", &LaunchPolicyTests::desktopIdentityRequestsX11Helper},
      {"flatpakIdentityWorksWithoutDesktopMetadata", &LaunchPolicyTests::flatpakIdentityWorksWithoutDesktopMetadata},
      {"arbitraryArgumentsDoNotImpersonateApplications", &LaunchPolicyTests::arbitraryArgumentsDoNotImpersonateApplications},
      {"unknownApplicationStaysWaylandOnly", &LaunchPolicyTests::unknownApplicationStaysWaylandOnly}},
      argc, argv);
}
