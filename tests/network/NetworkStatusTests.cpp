#include "desktop/network/NetworkStatus.hpp"

#include <cassert>

int main() {
  const auto online = LunaDash::describeNetwork(70, 4);
  assert(online.at("managed").get<bool>());
  assert(online.at("connected").get<bool>());
  assert(online.at("internet").get<bool>());
  assert(online.at("label") == "Connected to the Internet");

  const auto captivePortal = LunaDash::describeNetwork(70, 2);
  assert(captivePortal.at("connected").get<bool>());
  assert(!captivePortal.at("internet").get<bool>());
  assert(captivePortal.at("label") == "Sign-in required");

  const auto connecting = LunaDash::describeNetwork(40, 1);
  assert(!connecting.at("connected").get<bool>());
  assert(connecting.at("label") == "Connecting");
}
