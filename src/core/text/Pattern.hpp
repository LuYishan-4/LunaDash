#pragma once

#include <string>

namespace LunaDash {

// Perl-compatible regular expressions (the dialect used by settings schemas
// and plugin manifests), backed by GLib's PCRE2 engine. Matching searches the
// whole subject; anchor the pattern to require a full match.
bool patternIsValid(const std::string &pattern);
bool patternMatches(const std::string &pattern, const std::string &subject,
                    bool *valid = nullptr);

} // namespace LunaDash
