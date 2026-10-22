#include "compositor/wayland/Register.hpp"
#include "config/desktop/DesktopPreferences.hpp"

namespace LunaDash {
void WaylandCompositor::Impl::updateNightLight() {
  const auto preferences = desktopPreferences();
  const int temperature =
      preferences.value("eyeCare").toBool()
          ? preferences.value("eyeCareTemperature").toInt(4500)
          : 6500;
  for (auto *state : outputs) {
    if (state->nightTemperature == temperature)
      continue;
    auto *next =
        temperature < 6500 ? ludash_night_color_create(temperature) : nullptr;
    const bool applied = (temperature == 6500 || next) &&
                         ludash_night_color_apply(state->output, next);
    if (!applied) {
      ludash_night_color_destroy(next);
      state->nightError = "This output or renderer cannot apply the requested "
                          "color temperature.";
      continue;
    }
    ludash_night_color_destroy(state->nightColor);
    state->nightColor = next;
    state->nightTemperature = temperature;
    state->nightError.clear();
    wlr_damage_ring_add_whole(&state->sceneOutput->damage_ring);
    wlr_output_schedule_frame(state->output);
  }
}
} // namespace LunaDash
