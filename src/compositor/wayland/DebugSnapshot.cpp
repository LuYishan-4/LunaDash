// Diagnostics for `lunadashctl debug`: compositor state that `status` does not report,
// such as input focus, the cursor image, layer surfaces, helper processes and frame timing.
#include "compositor/wayland/Register.hpp"

#include "compositor/capture/ScreenCapture.hpp"
#include "compositor/client/ClientWindow.hpp"
#include "compositor/renderer/selection/RenderSelection.h"
#include "config/BuildConfig.hpp"

#include <filesystem>

namespace LunaDash {
namespace {
const char *layerName(zwlr_layer_shell_v1_layer layer) {
  switch (layer) {
  case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
    return "background";
  case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
    return "bottom";
  case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
    return "top";
  case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
    return "overlay";
  }
  return "unknown";
}

const char *keyboardInteractivity(uint32_t mode) {
  switch (mode) {
  case ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE:
    return "none";
  case ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE:
    return "exclusive";
  default:
    return "on-demand";
  }
}
} // namespace

Json WaylandCompositor::debugSnapshot() const {
  if (!d)
    return {{"error", "The compositor runtime is not running."}};

  // Who owns a surface: a window, a layer surface (shell), a popup or something else.
  const auto describe = [this](wlr_surface *surface) -> Json {
    if (!surface)
      return nullptr;
    auto *root = wlr_surface_get_root_surface(surface);
    if (auto *client = d->clientForSurface(root))
      return {{"kind", "window"}, {"id", client->id}, {"appId", client->appId},
              {"title", client->title}};
    for (const auto *layer : d->layers)
      if (layer->surface && layer->surface->surface == root)
        return {{"kind", "layer"},
                {"namespace", layer->surface->namespace_ ? layer->surface->namespace_ : ""},
                {"layer", layerName(layer->surface->current.layer)}};
    for (const auto *popup : d->xdgPopups)
      if (popup->popup && popup->popup->base->surface == root)
        return {{"kind", "popup"}};
    return {{"kind", "surface"}};
  };

  Json outputs = Json::array();
  for (const auto *state : d->outputs) {
    if (!state || !state->output)
      continue;
    const auto *output = state->output;
    outputs.push_back({
        {"name", output->name ? output->name : ""},
        {"enabled", output->enabled},
        {"width", output->width},
        {"height", output->height},
        {"refreshMilliHz", output->refresh},
        {"scale", output->scale},
        {"frames", state->frameCallbacks},
        {"slowFrames", state->slowFrames},
        {"lastFrameWorkUsec", state->lastFrameWorkUsec},
        {"maxFrameWorkUsec", state->maxFrameWorkUsec},
        {"softwareCursor", state->forceSoftwareCursor || state->softwareCursorLocked},
    });
  }

  Json keyboards = Json::array();
  for (const auto *state : d->keyboards)
    if (state && state->keyboard)
      keyboards.push_back({{"name", state->keyboard->base.name ? state->keyboard->base.name : ""},
                           {"virtual", state->virtualKeyboard}});

  Json layers = Json::array();
  for (const auto *state : d->layers) {
    if (!state || !state->surface)
      continue;
    const auto &current = state->surface->current;
    layers.push_back({
        {"namespace", state->surface->namespace_ ? state->surface->namespace_ : ""},
        {"layer", layerName(current.layer)},
        {"mapped", state->mapped},
        {"keyboard", keyboardInteractivity(current.keyboard_interactive)},
        {"width", current.actual_width},
        {"height", current.actual_height},
        {"exclusiveZone", current.exclusive_zone},
    });
  }

  Json processes = Json::array();
  for (const auto &process : processes_)
    if (process)
      processes.push_back({
          {"program", std::filesystem::path(process->program()).filename().string()},
          {"running", process->busy()},
          {"pid", process->busy() ? process->processId() : 0},
      });

  auto *seat = d->seat;
  const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::steady_clock::now() - d->startedAt);
  return {
      {"version", BuildConfig::version},
      {"commit", BuildConfig::commit},
      {"uptimeSeconds", uptime.count()},
      {"renderer", ludash_renderer_name(d->renderer)},
      {"outputs", outputs},
      {"input",
       {
           {"keyboards", keyboards},
           {"pointerDevices", d->pointerDevices},
           {"keyboardFocus", describe(seat ? seat->keyboard_state.focused_surface : nullptr)},
           {"pointerFocus", describe(seat ? seat->pointer_state.focused_surface : nullptr)},
           {"pointerGrab", seat && wlr_seat_pointer_has_grab(seat)},
           {"windowPointerOperation",
            d->pointerWindow ? (d->pointerResize ? "resize" : "move") : "none"},
           {"inputMethod", d->inputMethod != nullptr},
           {"textInputActive", d->activeTextInput != nullptr},
       }},
      {"cursor",
       {
           {"x", d->cursor ? d->cursor->x : 0.0},
           {"y", d->cursor ? d->cursor->y : 0.0},
           {"image", d->cursorImage},
           {"theme", d->cursorManager && d->cursorManager->name ? d->cursorManager->name : ""},
           {"size", d->cursorManager ? d->cursorManager->size : 0},
       }},
      {"layers", layers},
      {"windows",
       {
           {"count", clients_.size()},
           {"mapped", std::count_if(clients_.cbegin(), clients_.cend(),
                                    [](const auto &client) { return client->mapped; })},
       }},
      {"capture",
       {
           {"phase", screenCapture_ ? screenCapture_->phase() : "unavailable"},
           {"last", lastCapture_},
           {"error", captureError_},
       }},
      {"processes", processes},
      {"eventLoop",
       {
           {"dispatches", d->dispatchCalls},
           {"slowDispatches", d->slowDispatches},
           {"lastDispatchUsec", d->lastDispatchUsec},
           {"maxDispatchUsec", d->maxDispatchUsec},
       }},
      {"protocols",
       {
           {"cursorShape", d->cursorShapeManager != nullptr},
           {"fractionalScale", d->fractionalScale != nullptr},
           {"inputMethod", d->inputMethodManager != nullptr},
           {"textInput", d->textInputManager != nullptr},
           {"virtualKeyboard", d->virtualKeyboardManager != nullptr},
           {"idleInhibit", d->idleInhibit != nullptr},
           {"screencopy", d->screencopy != nullptr},
#if LUDASH_WLR_HAS_DATA_CONTROL
           {"dataControl", d->dataControl != nullptr},
#endif
#if LUDASH_WLR_HAS_DRM_SYNCOBJ
           {"explicitSync", d->explicitSync != nullptr},
#endif
#if LUDASH_WLR_HAS_XDG_ACTIVATION
           {"xdgActivation", d->xdgActivation != nullptr},
#endif
       }},
  };
}

} // namespace LunaDash
