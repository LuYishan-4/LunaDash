#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <xkbcommon/xkbcommon.h>

namespace LunaDash {

enum ShortcutModifier : uint32_t {
  ShortcutMeta = 1u << 0,
  ShortcutControl = 1u << 1,
  ShortcutAlt = 1u << 2,
  ShortcutShift = 1u << 3,
};

class ShortcutSettings {
public:
  ShortcutSettings();

  explicit ShortcutSettings(nlohmann::json legacyBindings);

  nlohmann::json snapshot() const;
  std::string actionFor(xkb_keysym_t keysym, uint32_t modifiers) const;
  bool apply(const nlohmann::json &changes, std::string *error);
  bool reset(std::string *error);

private:
  nlohmann::json bindings_;
};

} // namespace LunaDash
