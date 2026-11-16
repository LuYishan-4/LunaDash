#include "compositor/wayland/OutputComponent.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "core/filesystem/FileIo.hpp"
#include "core/log/Log.hpp"
#include <algorithm>
#include <string>

namespace LunaDash {

bool WaylandCompositor::Impl::OutputComponent::start() {
  impl->screencopy = wlr_screencopy_manager_v1_create(impl->display);
  impl->xdgOutput =
      wlr_xdg_output_manager_v1_create(impl->display, impl->outputLayout);
  if (!impl->screencopy || !impl->xdgOutput)
    return impl->fail("wlroots could not create the output protocol globals.");
  listen<&OutputComponent::onNewOutput>(&impl->backend->events.new_output,
                                        newOutput);
  return true;
}

void WaylandCompositor::Impl::OutputComponent::onNewOutput(wlr_output *output) {
  if (!output)
    return;
  if (!wlr_output_init_render(output, impl->allocator, impl->renderer)) {
    impl->fail("Could not initialize output rendering.");
    return;
  }
  if (!commitInitialState(output)) {
    impl->fail("wlroots rejected the output configuration.");
    return;
  }

  impl->outputs.push_back(new OutputState(impl, output));
  impl->updateNightLight();
  if (!impl->primaryOutput)
    impl->primaryOutput = output;
  impl->relayoutOutputs();
  impl->setCursorImage("default");
}

bool WaylandCompositor::Impl::OutputComponent::commitInitialState(
    wlr_output *output) {
  wlr_output_state pending;
  wlr_output_state_init(&pending);
  wlr_output_state_set_enabled(&pending, true);

  // The preferred size at its highest refresh rate, unless the user saved a
  // mode this output still offers.
  auto *chosen = wlr_output_preferred_mode(output);
  std::string settingsError;
  const auto settings = loadDisplayConfiguration(output->name, &settingsError);
  if (!settingsError.empty())
    logWarning("Could not migrate saved display settings: %s",
               settingsError.c_str());
  if (!impl->nested) {
    const int savedWidth = settings.value("width", 0);
    const int savedHeight = settings.value("height", 0);
    const int savedRefresh = settings.value("refresh", 0);
    wlr_output_mode *candidate = nullptr;
    if (chosen) {
      const auto *preferred = chosen;
      wl_list_for_each(candidate, &output->modes, link) {
        if (candidate->width == preferred->width &&
            candidate->height == preferred->height &&
            candidate->refresh > chosen->refresh)
          chosen = candidate;
      }
    }
    wl_list_for_each(candidate, &output->modes, link) {
      if (candidate->width == savedWidth && candidate->height == savedHeight &&
          candidate->refresh == savedRefresh) {
        chosen = candidate;
        break;
      }
    }
  }
  if (chosen)
    wlr_output_state_set_mode(&pending, chosen);
  else if (!impl->fullscreen)
    wlr_output_state_set_custom_mode(
        &pending, 1440, 900,
        WlrootsCompat::nestedModeRefresh(output, kDefaultOutputRefreshMilliHz));
  const float scale =
      std::clamp(static_cast<float>(settings.value("scale", 1.0)), 1.0f, 3.0f);
  wlr_output_state_set_scale(&pending, scale);

  bool committed = wlr_output_test_state(output, &pending) &&
                   wlr_output_commit_state(output, &pending);
  if (!committed) {
    wlr_output_state_set_scale(&pending, 1.0f);
    if (auto *preferred = wlr_output_preferred_mode(output))
      wlr_output_state_set_mode(&pending, preferred);
    committed = wlr_output_commit_state(output, &pending);
  }
  wlr_output_state_finish(&pending);
  return committed;
}

} // namespace LunaDash
