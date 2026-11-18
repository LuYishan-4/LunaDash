#include "desktop/input/InputSettings.hpp"
#include "core/Defines.hpp"

#include <algorithm>
#include <array>
#include <mutex>
#include <string_view>
#include <unordered_map>

#if LUDASH_HAS_XKBREGISTRY
#include <xkbcommon/xkbregistry.h>
#endif

namespace LunaDash {
namespace {

std::string validatedLayout(std::string_view requested) {
#if LUDASH_HAS_XKBREGISTRY
  rxkb_context *context = rxkb_context_new(RXKB_CONTEXT_NO_FLAGS);
  if (!context)
    return "us";
  if (!rxkb_context_parse_default_ruleset(context)) {
    rxkb_context_unref(context);
    return "us";
  }

  std::string result = "us";
  for (rxkb_layout *layout = rxkb_layout_first(context); layout;
       layout = rxkb_layout_next(layout)) {
    const char *name = rxkb_layout_get_name(layout);
    const char *variant = rxkb_layout_get_variant(layout);
    if (name && !variant && requested == name) {
      result = name;
      break;
    }
  }
  rxkb_context_unref(context);
  return result;
#else
  static constexpr std::array<std::string_view, 5> supported{
      "us", "gb", "de", "fr", "es"};
  return std::find(supported.begin(), supported.end(), requested) !=
                 supported.end()
             ? std::string(requested)
             : "us";
#endif
}

} // namespace

std::string keyboardLayoutPreference(const std::string &requested) {
  // Validating parses the whole XKB rules registry (~10 ms) and every status
  // reply asks: remember each answer. The installed layouts do not change
  // while the session runs.
  static std::mutex mutex;
  static std::unordered_map<std::string, std::string> known;
  const std::string key = requested.empty() ? std::string("us") : requested;
  std::lock_guard lock(mutex);
  const auto found = known.find(key);
  if (found != known.end())
    return found->second;
  return known.emplace(key, validatedLayout(key)).first->second;
}

int keyboardRepeatRate() { return 25; }
int keyboardRepeatDelay() { return 600; }

} // namespace LunaDash
