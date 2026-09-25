#include "core/plugins/PluginApi.h"
#include <limits.h>
#include <string.h>

int ludash_plugin_write_json(const char *json, char *response,
                             size_t capacity) {
  if (!json || !response)
    return -1;
  const size_t length = strlen(json);
  if (length >= capacity || length > INT_MAX)
    return -1;
  memcpy(response, json, length + 1);
  return (int)length;
}
