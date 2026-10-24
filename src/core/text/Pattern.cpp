#include "core/text/Pattern.hpp"

#include <glib.h>

namespace LunaDash {
namespace {
GRegex *compile(const std::string &pattern) {
  GError *error = nullptr;
  GRegex *regex = g_regex_new(pattern.c_str(), static_cast<GRegexCompileFlags>(0),
                              static_cast<GRegexMatchFlags>(0), &error);
  if (error)
    g_error_free(error);
  return regex;
}
} // namespace

bool patternIsValid(const std::string &pattern) {
  GRegex *regex = compile(pattern);
  if (!regex)
    return false;
  g_regex_unref(regex);
  return true;
}

bool patternMatches(const std::string &pattern, const std::string &subject,
                    bool *valid) {
  GRegex *regex = compile(pattern);
  if (valid)
    *valid = regex != nullptr;
  if (!regex)
    return false;
  const bool matched =
      g_regex_match_full(regex, subject.c_str(),
                         static_cast<gssize>(subject.size()), 0,
                         static_cast<GRegexMatchFlags>(0), nullptr, nullptr);
  g_regex_unref(regex);
  return matched;
}

} // namespace LunaDash
