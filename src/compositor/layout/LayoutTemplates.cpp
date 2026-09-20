#include "compositor/layout/WindowLayout.hpp"
#include "compositor/tiling/TilingLayout.hpp"
#include "compositor/stacking/StackingLayout.hpp"

namespace LunaDash {
QList<WindowLayoutTemplate> windowLayoutTemplates() {
  return {{WindowLayoutMode::Tiling, "tiling", true},
          {WindowLayoutMode::Stacking, "stacking", true}};
}
std::unique_ptr<WindowLayout> createWindowLayout(WindowLayoutMode mode) {
  switch (mode) {
  case WindowLayoutMode::Tiling:
    return std::make_unique<TilingLayout>();
  case WindowLayoutMode::Stacking:
    return std::make_unique<StackingLayout>();
  }
  return nullptr;
}
} // namespace LunaDash
