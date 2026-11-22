#include "compositor/window/animation/WindowAnimation.hpp"
#include "core/log/StallTrace.hpp"
#include "compositor/client/ClientWindow.hpp"
#include "compositor/client/ClientWindowControl.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/SurfaceText.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "config/desktop/DesktopPreferenceStore.hpp"
#include "core/filesystem/FileIo.hpp"
#include "core/log/Log.hpp"
#include <algorithm>
#include <cstdint>
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

constexpr int kShellSurfaceRadius = 24;


int layerBlurRadius(const wlr_layer_surface_v1 &layer) {
  constexpr std::uint32_t edges =
      ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
      ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
  const auto &state = layer.current;
  const bool fillsOutput = (state.anchor & edges) == edges && state.margin.top == 0 &&
                           state.margin.right == 0 && state.margin.bottom == 0 &&
                           state.margin.left == 0;
  return fillsOutput ? 0 : kShellSurfaceRadius;
}

std::uint64_t nanoseconds(const timespec &time) {
  return static_cast<std::uint64_t>(time.tv_sec) * 1000000000ULL +
         static_cast<std::uint64_t>(time.tv_nsec);
}
} // namespace

Size WaylandCompositor::Impl::outputSize() const {
  if (!primaryOutput)
    return virtualOutputSize;
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
      {"fullscreen", options().fullscreen}};
}
//debug info
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
  wlr_box box{};
  wlr_output_layout_get_box(outputLayout, nullptr, &box);
  if (wlr_box_empty(&box))
    box = {0, 0, virtualOutputSize.width(), virtualOutputSize.height()};
  wlr_scene_node_set_position(&background->node, box.x, box.y);
  wlr_scene_rect_set_size(background, box.width, box.height);
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
    const Size logical = clientLogicalSize(*client, client->geometry.size());
    surfaces.push_back({client->sceneTree, client->sceneContent, client->wlSurface,
                     {logical.width(), logical.height()},
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
  // Windows blur the wallpaper beneath them (one shared capture per output),
  // not the windows: moving, animating or typing never captures again.
  for (const auto &client : q->clients_)
    if (client->sceneTree)
      surfaces.push_back({client->sceneTree,
                       {client->geometry.width(), client->geometry.height()},
                       client->mapped && !client->fullscreen &&
                           !client->desktop && !client->utility,
                       windowCorners ? windowCorners->radius(client->sceneTree) : 0,
                       -1.0f,
                       q->windowAnimations_ &&
                           q->windowAnimations_->animation().ownsOpacity(client->sceneTree),
                       true});
  // Layer-shell surfaces (the shell's overlays and popups) opt in through
  // ext-background-effect-v1 and get the same producer-owned,
  // input-transparent blur underlays; never sample the panel's own text or
  // change its buffer alpha. Without the blur capability shells keep their
  // own (more opaque) glass.
  for (const auto *layer : layers) {
    if (!layer || !layer->mapped || !layer->surface || !layer->sceneLayer ||
        !backgroundEffects || !backgroundEffects->wantsBlur(layer->surface->surface))
      continue;
    const auto &state = layer->surface->surface->current;
    surfaces.push_back({layer->sceneLayer->tree, {state.width, state.height},
                     true, layerBlurRadius(*layer->surface), 1.0f,
                     q->windowAnimations_ &&
                         q->windowAnimations_->animation().ownsOpacity(layer->sceneLayer->tree)});
  }
  std::vector<WindowGlass::Output> boxes;
  for (const auto *state : outputs) {
    if (!state || !state->output)
      continue;
    wlr_box box{};
    wlr_output_layout_get_box(outputLayout, state->output, &box);
    if (!wlr_box_empty(&box))
      boxes.push_back({box.x, box.y, box.width, box.height});
  }
  const bool animating = q->windowAnimations_ &&
                         q->windowAnimations_->activeCount() > 0;
  windowGlass->update(surfaces, boxes, animating);
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

void WaylandCompositor::Impl::loadOutputSizePreferences() {
  const auto preferences = desktopPreferences();
  virtualOutputSize = LunaDash::virtualOutputSize(preferences);
  virtualOutputSizes = LunaDash::virtualOutputSizes(preferences);
}

bool WaylandCompositor::Impl::resizePrimaryOutput(const std::string &preset,
                                                  std::string *error) {
  // The offered sizes are the desktop preference virtualOutputSizes.
  const auto size = parseVirtualOutputSize(preset);
  const bool offered =
      size && std::find(virtualOutputSizes.begin(), virtualOutputSizes.end(),
                        *size) != virtualOutputSizes.end();
  if (!nested || options().fullscreen || !offered || !primaryOutput) {
    if (error)
      *error = "Choose a size from virtualOutputSizes in a windowed nested "
               "wlroots session.";
    return false;
  }

  wlr_output_state state;
  wlr_output_state_init(&state);
  wlr_output_state_set_custom_mode(
      &state, size->width(), size->height(),
      WlrootsCompat::nestedModeRefresh(primaryOutput,
                                       kDefaultOutputRefreshMilliHz));
  const bool ok = wlr_output_commit_state(primaryOutput, &state);
  wlr_output_state_finish(&state);
  if (!ok) {
    if (error)
      *error = "The wlroots backend rejected this nested output size.";
    return false;
  }
  relayoutOutputs();
  return true;
}

void WaylandCompositor::Impl::updateSessionLockOutputs() {
  if (!sessionLock)
    return;
  std::vector<SessionLock::Output> boxes;
  for (const auto *state : outputs) {
    if (!state || !state->output)
      continue;
    wlr_box box{};
    wlr_output_layout_get_box(outputLayout, state->output, &box);
    if (!wlr_box_empty(&box))
      boxes.push_back({state->output, box.x, box.y, box.width, box.height});
  }
  sessionLock->setOutputs(boxes);
}

void WaylandCompositor::Impl::relayoutOutputs() {
  updateBackground();
  updateSessionLockOutputs();
  arrangeLayers();
  q->arrange();
}

WaylandCompositor::Impl::OutputState::OutputState(Impl *owner,
                                                  wlr_output *wlrOutput)
    : impl(owner), output(wlrOutput) {
  // Hardware cursor planes avoid a full scene composition on every mouse
  // movement. Screencopy temporarily switches to software cursors so portal
  // streams still receive the pointer. Direct-KMS recorders can opt into the
  // old always-composited behavior with LUDASH_SOFTWARE_CURSOR=1.
  forceSoftwareCursor =
      !impl->nested && environmentInt("LUDASH_SOFTWARE_CURSOR") == 1;
  if (forceSoftwareCursor) {
    wlr_output_lock_software_cursors(output, true);
    softwareCursorLocked = true;
  }
  auto *layoutOutput = wlr_output_layout_add_auto(impl->outputLayout, output);
  sceneOutput = wlr_scene_output_create(impl->scene, output);
  if (layoutOutput && sceneOutput)
    wlr_scene_output_layout_add_output(impl->sceneLayout, layoutOutput,
                                       sceneOutput);

  listen<&OutputState::onFrame, &OutputState::frame>(&output->events.frame);
  listen<&OutputState::onDestroy, &OutputState::destroy>(
      &output->events.destroy);
  listen<&OutputState::onRequestState, &OutputState::requestState>(
      &output->events.request_state);
}

WaylandCompositor::Impl::OutputState::~OutputState() {
  if (screencopyTimer)
    wl_event_source_remove(screencopyTimer);
  ludash_night_color_destroy(nightColor);
}

void WaylandCompositor::Impl::OutputState::onFrame() {
  StallTrace::Section trace("frame", output && output->name ? output->name : "");
  if (!sceneOutput)
    return;
  ++frameCallbacks;
  timespec now{};
  clock_gettime(CLOCK_MONOTONIC, &now);
  const std::uint64_t nowNs = nanoseconds(now);

  const bool capturePending = updateCaptureCursor();
  std::uint64_t stageStart = nowNs;
  const auto stage = [this, &stageStart](std::size_t index) {
    timespec at{};
    clock_gettime(CLOCK_MONOTONIC, &at);
    const std::uint64_t ns = nanoseconds(at);
    auto &entry = frameStages[index];
    entry.lastUsec = static_cast<std::int64_t>((ns - stageStart) / 1000ULL);
    entry.maxUsec = std::max(entry.maxUsec, entry.lastUsec);
    if (entry.lastUsec >= 4000)
      ++entry.slow;
    stageStart = ns;
  };
  advanceAnimations(nowNs);
  stage(0);
  impl->q->advanceOverview(nowNs + refreshPeriodNs());
  stage(1);
  impl->updateWindowGlass();
  stage(2);
  // Commits since the last frame reset zoomed windows to their own size.
  for (const auto &client : impl->q->clients_)
    applyClientContentScale(*client);
  if (impl->windowCorners)
    impl->windowCorners->settleWaits();
  stage(3);
  commitScene();
  stage(4);
  sendFrameDone(&now);
  stage(5);
  // Animations, the overview's live previews and a shaken cursor's growth
  // step every frame.
  const bool cursorGrowing = impl->pointerCursor->advance();
  if ((impl->q->windowAnimations_ &&
       impl->q->windowAnimations_->activeCount() > 0) ||
      impl->q->overviewShown() || cursorGrowing)
    wlr_output_schedule_frame(output);
  scheduleCaptureTail(capturePending);
  recordFrameWork(nowNs);
}

bool WaylandCompositor::Impl::OutputState::updateCaptureCursor() {
  const bool capturePending =
      screencopyPendingForOutput(impl->screencopy, output);
  if (capturePending)
    screencopyKeepalive = 6;
  else if (screencopyKeepalive > 0)
    --screencopyKeepalive;

  const bool needsSoftwareCursor =
      forceSoftwareCursor || capturePending || screencopyKeepalive > 0;
  if (needsSoftwareCursor != softwareCursorLocked) {
    wlr_output_lock_software_cursors(output, needsSoftwareCursor);
    softwareCursorLocked = needsSoftwareCursor;
  }
  return capturePending;
}

std::uint64_t WaylandCompositor::Impl::OutputState::refreshPeriodNs() const {
  constexpr std::uint64_t fallbackRefreshPeriodNs = 16666667ULL;
  return output->refresh > 0
             ? 1000000000000ULL / static_cast<std::uint64_t>(output->refresh)
             : fallbackRefreshPeriodNs;
}

void WaylandCompositor::Impl::OutputState::advanceAnimations(
    std::uint64_t nowNs) {
  auto *animations = impl->q->windowAnimations_.get();
  if (!animations)
    return;
  const bool wasAnimating = animations->activeCount() > 0;
  // This frame reaches the screen about one refresh period from now; step the
  // animations to that time instead of to the moment the frame is built.
  animations->advance(nowNs + refreshPeriodNs());
  const bool stillAnimating = animations->activeCount() > 0;
  // Publish the moving layout at about 30 Hz, and once more when it settles.
  if (wasAnimating &&
      (!stillAnimating || lastAnimationPublishNs == 0 ||
       nowNs - lastAnimationPublishNs >= 33000000ULL)) {
    impl->q->publishWindowLayout();
    lastAnimationPublishNs = stillAnimating ? nowNs : 0;
  } else if (!wasAnimating) {
    lastAnimationPublishNs = 0;
  }
}

void WaylandCompositor::Impl::OutputState::commitScene() {
  if (ludash_night_color_commit(sceneOutput, nightColor))
    return;
  logWarning("wlroots scene output commit failed.");
  if (!nightColor)
    return;
  ludash_night_color_destroy(nightColor);
  nightColor = nullptr;
  nightTemperature = 6500;
  nightError =
      "The renderer rejected night light; the normal output has been restored.";
  wlr_output_schedule_frame(output);
}

void WaylandCompositor::Impl::OutputState::sendFrameDone(timespec *now) {
  wlr_scene_output_send_frame_done(sceneOutput, now);

  const std::uint64_t nowNs = nanoseconds(*now);
  const bool hiddenDue = nowNs - lastHiddenFrameDoneNs >= kHiddenFrameIntervalNs;
  if (hiddenDue)
    lastHiddenFrameDoneNs = nowNs;
  const auto layoutBox = [this](wlr_output *of) {
    wlr_box box{};
    wlr_output_layout_get_box(impl->outputLayout, of, &box);
    return Rect(box.x, box.y, box.width, box.height);
  };
  const Rect screen = layoutBox(output);
  std::vector<Rect> screens;
  for (const auto *state : impl->outputs)
    if (state && state->output)
      screens.push_back(layoutBox(state->output));
  const bool overview = impl->q->overviewShown();
  for (const auto &client : impl->q->clients_) {
    if (!client->mapped || !client->wlSurface ||
        (client->workspace != impl->q->workspace_ && !overview))
      continue;
    // The overview shows every workspace's windows live.
    const bool here = overview || client->geometry.intersects(screen);
    const bool elsewhere =
        !here && std::any_of(screens.begin(), screens.end(), [&](const Rect &other) {
          return client->geometry.intersects(other);
        });
    if (!here && (elsewhere || !hiddenDue))
      continue;
    wlr_surface_for_each_surface(
        client->wlSurface,
        [](wlr_surface *surface, int, int, void *data) {
          wlr_surface_send_frame_done(surface, static_cast<timespec *>(data));
        },
        now);
  }
}

void WaylandCompositor::Impl::OutputState::scheduleCaptureTail(
    bool capturePending) {
  if (!capturePending && screencopyKeepalive == 0)
    return;
  // Do not spin a screencopy tail synchronously from the output frame
  // callback. The headless backend can deliver scheduled frames immediately;
  // chaining them here starves the Wayland event loop and later screencopy
  // clients never get far enough to submit their copy request. Pace the tail
  // at roughly one display interval instead. wlroots still schedules the
  // first frame for every copy request itself.
  //
  // Coalesce callbacks from capture, client damage and animation into one
  // wakeup per output. Several frames inside 16 ms must not grow a timer tail.
  if (!screencopyTimer)
    screencopyTimer = timer<&OutputState::onScreencopyTimer>(impl->eventLoop);
  if (!screencopyTimer) {
    wlr_log(WLR_ERROR, "Could not create screencopy continuation timer.");
    return;
  }
  if (screencopyTimerScheduled)
    return;
  if (wl_event_source_timer_update(screencopyTimer, 16) == 0)
    screencopyTimerScheduled = true;
  else
    wlr_log(WLR_ERROR, "Could not schedule screencopy continuation.");
}

void WaylandCompositor::Impl::OutputState::recordFrameWork(
    std::uint64_t startedNs) {
  timespec finished{};
  clock_gettime(CLOCK_MONOTONIC, &finished);
  lastFrameWorkUsec =
      static_cast<std::int64_t>((nanoseconds(finished) - startedNs) / 1000ULL);
  maxFrameWorkUsec = std::max(maxFrameWorkUsec, lastFrameWorkUsec);
  if (lastFrameWorkUsec >= 8000)
    ++slowFrames;
}

void WaylandCompositor::Impl::OutputState::onScreencopyTimer() {
  screencopyTimerScheduled = false;
  wlr_output_schedule_frame(output);
}

void WaylandCompositor::Impl::OutputState::onRequestState(
    wlr_output_event_request_state *event) {
  if (event && wlr_output_commit_state(output, event->state))
    impl->relayoutOutputs();
}

void WaylandCompositor::Impl::OutputState::onDestroy() {
  Impl *owner = impl;
  if (owner->pendingDisplay == output)
    owner->clearPendingDisplay();
  std::erase(owner->outputs, this);
  owner->primaryOutput =
      owner->outputs.empty() ? nullptr : owner->outputs.front()->output;
  delete this;
  owner->relayoutOutputs();
}
} // namespace LunaDash
