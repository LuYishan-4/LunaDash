#include "desktop/weather/WeatherStatus.hpp"

#include <wayland-server-core.h>

#include <cassert>

namespace {
LunaDash::WeatherPreferences disabledPreferences() { return {}; }
} // namespace

int main() {
  auto *display = wl_display_create();
  assert(display);
  {
    LunaDash::WeatherStatus weather(wl_display_get_event_loop(display),
                                    disabledPreferences);
    const auto snapshot = weather.snapshot();
    assert(snapshot.at("enabled") == false);
    assert(snapshot.at("available") == false);
    weather.refresh();
    assert(weather.snapshot() == snapshot);
  }
  wl_display_destroy(display);
}
