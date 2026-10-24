#include "core/log/Log.hpp"

#include <cstdarg>
#include <cstdio>

namespace LunaDash {
namespace {
void write(const char *format, va_list arguments) {
  std::vfprintf(stderr, format, arguments);
  std::fputc('\n', stderr);
  std::fflush(stderr);
}
} // namespace

void logInfo(const char *format, ...) {
  va_list arguments;
  va_start(arguments, format);
  write(format, arguments);
  va_end(arguments);
}

void logWarning(const char *format, ...) {
  va_list arguments;
  va_start(arguments, format);
  write(format, arguments);
  va_end(arguments);
}

void logCritical(const char *format, ...) {
  va_list arguments;
  va_start(arguments, format);
  write(format, arguments);
  va_end(arguments);
}

} // namespace LunaDash
