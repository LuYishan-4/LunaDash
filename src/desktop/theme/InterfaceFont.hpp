#pragma once

#include <string>
#include <string_view>

namespace LunaDash {

// The family of a Pango font description such as "Adwaita Sans 11" or
// "Noto Sans CJK TC, Bold 10": its first family, without style words and size.
std::string fontDescriptionFamily(std::string_view description);

// The font LunaDash's own windows use: the desktop's font for the generic family
// chosen in the LunaDash fontFamily preference (sans-serif → the interface font,
// monospace → the monospace font), read from the XDG desktop portal or GSettings.
// Falls back to the generic name, which fontconfig resolves.
std::string desktopFontFamily();

} // namespace LunaDash
