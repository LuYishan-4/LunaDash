#include "compositor/window/animation/WindowAnimation.hpp"
#include "compositor/client/ClientWindow.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/SurfaceText.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "config/desktop/DesktopPreferenceStore.hpp"
#include "core/filesystem/FileIo.hpp"
#include "core/log/Log.hpp"
#include <algorithm>
#include <unordered_set>
#include <array>
#include <chrono>
#include <ctime>
#include <string_view>
#include <utility>
#include <vector>

namespace LunaDash {
namespace {
bool screencopyPendingForOutput(wlr_screencopy_manager_v1 *manager,
                                wlr_output *output) {
  if (!manager || !output)
    return false;
  wlr_screencopy_frame_v1 *frame = nullptr;
  wl_list_for_each(frame, &manager->frames, link)
    if (frame->output == output)
      return true;
  return false;
}
} // namespace

using Templates::attachListener;
using Templates::detachListener;
using Templates::listenerOwner;

Size WaylandCompositor::Impl::outputSize() const {
  if (!primaryOutput)
    return {1440, 900};
  int width = 0;
  int height = 0;
  wlr_output_effective_resolution(primaryOutput, &width, &height);
  return {std::max(1, width), std::max(1, height)};
}

Json WaylandCompositor::Impl::displaySnapshot() const {
  const Size size = outputSize();
  const auto now = std::chrono::steady_clock::now();
  const int revertSeconds =
      pendingDisplay && displayRevertDeadline > now
          ? static_cast<int>(
                std::chrono::ceil<std::chrono::seconds>(
                    displayRevertDeadline - now)
                    .count())
          : 0;
  Json nightLight = Json::array();
  std::uint64_t frameCallbacks = 0;
  std::uint64_t slowFrames = 0;
  std::int64_t lastFrameWorkUsec = 0;
  std::int64_t maxFrameWorkUsec = 0;
  for (const auto *output : outputs) {
    frameCallbacks += output->frameCallbacks;
    slowFrames += output->slowFrames;
    lastFrameWorkUsec = std::max(lastFrameWorkUsec, output->lastFrameWorkUsec);
    maxFrameWorkUsec = std::max(maxFrameWorkUsec, output->maxFrameWorkUsec);
    nightLight.push_back(Json::object({{"output", safeUtf8(output->output->name)},
                                  {"temperature", output->nightTemperature},
                                  {"error", output->nightError}}));
  }
  return {
      {"frameCallbacks", static_cast<std::int64_t>(frameCallbacks)},
      {"frameWorkMs", static_cast<double>(lastFrameWorkUsec) / 1000.0},
      {"maxFrameWorkMs", static_cast<double>(maxFrameWorkUsec) / 1000.0},
      {"slowFrames", static_cast<std::int64_t>(slowFrames)},
      {"eventLoop",
       Json::object({{"dispatchCalls", static_cast<std::int64_t>(dispatchCalls)},
                   {"lastDispatchMs",
                    static_cast<double>(lastDispatchUsec) / 1000.0},
                   {"maxDispatchMs",
                    static_cast<double>(maxDispatchUsec) / 1000.0},
                   {"slowDispatches", static_cast<std::int64_t>(slowDispatches)}})},
      {"nightLight", nightLight},
      {"width", size.width()},
      {"pixelWidth", primaryOutput ? primaryOutput->width : 0},
      {"pixelHeight", primaryOutput ? primaryOutput->height : 0},
      {"modes", displayModes()},
      {"mode", primaryOutput
                   ? std::to_string(primaryOutput->width) + "x" +
                         std::to_string(primaryOutput->height) + "@" +
                         std::to_string(wl_list_empty(&primaryOutput->modes)
                                            ? 0
                                            : primaryOutput->refresh)
                   : std::string()},
      {"pending", pendingDisplay != nullptr},
      {"revertSeconds", revertSeconds},
      {"error", displayError},
      {"height", size.height()},
      {"scale", primaryOutput ? primaryOutput->scale : 1.0},
      {"refreshRate", primaryOutput
                          ? static_cast<double>(primaryOutput->refresh) / 1000.0
                          : 0.0},
      {"output", primaryOutput ? safeUtf8(primaryOutput->name) : std::string()},
      {"nested", nested},
      {"fullscreen", fullscreen}};
}

int WaylandCompositor::Impl::mappedLayerCount() const {
  int count = 0;
  for (const auto *layer : layers)
    if (layer && layer->mapped)
      ++count;
  return count;
}

void WaylandCompositor::Impl::updateBackground() {
  if (!background)
    return;
  const Size size = outputSize();
  wlr_scene_rect_set_size(background, size.width(), size.height());
  // Neutral fallback while the shell's startup surface and wallpaper map.
  const float color[4] = {0.043f, 0.067f, 0.078f, 1.0f};
  wlr_scene_rect_set_color(background, color);
}

void WaylandCompositor::Impl::updateWindowCorners() {
  if (!windowCorners)
    return;
  std::vector<WindowCorners::Surface> surfaces;
  const float opacity = eyeCareActive ? 1.0f : steadyWindowOpacity;
  for (const auto &client : q->clients_) {
    if (!client->sceneTree)
      continue;
    wlr_box geometry{};
    if (client->surface) {
#if WLR_VERSION_MINOR >= 19
      geometry = client->surface->geometry;
#else
      wlr_xdg_surface_get_geometry(client->surface, &geometry);
#endif
    }
    surfaces.push_back({client->sceneTree, client->sceneContent, client->wlSurface,
                     {client->geometry.width(), client->geometry.height()},
                     {geometry.x, geometry.y},
                     client->mapped && !client->fullscreen &&
                         !client->desktop && !client->utility, opacity});
  }
  windowCorners->update(surfaces);
}

void WaylandCompositor::Impl::updateWindowGlass() {
  if (!windowGlass)
    return;
  std::vector<WindowGlass::Surface> surfaces;
  for (const auto &client : q->clients_)
    if (client->sceneTree)
      surfaces.push_back({client->sceneTree,
                       {client->geometry.width(), client->geometry.height()},
                       client->mapped && !client->fullscreen &&
                           !client->desktop && !client->utility,
                       windowCorners ? windowCorners->radius(client->sceneTree) : 0});
  // The shell's overlays and popups are layer-shell surfaces, not xdg
  // toplevels. Give them the same producer-owned, input-transparent blur
  // underlays; never sample the panel's own text or change its buffer alpha.
  // Without blur support they keep their own (more opaque) glass.
  for (const auto *layer : layers) {
    if (!layer || !layer->mapped || !layer->surface || !layer->sceneLayer)
      continue;
    const std::string name =
        layer->surface->namespace_ ? layer->surface->namespace_ : "";
    // LunaDash-Shell overlays cover the screen; its popups are exactly their rounded surface
    // (Style.radius 24). The panel, dock, wallpaper, toasts and OSD are not frosted.
    static const std::unordered_set<std::string> overlays{
        "lunadash-settings", "lunadash-dashboard", "lunadash-launcher", "lunadash-gallery",
        "lunadash-wallpaper-library", "lunadash-switcher", "lunadash-power",
        "lunadash-polkit", "lunadash-welcome"};
    static const std::unordered_set<std::string> popups{
        "lunadash-audio", "lunadash-calendar", "lunadash-clipboard", "lunadash-network",
        "lunadash-tray-menu"};
    // Full-screen overlays need square corners: a rounded blur leaves the screen corners sharp.
    const int radius = overlays.contains(name) ? 0 : popups.contains(name) ? 24 : -1;
    if (radius < 0)
      continue;
    const auto &state = layer->surface->surface->current;
    surfaces.push_back({layer->sceneLayer->tree, {state.width, state.height},
                     true, radius, 1.0f});
  }
  const bool animating = q->windowAnimations_ &&
                         q->windowAnimations_->activeCount() > 0;
  windowGlass->update(surfaces, animating);
}

void WaylandCompositor::Impl::arrangeLayers() {
  const Size size = outputSize();
  wlr_box full{0, 0, size.width(), size.height()};
  wlr_box usable = full;
  // Reserve panels before configuring non-exclusive surfaces, regardless of
  // client startup/commit order.
  for (const bool exclusive : {true, false}) {
    for (auto *layer : layers) {
      if (!layer || !layer->sceneLayer || !layer->surface)
        continue;
      // wlroots 0.20 emits layer_shell.new_surface before the client's first
      // commit. wlr_scene_layer_surface_v1_configure() is only valid after the
      // role has been initialized by that commit.
      if (!layer->surface->initialized ||
          (layer->surface->current.exclusive_zone > 0) != exclusive)
        continue;
      if (!layer->surface->output && primaryOutput)
        layer->surface->output = primaryOutput;
      wlr_scene_layer_surface_v1_configure(layer->sceneLayer, &full, &usable);
      // A layer that is still sliding in keeps sliding toward its new place.
      if (layer->mapped && q->windowAnimations_)
        q->windowAnimations_->animation().retarget(layer->sceneLayer->tree,
                                                   layerRectangle(layer));
    }
  }
  usableArea = Rect(usable.x, usable.y, usable.width, usable.height);
}

bool WaylandCompositor::Impl::resizePrimaryOutput(const std::string &preset,
                                                  std::string *error) {
  static const std::array<std::pair<std::string_view, Size>, 4> sizes{{
      {"800x600", {800, 600}},
      {"1280x720", {1280, 720}},
      {"1440x900", {1440, 900}},
      {"1920x1080", {1920, 1080}},
  }};
  const auto presetName = preset;
  const auto size = std::find_if(sizes.begin(), sizes.end(),
                                 [&presetName](const auto &entry) {
                                   return entry.first == presetName;
                                 });
  if (!nested || fullscreen || size == sizes.end() || !primaryOutput) {
    if (error)
      *error = "Choose a supported size in a windowed nested wlroots session.";
    return false;
  }

  wlr_output_state state;
  wlr_output_state_init(&state);
  wlr_output_state_set_custom_mode(
      &state, size->second.width(), size->second.height(),
      WlrootsCompat::nestedModeRefresh(primaryOutput,
                                       kDefaultOutputRefreshMilliHz));
  const bool ok = wlr_output_commit_state(primaryOutput, &state);
  wlr_output_state_finish(&state);
  if (!ok) {
    if (error)
      *error = "The wlroots backend rejected this nested output size.";
    return false;
  }
  updateBackground();
  arrangeLayers();
  q->arrange();
  return true;
}

void WaylandCompositor::Impl::handleNewOutput(wl_listener *listener,
                                              void *data) {
  auto *self = listenerOwner<Impl>(listener);
  auto *output = static_cast<wlr_output *>(data);
  if (!self || !output)
    return;

  if (!wlr_output_init_render(output, self->allocator, self->renderer)) {
    self->fail("Could not initialize output rendering.");
    return;
  }

  wlr_output_state pending;
  wlr_output_state_init(&pending);
  wlr_output_state_set_enabled(&pending, true);
  auto *chosen = wlr_output_preferred_mode(output);
  if (chosen && !self->nested) {
    wlr_output_mode *candidate = nullptr;
    wl_list_for_each(candidate, &output->modes, link) {
      if (candidate->width == chosen->width &&
          candidate->height == chosen->height &&
          candidate->refresh > chosen->refresh)
        chosen = candidate;
    }
  }
  std::string settingsError;
  const auto settings = loadDisplayConfiguration(output->name, &settingsError);
  if (!settingsError.empty())
    logWarning("Could not migrate saved display settings: %s",
             settingsError.c_str());
  if (!self->nested) {
    wlr_output_mode *candidate = nullptr;
    wl_list_for_each(candidate, &output->modes, link) {
      if (candidate->width == settings.value("width", 0) &&
          candidate->height == settings.value("height", 0) &&
          candidate->refresh == settings.value("refresh", 0)) {
        chosen = candidate;
        break;
      }
    }
  }
  if (chosen)
    wlr_output_state_set_mode(&pending, chosen);
  else if (!self->fullscreen)
    wlr_output_state_set_custom_mode(
        &pending, 1440, 900,
        WlrootsCompat::nestedModeRefresh(output,
                                         kDefaultOutputRefreshMilliHz));
  const float scale =
      std::clamp(static_cast<float>(settings.value("scale", 1.0)), 1.0f, 3.0f);
  wlr_output_state_set_scale(&pending, scale);
  if (!wlr_output_test_state(output, &pending) ||
      !wlr_output_commit_state(output, &pending)) {
    wlr_output_state_set_scale(&pending, 1.0f);
    if (auto *preferred = wlr_output_preferred_mode(output))
      wlr_output_state_set_mode(&pending, preferred);
    if (!wlr_output_commit_state(output, &pending)) {
      wlr_output_state_finish(&pending);
      self->fail("wlroots rejected the output configuration.");
      return;
    }
  }
  wlr_output_state_finish(&pending);

  auto *state = new OutputState;
  state->impl = self;
  state->output = output;
  // Hardware cursor planes avoid a full scene composition on every mouse
  // movement. Screencopy temporarily switches to software cursors so portal
  // streams still receive the pointer. Direct-KMS recorders can opt into the
  // old always-composited behavior with LUDASH_SOFTWARE_CURSOR=1.
  state->forceSoftwareCursor =
      !self->nested && environmentInt("LUDASH_SOFTWARE_CURSOR") == 1;
  if (state->forceSoftwareCursor) {
    wlr_output_lock_software_cursors(output, true);
    state->softwareCursorLocked = true;
  }
  auto *layoutOutput = wlr_output_layout_add_auto(self->outputLayout, output);
  state->sceneOutput = wlr_scene_output_create(self->scene, output);
  if (layoutOutput && state->sceneOutput)
    wlr_scene_output_layout_add_output(self->sceneLayout, layoutOutput,
                                       state->sceneOutput);

  attachListener(&output->events.frame, state->frame, state, handleOutputFrame);
  attachListener(&output->events.destroy, state->destroy, state,
                 handleOutputDestroy);
  attachListener(&output->events.request_state, state->requestState, state,
                 handleOutputRequestState);
  self->outputs.push_back(state);
  self->updateNightLight();

  if (!self->primaryOutput)
    self->primaryOutput = output;
  self->updateBackground();
  self->arrangeLayers();
  self->q->arrange();
  self->setCursorImage("default");
}

void WaylandCompositor::Impl::handleOutputFrame(wl_listener *listener, void *) {
  auto *state = listenerOwner<OutputState>(listener);
  if (!state || !state->sceneOutput)
    return;
  ++state->frameCallbacks;
  timespec now{};
  clock_gettime(CLOCK_MONOTONIC, &now);
  const std::uint64_t nowNs = static_cast<std::uint64_t>(now.tv_sec) * 1000000000ULL +
                        static_cast<std::uint64_t>(now.tv_nsec);

  // Sample before commit: a screencopy frame is removed when this output
  // commit satisfies it. The tail bridges the small gap before xdpw queues
  // the next PipeWire frame and prevents a one-frame/frozen stream.
  const bool capturePending =
      screencopyPendingForOutput(state->impl->screencopy, state->output);
  if (capturePending)
    state->screencopyKeepalive = 6;
  else if (state->screencopyKeepalive > 0)
    --state->screencopyKeepalive;

  const bool needsSoftwareCursor =
      state->forceSoftwareCursor || capturePending || state->screencopyKeepalive > 0;
  if (needsSoftwareCursor != state->softwareCursorLocked) {
    wlr_output_lock_software_cursors(state->output, needsSoftwareCursor);
    state->softwareCursorLocked = needsSoftwareCursor;
  }

  auto *animations = state->impl->q->windowAnimations_.get();
  const bool wasAnimating = animations && animations->activeCount() > 0;
  if (animations)
    animations->advance();
  const bool stillAnimating = animations && animations->activeCount() > 0;
  if (wasAnimating &&
      (!stillAnimating || state->lastAnimationPublishNs == 0 ||
       nowNs - state->lastAnimationPublishNs >= 33000000ULL)) {
    state->impl->q->publishWindowLayout();
    state->lastAnimationPublishNs = stillAnimating ? nowNs : 0;
  } else if (!wasAnimating) {
    state->lastAnimationPublishNs = 0;
  }
  state->impl->updateWindowGlass();
  if (!ludash_night_color_commit(state->sceneOutput, state->nightColor)) {
    logWarning("wlroots scene output commit failed.");
    if (state->nightColor) {
      ludash_night_color_destroy(state->nightColor);
      state->nightColor = nullptr;
      state->nightTemperature = 6500;
      state->nightError = "The renderer rejected night light; the normal output has been restored.";
      wlr_output_schedule_frame(state->output);
    }
  }
  wlr_scene_output_send_frame_done(state->sceneOutput, &now);

  if (animations && animations->activeCount() > 0)
    wlr_output_schedule_frame(state->output);

  // Do not spin a screencopy tail synchronously from the output frame
  // callback. The headless backend can deliver scheduled frames immediately;
  // chaining them here starves the Wayland event loop and later screencopy
  // clients never get far enough to submit their copy request. Pace the tail
  // at roughly one display interval instead. wlroots still schedules the
  // first frame for every copy request itself.
  if (capturePending || state->screencopyKeepalive > 0) {
    // Coalesce callbacks from capture, client damage and animation into one
    // wakeup per output. Several frames inside 16 ms must not grow a timer tail.
    if (!state->screencopyTimer) {
      state->screencopyTimer = wl_event_loop_add_timer(
          state->impl->eventLoop, handleScreencopyTimer, state);
    }
    if (state->screencopyTimer && !state->screencopyTimerScheduled) {
      if (wl_event_source_timer_update(state->screencopyTimer, 16) == 0)
        state->screencopyTimerScheduled = true;
      else
        wlr_log(WLR_ERROR, "Could not schedule screencopy continuation.");
    } else if (!state->screencopyTimer) {
      wlr_log(WLR_ERROR, "Could not create screencopy continuation timer.");
    }
  }

  timespec finished{};
  clock_gettime(CLOCK_MONOTONIC, &finished);
  const std::uint64_t finishedNs =
      static_cast<std::uint64_t>(finished.tv_sec) * 1000000000ULL +
      static_cast<std::uint64_t>(finished.tv_nsec);
  state->lastFrameWorkUsec =
      static_cast<std::int64_t>((finishedNs - nowNs) / 1000ULL);
  state->maxFrameWorkUsec =
      std::max(state->maxFrameWorkUsec, state->lastFrameWorkUsec);
  if (state->lastFrameWorkUsec >= 8000)
    ++state->slowFrames;
}

int WaylandCompositor::Impl::handleScreencopyTimer(void *data) {
  auto *state = static_cast<OutputState *>(data);
  if (!state)
    return 0;
  state->screencopyTimerScheduled = false;
  if (state->output)
    wlr_output_schedule_frame(state->output);
  return 0;
}

void WaylandCompositor::Impl::handleOutputRequestState(wl_listener *listener,
                                                       void *data) {
  auto *state = listenerOwner<OutputState>(listener);
  auto *event = static_cast<wlr_output_event_request_state *>(data);
  if (state && event && wlr_output_commit_state(state->output, event->state)) {
    state->impl->updateBackground();
    state->impl->arrangeLayers();
    state->impl->q->arrange();
  }
}

void WaylandCompositor::Impl::handleOutputDestroy(wl_listener *listener,
                                                  void *) {
  auto *state = listenerOwner<OutputState>(listener);
  if (!state)
    return;
  auto *self = state->impl;
  if (state->screencopyTimer)
    wl_event_source_remove(state->screencopyTimer);
  state->screencopyTimer = nullptr;
  if (self->pendingDisplay == state->output)
    self->clearPendingDisplay();
  detachListener(state->frame);
  detachListener(state->destroy);
  detachListener(state->requestState);
  if (self->primaryOutput == state->output)
    self->primaryOutput = nullptr;
  std::erase(self->outputs, state);
  ludash_night_color_destroy(state->nightColor);
  for (auto *candidate : self->outputs)
    if (candidate && candidate->output) {
      self->primaryOutput = candidate->output;
      break;
    }
  delete state;
  self->updateBackground();
  self->arrangeLayers();
  self->q->arrange();
}
} // namespace LunaDash
