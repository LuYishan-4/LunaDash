#pragma once

#include "compositor/layout/WindowLayout.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace LunaDash {

using WindowLayoutFactory = std::unique_ptr<WindowLayout> (*)();

// A window-management policy: a layout plus the settings, actions and
// animation profile the compositor exposes for it. Templates are registered
// (registerWindowTemplate); the compositor only looks them up.
struct WindowTemplate {
  std::string key;
  WindowLayoutFactory createLayout = nullptr;
  bool clientMoveResize = false;
  bool allowOverlap = false;
  bool activationTogglesMaximize = false;
  Json layoutSettingsSchema = Json::object();
  Json layoutSettingsDefaults = Json::object();
  Json layoutActions = Json::object();
  // Window-animation profile (the window-animation plugin target's settings).
  Json animation = Json::object();
  // Springs and timings of the scene animation, not exposed to plugins:
  // openScale, closeDuration and {stiffness, dampingRatio} objects for
  // windowOpen, windowMovement, windowResize and workspaceSwitch.
  Json motion = Json::object();
  // Layout mode reported to the shell (state().layoutMode).
  std::string layoutMode;
  // Templates sharing a variant preference are alternatives of one layout: the
  // desktop preference named here holds the chosen `variant`.
  std::string variantPreference;
  std::string variant;
  // Uses the desktop's gap preference as its default gap.
  bool sharesDesktopGap = false;
};

// Registry. The built-in templates (BuiltinWindowTemplates.cpp) are registered
// before the first lookup; the first registered default wins.
bool registerWindowTemplate(WindowTemplate windowTemplate, bool makeDefault = false,
                            std::string *error = nullptr);
std::vector<WindowTemplate> windowTemplates();
const WindowTemplate &defaultWindowTemplate();
// The template with this key, or the default template for an unknown/empty key.
const WindowTemplate &windowTemplateForKey(std::string_view key);
// The requested template, replaced by the variant `preferences` selects when it
// belongs to a variant group.
const WindowTemplate &resolveWindowTemplate(std::string_view key,
                                            const Json &preferences);
// Variants registered for a variant preference, in registration order.
std::vector<std::string> windowTemplateVariants(std::string_view variantPreference);

// Authoring helpers for template definitions.
Json windowLayoutIntegerParameter(int minimum, int maximum, int value = 0);
Json windowLayoutAction(const std::string &label, Json parameters,
                        bool requiresArea = false, bool cardinalDirection = false);
Json windowTemplateDefaults(const Json &schema);

std::unique_ptr<WindowLayout>
createWindowLayout(const WindowTemplate &windowTemplate);
bool validateWindowLayoutSettings(const WindowTemplate &windowTemplate,
                                  const Json &settings,
                                  std::string *error = nullptr);
bool windowTemplateSupportsAction(const WindowTemplate &windowTemplate,
                                  const std::string &action);
bool performWindowLayoutAction(WindowLayout &layout,
                               const WindowTemplate &windowTemplate,
                               const std::string &action,
                               const Json &payload = {});

// Registers tiling, stacking and scrolling (BuiltinWindowTemplates.cpp).
void registerBuiltinWindowTemplates();

} // namespace LunaDash
