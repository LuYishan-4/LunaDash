#pragma once

#include "compositor/layout/WindowLayout.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace LunaDash {

using WindowLayoutFactory = std::unique_ptr<WindowLayout> (*)();


struct WindowTemplate {
  std::string key;
  WindowLayoutFactory createLayout = nullptr;
  bool clientMoveResize = false;
  bool allowOverlap = false;
  bool activationTogglesMaximize = false;
  Json layoutSettingsSchema = Json::object();
  Json layoutSettingsDefaults = Json::object();
  Json layoutActions = Json::object();

  Json animation = Json::object();

  Json motion = Json::object();
  // Layout mode reported to the shell (state().layoutMode).
  std::string layoutMode;
  // Uses the desktop's gap preference as its default gap.
  bool sharesDesktopGap = false;
};

bool registerWindowTemplate(WindowTemplate windowTemplate, bool makeDefault = false,
                            std::string *error = nullptr);
std::vector<WindowTemplate> windowTemplates();
const WindowTemplate &defaultWindowTemplate();
// The template with this key, or the default template for an unknown/empty key.
const WindowTemplate &windowTemplateForKey(std::string_view key);



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

// Registers tiling and stacking (BuiltinWindowTemplates.cpp).
void registerBuiltinWindowTemplates();

} // namespace LunaDash
