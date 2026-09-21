#include "compositor/window/WindowTemplate.hpp"
#include "compositor/layout/FreeformLayout.hpp"
#include "compositor/tiling/TilingLayout.hpp"

namespace LunaDash {
namespace {
QJsonObject defaultWindowAnimation() {
  return {{"duration", -1},
          {"enterOffset", 12},
          {"focusOpacity", 0.82},
          {"exitScale", 0.90},
          {"easing", "outCubic"}};
}

const WindowTemplate &tilingTemplate() {
  static const WindowTemplate value{
      "tiling", WindowLayoutMode::Tiling, WindowPointerTemplate::Tiling,
      WindowActivationTemplate::ToggleMaximize, false,
      defaultWindowAnimation()};
  return value;
}

const WindowTemplate &stackingTemplate() {
  static const WindowTemplate value{
      "stacking", WindowLayoutMode::Stacking, WindowPointerTemplate::Freeform,
      WindowActivationTemplate::FocusOnly, true, defaultWindowAnimation()};
  return value;
}
} // namespace

QList<WindowTemplate> windowTemplates() {
  return {tilingTemplate(), stackingTemplate()};
}

const WindowTemplate &windowTemplateForKey(const QString &key) {
  if (key == "stacking")
    return stackingTemplate();
  return tilingTemplate();
}

std::unique_ptr<WindowLayout>
createWindowLayout(const WindowTemplate &windowTemplate) {
  switch (windowTemplate.layoutMode) {
  case WindowLayoutMode::Tiling:
    return std::make_unique<TilingLayout>();
  case WindowLayoutMode::Stacking:
    return std::make_unique<FreeformLayout>();
  }
  return nullptr;
}

} // namespace LunaDash
