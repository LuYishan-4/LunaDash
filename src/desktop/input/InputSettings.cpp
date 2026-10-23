#include "desktop/input/InputSettings.hpp"
#include "core/Defines.hpp"

#include <algorithm>
#include <array>
#include <string_view>

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
  return validatedLayout(requested.empty() ? std::string_view("us")
                                           : std::string_view(requested));
}

int keyboardRepeatRate() { return 25; }
int keyboardRepeatDelay() { return 600; }

} // namespace LunaDash
