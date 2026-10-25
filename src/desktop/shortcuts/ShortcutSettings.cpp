#include "desktop/shortcuts/ShortcutSettings.hpp"

#include "config/desktop/DesktopPreferenceStore.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <string_view>

namespace LunaDash {
namespace {

nlohmann::json defaults() {
  nlohmann::json result{
      {"focusLeft", "Meta+H"},         {"focusRight", "Meta+L"},
      {"focusUp", "Meta+K"},           {"focusDown", "Meta+J"},
      {"groupLeft", "Meta+Shift+H"},   {"groupRight", "Meta+Shift+L"},
      {"reorderLeft", "Meta+Ctrl+H"},  {"reorderRight", "Meta+Ctrl+L"},
      {"expelWindow", "Meta+Shift+E"}, {"centerColumn", "Meta+Shift+C"},
      {"widenColumn", "Meta+="},       {"narrowColumn", "Meta+-"},
      {"maximizeWindow", "Meta+F"},    {"closeWindow", "Meta+C"},
      {"minimizeWindow", "Meta+M"},    {"launchTerminal", "Meta+T"},
      {"launchFiles", "Meta+E"},       {"launchLauncher", "Meta+D"},
      {"screenshot", "Meta+Shift+S"},  {"launchOrbit", "Meta+A"},
      {"chooseWallpaper", "Meta+W"},   {"randomWallpaper", "Meta+Ctrl+W"},
      {"toggleEyeCare", "Meta+N"},     {"toggleScratchpad", "Meta+grave"},
      {"openControlCenter", "Meta+I"}, {"openClipboard", "Meta+V"},
      {"openPowerMenu", "Meta+X"},     {"toggleFullscreen", "Meta+Shift+F"},
      {"toggleTilingMode", "Meta+Shift+Space"}};
  for (int workspace = 1; workspace <= 10; ++workspace) {
    const auto number = std::to_string(workspace % 10);
    result["workspace" + std::to_string(workspace)] = "Meta+" + number;
    result["moveToWorkspace" + std::to_string(workspace)] =
        "Meta+Shift+" + number;
  }
  return result;
}

xkb_keysym_t keySym(std::string_view name) {
  if (name == "=")
    return XKB_KEY_equal;
  if (name == "-")
    return XKB_KEY_minus;
  std::string key(name);
  std::transform(key.begin(), key.end(), key.begin(), [](unsigned char value) {
    return static_cast<char>(std::tolower(value));
  });
  if (key == "space")
    return XKB_KEY_space;
  if (key == "return")
    return XKB_KEY_Return;
  return xkb_keysym_from_name(key.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
}

struct ParsedShortcut {
  xkb_keysym_t symbol = XKB_KEY_NoSymbol;
  uint32_t modifiers = 0;
  std::string canonical;
};

ParsedShortcut parseShortcut(std::string_view text) {
  if (text == "Disabled")
    return {XKB_KEY_NoSymbol, 0, "Disabled"};

  uint32_t modifiers = 0;
  std::string key;
  std::size_t start = 0;
  while (start <= text.size()) {
    const auto separator = text.find('+', start);
    const auto piece = text.substr(start, separator == std::string_view::npos
                                              ? text.size() - start
                                              : separator - start);
    if (piece == "Meta")
      modifiers |= ShortcutMeta;
    else if (piece == "Ctrl" || piece == "Control")
      modifiers |= ShortcutControl;
    else if (piece == "Alt")
      modifiers |= ShortcutAlt;
    else if (piece == "Shift")
      modifiers |= ShortcutShift;
    else if (key.empty())
      key = piece;
    else
      return {};
    if (separator == std::string_view::npos)
      break;
    start = separator + 1;
  }

  if (key.empty() || !(modifiers & (ShortcutMeta | ShortcutAlt)))
    return {};

  const xkb_keysym_t symbol = keySym(key);
  if (symbol == XKB_KEY_NoSymbol)
    return {};

  std::string canonicalKey = key;
  if (key.size() == 1 &&
      std::isalpha(static_cast<unsigned char>(key.front()))) {
    canonicalKey.front() = static_cast<char>(
        std::toupper(static_cast<unsigned char>(canonicalKey.front())));
  } else if (symbol == XKB_KEY_space) {
    canonicalKey = "Space";
  } else if (symbol == XKB_KEY_Return) {
    canonicalKey = "Return";
  }

  std::string canonical;
  const auto append = [&canonical](std::string_view value) {
    if (!canonical.empty())
      canonical += '+';
    canonical.append(value);
  };
  if (modifiers & ShortcutMeta)
    append("Meta");
  if (modifiers & ShortcutControl)
    append("Ctrl");
  if (modifiers & ShortcutAlt)
    append("Alt");
  if (modifiers & ShortcutShift)
    append("Shift");
  append(canonicalKey);
  return {symbol, modifiers, std::move(canonical)};
}

bool reservedWindowSwitchKey(const ParsedShortcut &shortcut) {
  return (shortcut.symbol == XKB_KEY_Tab ||
          shortcut.symbol == XKB_KEY_ISO_Left_Tab) &&
         (shortcut.modifiers == ShortcutAlt ||
          shortcut.modifiers == (ShortcutAlt | ShortcutShift) ||
          shortcut.modifiers == ShortcutMeta ||
          shortcut.modifiers == (ShortcutMeta | ShortcutShift));
}

} // namespace

ShortcutSettings::ShortcutSettings()
    : ShortcutSettings(nlohmann::json::object()) {}

ShortcutSettings::ShortcutSettings(nlohmann::json legacyBindings)
    : bindings_(defaults()) {
  if (!hasStoredShortcutBindings() && legacyBindings.is_object()) {
    std::string error;
    importLegacyShortcutBindings(legacyBindings, &error);
  }

  auto configured = loadShortcutBindings();
  const auto knownActions = defaults();
  bool removedObsolete = false;
  if (configured.is_object()) {
    for (auto it = configured.begin(); it != configured.end();) {
      if (!knownActions.contains(it.key())) {
        it = configured.erase(it);
        removedObsolete = true;
      } else {
        ++it;
      }
    }
  } else {
    configured = nlohmann::json::object();
  }
  if (removedObsolete) {
    std::string error;
    saveShortcutBindings(configured, &error);
  }

  const auto uses = [&configured](std::string_view sequence,
                                  std::string_view except) {
    for (const auto &[key, value] : configured.items())
      if (key != except && value.is_string() &&
          value.get_ref<const std::string &>() == sequence)
        return true;
    return false;
  };
  const auto configuredValue = [&configured](std::string_view key) {
    const auto it = configured.find(std::string(key));
    return it != configured.end() && it->is_string() ? it->get<std::string>()
                                                     : std::string{};
  };
  if (configuredValue("launchTerminal") == "Meta+Return" &&
      !uses("Meta+T", "launchTerminal"))
    configured["launchTerminal"] = "Meta+T";
  if (!configured.contains("launchTerminal") &&
      uses("Meta+T", "launchTerminal"))
    bindings_["launchTerminal"] = "Disabled";
  if (!configured.contains("workspace10") && uses("Meta+0", "workspace10"))
    bindings_["workspace10"] = "Disabled";
  if (!configured.contains("moveToWorkspace10") &&
      uses("Meta+Shift+0", "moveToWorkspace10"))
    bindings_["moveToWorkspace10"] = "Disabled";
  for (auto &[key, value] : configured.items()) {
    (void)key;
    if (!value.is_string())
      continue;
    const auto binding = parseShortcut(value.get_ref<const std::string &>());
    if (reservedWindowSwitchKey(binding))
      value = "Disabled";
  }

  // Migrate the previous shipped default without replacing custom bindings
  // or stealing Meta+Shift+S from another explicitly assigned action.
  if (configuredValue("screenshot") == "Alt+Shift+F5") {
    bool used = false;
    for (const auto &[key, value] : configured.items())
      used |= key != "screenshot" && value.is_string() &&
              value.get_ref<const std::string &>() == "Meta+Shift+S";
    if (!used)
      configured["screenshot"] = "Meta+Shift+S";
  }
  // New defaults never invalidate a user's existing custom bindings.
  for (const auto &[key, value] : bindings_.items())
    if (!configured.contains(key) && value.is_string() &&
        uses(value.get_ref<const std::string &>(), key))
      bindings_[key] = "Disabled";

  std::string error;
  if (!configured.empty())
    apply(configured, &error);
}

nlohmann::json ShortcutSettings::snapshot() const { return bindings_; }

std::string ShortcutSettings::actionFor(xkb_keysym_t keysym,
                                        uint32_t modifiers) const {
  for (const auto &[action, value] : bindings_.items()) {
    if (!value.is_string())
      continue;
    const ParsedShortcut shortcut =
        parseShortcut(value.get_ref<const std::string &>());
    if (shortcut.canonical != "Disabled" &&
        xkb_keysym_to_lower(shortcut.symbol) == xkb_keysym_to_lower(keysym) &&
        shortcut.modifiers == modifiers)
      return action;
  }
  return {};
}

bool ShortcutSettings::apply(const nlohmann::json &changes,
                             std::string *error) {
  if (!changes.is_object()) {
    if (error)
      *error = "Expected shortcut object.";
    return false;
  }
  auto candidate = bindings_;
  const auto known = defaults();
  for (const auto &[action, value] : changes.items()) {
    if (!known.contains(action) || !value.is_string()) {
      if (error)
        *error = "Unknown shortcut action: " + action;
      return false;
    }
    const ParsedShortcut parsed =
        parseShortcut(value.get_ref<const std::string &>());
    if (parsed.canonical.empty()) {
      if (error)
        *error = "Shortcut must be one Meta or Alt key combination.";
      return false;
    }
    if (reservedWindowSwitchKey(parsed)) {
      if (error)
        *error = "Alt+Tab and Super+Tab are reserved for window and workspace "
                 "switching.";
      return false;
    }
    candidate[action] = parsed.canonical;
  }

  std::set<std::string> used;
  for (const auto &[action, value] : candidate.items()) {
    (void)action;
    if (!value.is_string())
      continue;
    const auto &sequence = value.get_ref<const std::string &>();
    if (sequence == "Disabled")
      continue;
    if (!used.insert(sequence).second) {
      if (error)
        *error = "Shortcut is already assigned: " + sequence;
      return false;
    }
  }

  std::string nativeError;
  if (!saveShortcutBindings(candidate, &nativeError)) {
    if (error)
      *error = std::move(nativeError);
    return false;
  }
  bindings_ = std::move(candidate);
  return true;
}

bool ShortcutSettings::reset(std::string *error) {
  if (!saveShortcutBindings(nlohmann::json::object(), error))
    return false;
  bindings_ = defaults();
  return true;
}

} // namespace LunaDash
