#include "desktop/system/SystemMetrics.h"
#include <string.h>
static int ludash_parse_counter(const char **cursor, const char *end,
                                uint64_t *output) {
  const char *current = *cursor;
  while (current < end && (*current == ' ' || *current == '\t'))
    ++current;
  if (current == end || *current < '0' || *current > '9')
    return 0;
  uint64_t result = 0;
  while (current < end && *current >= '0' && *current <= '9') {
    const unsigned int digit = (unsigned int)(*current - '0');
    if (result > (UINT64_MAX - digit) / 10)
      return 0;
    result = result * 10 + digit;
    ++current;
  }
  if (current < end && *current != ' ' && *current != '\t' &&
      *current != '\n' && *current != '\r')
    return 0;
  *cursor = current;
  *output = result;
  return 1;
}
int ludash_parse_cpu(const char *text, size_t length,
                     LuDashCpuCounters *output) {
  if (!text || !output || length < 5 || length > 128 * 1024 ||
      memcmp(text, "cpu", 3) || (text[3] != ' ' && text[3] != '\t'))
    return 0;
  const char *end = memchr(text, '\n', length);
  if (!end)
    end = text + length;
  const char *cursor = text + 3;
  LuDashCpuCounters result = {0, 0};
  size_t fields = 0;
  while (fields < 8) {
    while (cursor < end && (*cursor == ' ' || *cursor == '\t'))
      ++cursor;
    if (cursor == end)
      break;
    uint64_t value = 0;
    if (!ludash_parse_counter(&cursor, end, &value) ||
        result.total > UINT64_MAX - value)
      return 0;
    result.total += value;
    if (fields == 3 || fields == 4) {
      if (result.idle > UINT64_MAX - value)
        return 0;
      result.idle += value;
    }
    ++fields;
  }
  if (fields < 4)
    return 0;
  *output = result;
  return 1;
}
int ludash_parse_memory(const char *text, size_t length,
                        LuDashMemoryCounters *output) {
  if (!text || !output || length > 128 * 1024)
    return 0;
  const char *cursor = text;
  const char *end = text + length;
  LuDashMemoryCounters result = {0, 0};
  int found_total = 0, found_available = 0;
  while (cursor < end) {
    const char *line_end = memchr(cursor, '\n', (size_t)(end - cursor));
    if (!line_end)
      line_end = end;
    const size_t line_length = (size_t)(line_end - cursor);
    const char *value = NULL;
    uint64_t *destination = NULL;
    if (line_length >= 9 && !memcmp(cursor, "MemTotal:", 9)) {
      value = cursor + 9;
      destination = &result.total_kib;
      found_total = 1;
    }
    if (line_length >= 13 && !memcmp(cursor, "MemAvailable:", 13)) {
      value = cursor + 13;
      destination = &result.available_kib;
      found_available = 1;
    }
    if (destination) {
      if (!ludash_parse_counter(&value, line_end, destination))
        return 0;
      while (value < line_end && (*value == ' ' || *value == '\t'))
        ++value;
      if (line_end - value < 2 || memcmp(value, "kB", 2))
        return 0;
      value += 2;
      while (value < line_end &&
             (*value == ' ' || *value == '\t' || *value == '\r'))
        ++value;
      if (value != line_end)
        return 0;
    }
    cursor = line_end < end ? line_end + 1 : end;
  }
  if (!found_total || !found_available || !result.total_kib)
    return 0;
  *output = result;
  return 1;
}
int ludash_cpu_percent(LuDashCpuCounters previous, LuDashCpuCounters current) {
  if (!previous.total || current.total <= previous.total ||
      current.idle < previous.idle)
    return 0;
  const uint64_t total = current.total - previous.total,
                 idle = current.idle - previous.idle;
  if (idle > total)
    return 0;
  return (int)(100.0 * (double)(total - idle) / (double)total);
}
uint64_t ludash_memory_used(LuDashMemoryCounters memory) {
  return memory.available_kib < memory.total_kib
             ? memory.total_kib - memory.available_kib
             : 0;
}
int ludash_memory_percent(LuDashMemoryCounters memory) {
  return memory.total_kib ? (int)(100.0 * (double)ludash_memory_used(memory) /
                                  (double)memory.total_kib)
                          : 0;
}
