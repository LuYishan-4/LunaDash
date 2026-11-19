#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <xkbcommon/xkbcommon.h>

namespace LunaDash {

enum KeyModifier : uint32_t {
  KeyMeta = 1u << 0,
  KeyControl = 1u << 1,
  KeyAlt = 1u << 2,
  KeyShift = 1u << 3,
};

// "Meta+Shift+S": a key and its modifiers, written canonically
// (modifiers in Meta, Ctrl, Alt, Shift order; letters upper case).
struct KeyCombo {
  xkb_keysym_t symbol = XKB_KEY_NoSymbol;
  uint32_t modifiers = 0;
  std::string canonical;
};
std::optional<KeyCombo> parseKeyCombo(std::string_view text);

// Keys bound to actions, as niri binds them: each key runs an action written
// with its arguments ("close-window", "focus-workspace 3", "spawn kitty").
// Bindings are grouped by mode: "normal", and "overview", which is tried
// first while the overview is open. Stored in desktop-preferences.json under
// "keybindings"; the defaults apply until something is changed.
class KeyBindings {
public:
  // Whether an action (with its arguments) exists; the compositor knows.
  using Validator = std::function<bool(std::string_view action, std::string *error)>;

  explicit KeyBindings(Validator validate);

  static const std::vector<std::string> &modes();

  // {"normal": {"Meta+Q": "close-window", …}, "overview": {…}}
  const nlohmann::json &snapshot() const { return bindings_; }
  // The action bound to the key in `mode`, or "".
  std::string actionFor(std::string_view mode, xkb_keysym_t symbol,
                        uint32_t modifiers) const;
  // Binds `keys` to `action`, removing the binding of `replace` (the keys it
  // had before) when given.
  bool bind(std::string_view mode, std::string_view keys,
            std::string_view action, std::string_view replace,
            std::string *error);
  bool unbind(std::string_view mode, std::string_view keys, std::string *error);
  // Back to the defaults.
  bool reset(std::string *error);

private:
  struct Entry {
    std::string mode;
    KeyCombo combo;
    std::string action;
  };

  bool store(nlohmann::json next, std::string *error);
  void index();

  Validator validate_;
  nlohmann::json bindings_;
  std::vector<Entry> entries_;
};

} // namespace LunaDash
