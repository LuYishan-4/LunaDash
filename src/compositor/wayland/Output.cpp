#include "compositor/client/ClientWindow.hpp"
#include "compositor/wayland/Runtime.hpp"
#include "compositor/wayland/SurfaceText.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include <QJsonArray>
#include <QSettings>
#include <QTimer>
#include <algorithm>
#include <ctime>

namespace LunaDash {
using Templates::attachListener;
using Templates::detachListener;
using Templates::listenerOwner;

QSize WaylandCompositor::Impl::outputSize() const {
  if (!primaryOutput)
    return {1440, 900};
  int width = 0;
  int height = 0;
  wlr_output_effective_resolution(primaryOutput, &width, &height);
  return {std::max(1, width), std::max(1, height)};
}

QJsonObject WaylandCompositor::Impl::displaySnapshot() const {
  const QSize size = outputSize();
  return {{"width", size.width()},
          {"height", size.height()},
          {"scale", primaryOutput ? primaryOutput->scale : 1.0},
          {"refreshRate",
           primaryOutput ? static_cast<double>(primaryOutput->refresh) / 1000.0
                         : 0.0},
          {"output", primaryOutput ? safeUtf8(primaryOutput->name) : QString()},
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
  const QSize size = outputSize();
  wlr_scene_rect_set_size(background, size.width(), size.height());
  const int palette = QSettings().value("appearance/wallpaper", 0).toInt();
  if (palette == 1) {
    const float color[4] = {0.06f, 0.20f, 0.16f, 1.0f};
    wlr_scene_rect_set_color(background, color);
  } else {
    const float color[4] = {0.07f, 0.09f, 0.18f, 1.0f};
    wlr_scene_rect_set_color(background, color);
  }
}

void WaylandCompositor::Impl::arrangeLayers() {
  const QSize size = outputSize();
  wlr_box full{0, 0, size.width(), size.height()};
  wlr_box usable = full;
  for (auto *layer : layers) {
    if (!layer || !layer->sceneLayer || !layer->surface)
      continue;
    // wlroots 0.20 emits layer_shell.new_surface before the client's first
    // commit. wlr_scene_layer_surface_v1_configure() is only valid after the
    // role has been initialized by that commit.
    if (!layer->surface->initialized)
      continue;
    if (!layer->surface->output && primaryOutput)
      layer->surface->output = primaryOutput;
    wlr_scene_layer_surface_v1_configure(layer->sceneLayer, &full, &usable);
  }
  usableArea = QRect(usable.x, usable.y, usable.width, usable.height);
}

bool WaylandCompositor::Impl::resizePrimaryOutput(const QString &preset,
                                                  QString *error) {
  static const QHash<QString, QSize> sizes{
      {"1280x720", {1280, 720}},
      {"1440x900", {1440, 900}},
      {"1920x1080", {1920, 1080}},
  };
  if (!nested || fullscreen || !sizes.contains(preset) || !primaryOutput) {
    if (error)
      *error = "Choose a supported size in a windowed nested wlroots session.";
    return false;
  }

  const QSize size = sizes.value(preset);
  wlr_output_state state;
  wlr_output_state_init(&state);
  wlr_output_state_set_custom_mode(&state, size.width(), size.height(), 0);
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
  if (auto *mode = wlr_output_preferred_mode(output))
    wlr_output_state_set_mode(&pending, mode);
  else if (!self->fullscreen)
    wlr_output_state_set_custom_mode(&pending, 1440, 900, 60000);

  if (!wlr_output_commit_state(output, &pending))
    qWarning("wlroots rejected the preferred output state.");
  wlr_output_state_finish(&pending);

  auto *state = new OutputState;
  state->impl = self;
  state->output = output;
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
  self->outputs.append(state);

  if (!self->primaryOutput)
    self->primaryOutput = output;
  self->updateBackground();
  self->arrangeLayers();
  self->q->arrange();
  wlr_cursor_set_xcursor(self->cursor, self->cursorManager, "default");
}

void WaylandCompositor::Impl::handleOutputFrame(wl_listener *listener, void *) {
  auto *state = listenerOwner<OutputState>(listener);
  if (!state || !state->sceneOutput)
    return;
  if (!wlr_scene_output_commit(state->sceneOutput, nullptr))
    qWarning("wlroots scene output commit failed.");
  timespec now{};
  clock_gettime(CLOCK_MONOTONIC, &now);
  wlr_scene_output_send_frame_done(state->sceneOutput, &now);
}

void WaylandCompositor::Impl::handleOutputRequestState(wl_listener *listener,
                                                       void *data) {
  auto *state = listenerOwner<OutputState>(listener);
  auto *event = static_cast<wlr_output_event_request_state *>(data);
  if (state && event)
    wlr_output_commit_state(state->output, event->state);
}

void WaylandCompositor::Impl::handleOutputDestroy(wl_listener *listener,
                                                  void *) {
  auto *state = listenerOwner<OutputState>(listener);
  if (!state)
    return;
  auto *self = state->impl;
  detachListener(state->frame);
  detachListener(state->destroy);
  detachListener(state->requestState);
  if (self->primaryOutput == state->output)
    self->primaryOutput = nullptr;
  self->outputs.removeAll(state);
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
