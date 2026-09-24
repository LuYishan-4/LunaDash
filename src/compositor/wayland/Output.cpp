#include "compositor/window/animation/WindowAnimation.hpp"
#include "compositor/client/ClientWindow.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/SurfaceText.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include <QJsonArray>
#include <QSettings>
#include <QTimer>
#include <algorithm>
#include <ctime>

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
  QJsonArray nightLight;
  for (const auto *output : outputs)
    nightLight.append(QJsonObject{{"output", safeUtf8(output->output->name)},
                                  {"temperature", output->nightTemperature},
                                  {"error", output->nightError}});
  return {
      {"nightLight", nightLight},
      {"width", size.width()},
      {"pixelWidth", primaryOutput ? primaryOutput->width : 0},
      {"pixelHeight", primaryOutput ? primaryOutput->height : 0},
      {"modes", displayModes()},
      {"mode", primaryOutput ? QString("%1x%2@%3")
                                   .arg(primaryOutput->width)
                                   .arg(primaryOutput->height)
                                   .arg(wl_list_empty(&primaryOutput->modes)
                                            ? 0
                                            : primaryOutput->refresh)
                             : QString()},
      {"pending", pendingDisplay != nullptr},
      {"revertSeconds", displayRevertTimer && displayRevertTimer->isActive()
                            ? (displayRevertTimer->remainingTime() + 999) / 1000
                            : 0},
      {"error", displayError},
      {"height", size.height()},
      {"scale", primaryOutput ? primaryOutput->scale : 1.0},
      {"refreshRate", primaryOutput
                          ? static_cast<double>(primaryOutput->refresh) / 1000.0
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
  // Neutral fallback while the shell's startup surface and wallpaper map.
  const float color[4] = {0.043f, 0.067f, 0.078f, 1.0f};
  wlr_scene_rect_set_color(background, color);
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
  QSettings settings;
  const QString prefix = "display/" + safeUtf8(output->name) + "/";
  if (!self->nested) {
    wlr_output_mode *candidate = nullptr;
    wl_list_for_each(candidate, &output->modes, link) {
      if (candidate->width == settings.value(prefix + "width").toInt() &&
          candidate->height == settings.value(prefix + "height").toInt() &&
          candidate->refresh == settings.value(prefix + "refresh").toInt()) {
        chosen = candidate;
        break;
      }
    }
  }
  if (chosen)
    wlr_output_state_set_mode(&pending, chosen);
  else if (!self->fullscreen)
    wlr_output_state_set_custom_mode(&pending, 1440, 900, 0);
  const float scale =
      std::clamp(settings.value(prefix + "scale", 1.0).toFloat(), 1.0f, 3.0f);
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

  // Direct KMS recorders capture the primary scanout buffer, but hardware
  // cursor planes are separate and are not reliably composited by all drivers
  // (notably NVIDIA). Keep the cursor in the scene-rendered framebuffer on
  // real login outputs so monitor recording sees exactly what LunaDash shows.
  if (!self->nested)
    wlr_output_lock_software_cursors(output, true);

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
  self->updateNightLight();

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

  // Sample before commit: a screencopy frame is removed when this output
  // commit satisfies it. The tail bridges the small gap before xdpw queues
  // the next PipeWire frame and prevents a one-frame/frozen stream.
  const bool capturePending =
      screencopyPendingForOutput(state->impl->screencopy, state->output);
  if (capturePending)
    state->screencopyKeepalive = 6;
  else if (state->screencopyKeepalive > 0)
    --state->screencopyKeepalive;

  auto *animations = state->impl->q->windowAnimations_.get();
  if (animations)
    animations->advance();
  if (!ludash_night_color_commit(state->sceneOutput, state->nightColor)) {
    qWarning("wlroots scene output commit failed.");
    if (state->nightColor) {
      ludash_night_color_destroy(state->nightColor);
      state->nightColor = nullptr;
      state->nightTemperature = 6500;
      state->nightError = "The renderer rejected night light; the normal output has been restored.";
      wlr_output_schedule_frame(state->output);
    }
  }
  timespec now{};
  clock_gettime(CLOCK_MONOTONIC, &now);
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
    auto *impl = state->impl;
    auto *output = state->output;
    QTimer::singleShot(16, impl->q, [impl, output] {
      const bool alive =
          std::any_of(impl->outputs.cbegin(), impl->outputs.cend(),
                      [output](const auto *candidate) {
                        return candidate && candidate->output == output;
                      });
      if (alive)
        wlr_output_schedule_frame(output);
    });
  }
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
  if (self->pendingDisplay == state->output)
    self->clearPendingDisplay();
  detachListener(state->frame);
  detachListener(state->destroy);
  detachListener(state->requestState);
  if (self->primaryOutput == state->output)
    self->primaryOutput = nullptr;
  self->outputs.removeAll(state);
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
