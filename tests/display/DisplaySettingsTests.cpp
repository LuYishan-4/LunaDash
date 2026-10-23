#include "desktop/display/BrightnessSettings.hpp"
#include "desktop/display/DdcBrightnessSettings.hpp"

#include <cassert>

int main() {
  const auto backlight =
      LunaDash::parseBacklight("intel_backlight,backlight,937,63%,4000\n");
  assert(backlight.at("available") == true);
  assert(backlight.at("device") == "intel_backlight");
  assert(backlight.at("percent") == 63);
  assert(
      LunaDash::parseBacklight("device,leds,100,50%,100\n").at("available") ==
      false);
  assert(LunaDash::parseBacklight("device,backlight,100,150%,100\n")
             .at("available") == false);

  const auto displays = LunaDash::parseDdcDisplays("Display 1\n"
                                                   "  I2C bus: /dev/i2c-6\n"
                                                   "  Monitor: Dell U2720Q\n"
                                                   "Display 2\n"
                                                   "  I2C bus: /dev/i2c-6\n"
                                                   "  Monitor: Duplicate bus\n"
                                                   "Display 3\n"
                                                   "  I2C bus: /dev/i2c-9\n"
                                                   "  Monitor: LG 27UL850\n"
                                                   "Invalid display\n"
                                                   "  I2C bus: /dev/i2c-12\n"
                                                   "  Monitor: Incomplete\n");
  assert(displays.size() == 2);
  assert(displays.at(0).at("id") == "i2c-6:Dell U2720Q");
  assert(displays.at(0).at("available") == false);
  assert(displays.at(1).at("bus") == 9);

  const auto brightness = LunaDash::parseDdcBrightness("VCP 10 C 45 100\n");
  assert(brightness.at("available") == true);
  assert(brightness.at("maximum") == 100);
  assert(brightness.at("percent") == 45);
  assert(LunaDash::parseDdcBrightness("VCP 10 C 80 70\n").at("available") ==
         false);
  assert(LunaDash::parseDdcBrightness("unsupported\n").at("available") ==
         false);
}
