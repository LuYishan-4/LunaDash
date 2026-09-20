#include "compositor/layout/WindowLayout.hpp"
#include "compositor/tiling/TilingLayout.hpp"

namespace LunaDash {
QList<WindowLayoutTemplate> windowLayoutTemplates() {
  return {{WindowLayoutMode::Tiling, "tiling", true},
          {WindowLayoutMode::Stacking, "stacking", false}};
}
std::unique_ptr<WindowLayout> createWindowLayout(WindowLayoutMode mode) {
  switch (mode) {
  case WindowLayoutMode::Tiling:
    return std::make_unique<TilingLayout>();
  case WindowLayoutMode::Stacking:
    return nullptr;
  }
  return nullptr;
}
} // namespace LunaDash
