#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/SurfaceText.hpp"
#include "config/desktop/DesktopPreferenceStore.hpp"
#include "core/log/Log.hpp"
#include "core/text/Text.hpp"
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace LunaDash {
namespace {
std::string modeId(int width, int height, int refresh) {
  return std::to_string(width) + "x" + std::to_string(height) + "@" +
         std::to_string(refresh);
}

std::string modeLabel(int width, int height, int refreshMilliHz) {
  char refresh[32];
  std::snprintf(refresh, sizeof refresh, "%.2f", refreshMilliHz / 1000.0);
  return std::to_string(width) + " \u00d7 " + std::to_string(height) +
         " \u00b7 " + refresh + " Hz";
}

int parseDimension(const std::string &text) {
  int value = 0;
  const auto result =
      std::from_chars(text.data(), text.data() + text.size(), value);
  return result.ec == std::errc() && result.ptr == text.data() + text.size()
             ? value
             : 0;
}
} // namespace

Json WaylandCompositor::Impl::displayModes() const {
  Json result = Json::array();
  if (!primaryOutput)
    return result;
  wlr_output_mode *mode = nullptr;
  wl_list_for_each(mode, &primaryOutput->modes, link) {
    result.push_back(
        Json::object({{"id", modeId(mode->width, mode->height, mode->refresh)},
                    {"label", modeLabel(mode->width, mode->height,
                                        mode->refresh)}}));
  }
  if (result.empty() && nested && !fullscreen) {
    for (const Size size :
         {Size(1280, 720), Size(1440, 900), Size(1920, 1080)})
      result.push_back(Json::object({
          {"id", modeId(size.width(), size.height(),
                         kDefaultOutputRefreshMilliHz)},
          {"label", modeLabel(size.width(), size.height(),
                              kDefaultOutputRefreshMilliHz)}}));
  }
  return result;
}

bool WaylandCompositor::Impl::configureDisplay(const Json &changes,
                                               std::string *error) {
  if (!primaryOutput || pendingDisplay || changes.empty()) {
    *error =
        "Confirm or revert the pending display change before applying another.";
    return false;
  }
  for (auto it = changes.begin(); it != changes.end(); ++it) {
    if (it.key() != "mode" && it.key() != "scale") {
      *error = "Unknown display setting.";
      return false;
    }
  }
  const double scale = jsonDouble(changes, "scale", primaryOutput->scale);
  if ((jsonContains(changes, "scale") &&
       !jsonValue(changes, "scale").is_number()) ||
      !std::isfinite(scale) || scale < 1.0 || scale > 3.0) {
    *error = "Display scale must be between 1 and 3.";
    return false;
  }
  wlr_output_state next;
  wlr_output_state_init(&next);
  wlr_output_state_set_scale(&next, static_cast<float>(scale));
  if (jsonContains(changes, "mode")) {
    const std::string id = jsonString(changes, "mode");
    bool found = false;
    wlr_output_mode *mode = nullptr;
    wl_list_for_each(mode, &primaryOutput->modes, link) {
      if (modeId(mode->width, mode->height, mode->refresh) == id) {
        wlr_output_state_set_mode(&next, mode);
        found = true;
        break;
      }
    }
    if (!found && nested && !fullscreen) {
      for (const auto &entry : displayModes()) {
        if (jsonString(entry, "id") != id)
          continue;
        const auto size = splitText(id.substr(0, id.find('@')), 'x');
        if (size.size() != 2)
          continue;
        wlr_output_state_set_custom_mode(&next, parseDimension(size[0]),
                                         parseDimension(size[1]),
                                         kDefaultOutputRefreshMilliHz);
        found = true;
        break;
      }
    }
    if (!found) {
      wlr_output_state_finish(&next);
      *error = "Choose a mode advertised by the active display.";
      return false;
    }
  }
  wlr_output_state_init(&previousDisplay);
  if (primaryOutput->current_mode)
    wlr_output_state_set_mode(&previousDisplay, primaryOutput->current_mode);
  else
    wlr_output_state_set_custom_mode(&previousDisplay, primaryOutput->width,
                                     primaryOutput->height,
                                     primaryOutput->refresh);
  wlr_output_state_set_scale(&previousDisplay, primaryOutput->scale);
  if (!wlr_output_test_state(primaryOutput, &next) ||
      !wlr_output_commit_state(primaryOutput, &next)) {
    wlr_output_state_finish(&previousDisplay);
    wlr_output_state_finish(&next);
    *error = "The display rejected this resolution, refresh rate or scale.";
    return false;
  }
  wlr_output_state_finish(&next);
  pendingDisplay = primaryOutput;
  displayError.clear();
  if (!displayRevertTimer)
    displayRevertTimer =
        wl_event_loop_add_timer(eventLoop, handleDisplayRevertTimer, this);
  if (!displayRevertTimer ||
      wl_event_source_timer_update(displayRevertTimer, 15000) < 0) {
    const bool restored =
        wlr_output_commit_state(primaryOutput, &previousDisplay);
    displayError = restored
                       ? "Could not schedule display confirmation timeout."
                       : "Could not schedule confirmation timeout or restore "
                         "the previous display mode.";
    *error = displayError;
    clearPendingDisplay();
    updateBackground();
    arrangeLayers();
    q->arrange();
    return false;
  }
  displayRevertDeadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(15000);
  updateBackground();
  arrangeLayers();
  q->arrange();
  return true;
}

int WaylandCompositor::Impl::handleDisplayRevertTimer(void *data) {
  auto *self = static_cast<Impl *>(data);
  if (self)
    self->revertDisplay();
  return 0;
}

void WaylandCompositor::Impl::confirmDisplay() {
  if (!pendingDisplay)
    return;
  std::string error;
  if (!saveDisplaySettings(
          pendingDisplay->name,
          {{"width", pendingDisplay->width},
           {"height", pendingDisplay->height},
           {"refresh", pendingDisplay->refresh},
           {"scale", pendingDisplay->scale}},
          &error))
    logWarning("Could not save display settings: %s", error.c_str());
  clearPendingDisplay();
}

void WaylandCompositor::Impl::clearPendingDisplay() {
  if (displayRevertTimer) {
    wl_event_source_remove(displayRevertTimer);
    displayRevertTimer = nullptr;
  }
  displayRevertDeadline = {};
  if (pendingDisplay)
    wlr_output_state_finish(&previousDisplay);
  pendingDisplay = nullptr;
}

void WaylandCompositor::Impl::revertDisplay() {
  if (!pendingDisplay)
    return;
  if (!wlr_output_commit_state(pendingDisplay, &previousDisplay))
    displayError = "Could not restore the previous display mode.";
  clearPendingDisplay();
  updateBackground();
  arrangeLayers();
  q->arrange();
}
} // namespace LunaDash
