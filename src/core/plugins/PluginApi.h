#ifndef LUDASH_PLUGIN_API_H
#define LUDASH_PLUGIN_API_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
namespace LunaDash {
extern "C" {
#endif
#define LUDASH_PLUGIN_API_VERSION 2u
#define LUDASH_PLUGIN_EXPORT __attribute__((visibility("default")))

/* UTF-8 JSON, borrowed request and caller-owned response. Return bytes written,
 * excluding the NUL, or a negative value on failure. No Qt/wlroots pointers
 * cross this ABI. Calls occur on the compositor thread; never block. */
typedef struct ludash_plugin_api {
  uint32_t struct_size;
  uint32_t api_version;
  const char *metadata_json;
  int (*process)(const char *request, char *response, size_t capacity);
} ludash_plugin_api;

/* The author implements process; the SDK generates entry_v2 with metadata. */
int ludash_plugin_process(const char *request, char *response, size_t capacity);
LUDASH_PLUGIN_EXPORT const ludash_plugin_api *ludash_plugin_entry_v2(void);
int ludash_plugin_write_json(const char *json, char *response, size_t capacity);
#ifdef __cplusplus
}
} // namespace LunaDash
#endif
#endif
