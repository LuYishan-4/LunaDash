#include "core/plugins/PluginApi.h"

int ludash_plugin_process(const char *request, char *response, size_t capacity) {
    (void)request;
    /* A C implementation can parse request with its preferred JSON library.
       This complete profile works for replacement and augmentation. */
    return ludash_plugin_write_json(
        "{\"duration\":280,\"enterOffset\":20,\"focusOpacity\":0.9,"
        "\"exitScale\":0.95,\"easing\":\"outQuint\"}", response, capacity);
}
