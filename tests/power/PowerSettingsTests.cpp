#include "desktop/power/PowerSettings.hpp"

#include <cassert>

int main() {
  const auto profiles = LunaDash::parsePowerProfiles("  performance:\n"
                                                     "* balanced:\n"
                                                     "  power-saver:\n"
                                                     "  custom-profile:\n");

  assert(profiles.at("available") == true);
  assert(profiles.at("profiles").size() == 3);
  assert(profiles.at("profiles").at(0) == "performance");
  assert(profiles.at("profiles").at(1) == "balanced");
  assert(profiles.at("profiles").at(2) == "power-saver");
  assert(profiles.at("current") == "balanced");

  const auto unavailable = LunaDash::parsePowerProfiles("unknown:\n");
  assert(unavailable.at("available") == false);
  assert(unavailable.at("profiles").empty());
  assert(unavailable.at("current") == "");
}
