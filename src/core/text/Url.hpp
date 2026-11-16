#pragma once

#include <string>
#include <string_view>

namespace LunaDash {

// file:// URL for an absolute local path, percent-encoding reserved bytes.
std::string fileUrl(std::string_view path);
// file:// URI for a local path as GLib spells it (g_filename_to_uri), or an
// empty string when the path cannot be represented.
std::string localFileUri(const std::string &path);
// Local path for a file:// URL, or an empty string for other schemes.
std::string localPathFromFileUrl(std::string_view url);
std::string percentEncode(std::string_view text, std::string_view keep = {});
std::string percentDecode(std::string_view text);
// True for an absolute https:// URL with a host and no whitespace or
// control characters.
bool isHttpsUrl(std::string_view url);
// Scheme of an absolute URL in lower case, or an empty string.
std::string urlScheme(std::string_view url);

} // namespace LunaDash
