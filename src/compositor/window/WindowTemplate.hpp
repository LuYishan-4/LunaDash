#pragma once

#include "compositor/layout/WindowLayout.hpp"

#include <memory>
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
};

std::vector<WindowTemplate> windowTemplates();
const WindowTemplate &windowTemplateForKey(const std::string &key);
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

} // namespace LunaDash
