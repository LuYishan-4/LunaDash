#pragma once

namespace LunaDash {

// printf-style diagnostics written to standard error, one line per call.
void logInfo(const char *format, ...) __attribute__((format(printf, 1, 2)));
void logWarning(const char *format, ...) __attribute__((format(printf, 1, 2)));
void logCritical(const char *format, ...) __attribute__((format(printf, 1, 2)));

} // namespace LunaDash
