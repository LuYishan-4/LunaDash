#include "service/portal/FileChooserPortal.hpp"

#include "service/portal/FileChooserOptions.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace LunaDash {
namespace {

using Json = nlohmann::json;
namespace fs = std::filesystem;

constexpr char kDesktopPath[] = "/org/freedesktop/portal/desktop";
constexpr char kFileChooserInterface[] =
    "org.freedesktop.impl.portal.FileChooser";
constexpr char kRequestInterface[] = "org.freedesktop.impl.portal.Request";
constexpr char kFailedError[] = "org.freedesktop.portal.Error.Failed";

struct Filter {
  std::string label;
};

struct PickerOperation {
  ~PickerOperation() {
    if (timeout)
      g_source_remove(timeout);
    if (requestRegistration && connection)
      g_dbus_connection_unregister_object(connection, requestRegistration);
    if (process)
      g_object_unref(process);
    if (invocation)
      g_object_unref(invocation);
    if (introspection)
      g_dbus_node_info_unref(introspection);
    if (connection)
      g_object_unref(connection);
    if (options)
      g_variant_unref(options);
  }

  GDBusConnection *connection = nullptr;
  GDBusNodeInfo *introspection = nullptr;
  GDBusMethodInvocation *invocation = nullptr;
  GSubprocess *process = nullptr;
  guint requestRegistration = 0;
  guint timeout = 0;
  bool cancelled = false;
  bool timedOut = false;
  std::string requestPath;
  std::string requestText;
  std::string method;
  std::string mode;
  GVariant *options = nullptr;
};

void returnPortalResponse(GDBusMethodInvocation *invocation, guint response,
                          GVariant *results) {
  g_dbus_method_invocation_return_value(
      invocation, g_variant_new("(u@a{sv})", response,
                                results ? results
                                        : g_variant_new_array(
                                              G_VARIANT_TYPE("{sv}"), nullptr,
                                              0)));
}

std::string stringOption(GVariant *options, const char *key) {
  const gchar *value = nullptr;
  return g_variant_lookup(options, key, "&s", &value) && value ? value : "";
}

bool booleanOption(GVariant *options, const char *key) {
  gboolean value = FALSE;
  return g_variant_lookup(options, key, "b", &value) && value;
}

std::string bytesOption(GVariant *options, const char *key) {
  GVariant *value = g_variant_lookup_value(options, key, G_VARIANT_TYPE("ay"));
  if (!value)
    return {};
  gsize length = 0;
  const auto *bytes = static_cast<const guint8 *>(
      g_variant_get_fixed_array(value, &length, sizeof(guint8)));
  if (length > 0 && bytes[length - 1] == 0)
    --length;
  gchar *encoded = g_base64_encode(bytes, length);
  std::string result = encoded ? encoded : "";
  g_free(encoded);
  g_variant_unref(value);
  return result;
}

void appendPattern(Json &patterns, std::string_view value) {
  if (value.empty() || !patterns.is_array())
    return;
  for (const auto &existing : patterns)
    if (existing.is_string() && existing.get_ref<const std::string &>() == value)
      return;
  patterns.emplace_back(value);
}

void appendMimeType(Json &mimeTypes, std::string_view value) {
  if (value.empty() || !mimeTypes.is_array())
    return;
  for (const auto &existing : mimeTypes)
    if (existing.is_string() && existing.get_ref<const std::string &>() == value)
      return;
  mimeTypes.emplace_back(value);
}

Json filtersOption(GVariant *options, int &selectedFilter) {
  Json filters = Json::array();
  selectedFilter = -1;
  GVariant *value = g_variant_lookup_value(
      options, "filters", G_VARIANT_TYPE("a(sa(us))"));
  if (!value)
    return filters;

  std::string currentLabel;
  GVariant *current = g_variant_lookup_value(
      options, "current_filter", G_VARIANT_TYPE("(sa(us))"));
  if (current) {
    const gchar *label = nullptr;
    GVariant *rules = nullptr;
    g_variant_get(current, "(&s@a(us))", &label, &rules);
    if (label)
      currentLabel = label;
    g_variant_unref(rules);
    g_variant_unref(current);
  }

  GVariantIter iterator;
  g_variant_iter_init(&iterator, value);
  const gchar *label = nullptr;
  GVariant *rules = nullptr;
  while (g_variant_iter_next(&iterator, "(&s@a(us))", &label, &rules)) {
    Json patterns = Json::array();
    Json mimeTypes = Json::array();
    GVariantIter ruleIterator;
    g_variant_iter_init(&ruleIterator, rules);
    guint type = 0;
    const gchar *ruleValue = nullptr;
    while (g_variant_iter_next(&ruleIterator, "(u&s)", &type, &ruleValue)) {
      if (!ruleValue)
        continue;
      if (type == 0)
        appendPattern(patterns, ruleValue);
      else if (type == 1)
        appendMimeType(mimeTypes, ruleValue);
    }
    if (label && currentLabel == label)
      selectedFilter = static_cast<int>(filters.size());
    filters.push_back({{"label", label ? label : ""},
                       {"patterns", std::move(patterns)},
                       {"mime-types", std::move(mimeTypes)}});
    g_variant_unref(rules);
  }
  g_variant_unref(value);
  if (selectedFilter < 0 && !filters.empty())
    selectedFilter = 0;
  return filters;
}

Json choicesOption(GVariant *options) {
  Json choices = Json::array();
  GVariant *value = g_variant_lookup_value(
      options, "choices", G_VARIANT_TYPE("a(ssa(ss)s)"));
  if (!value)
    return choices;

  GVariantIter iterator;
  g_variant_iter_init(&iterator, value);
  const gchar *id = nullptr;
  const gchar *label = nullptr;
  const gchar *selected = nullptr;
  GVariant *values = nullptr;
  while (g_variant_iter_next(&iterator, "(&s&s@a(ss)&s)", &id, &label,
                             &values, &selected)) {
    Json optionsJson = Json::array();
    GVariantIter optionIterator;
    g_variant_iter_init(&optionIterator, values);
    const gchar *optionId = nullptr;
    const gchar *optionLabel = nullptr;
    while (g_variant_iter_next(&optionIterator, "(&s&s)", &optionId,
                               &optionLabel))
      optionsJson.push_back({{"id", optionId ? optionId : ""},
                             {"label", optionLabel ? optionLabel : ""}});
    choices.push_back({{"id", id ? id : ""},
                       {"label", label ? label : ""},
                       {"selected", selected ? selected : ""},
                       {"values", std::move(optionsJson)}});
    g_variant_unref(values);
  }
  g_variant_unref(value);
  return choices;
}

Json pickerRequest(const std::string &mode, const std::string &title,
                   GVariant *options) {
  int filterIndex = -1;
  Json request{{"mode", mode},
               {"title", title},
               {"multiple", booleanOption(options, "multiple")},
               {"current-folder", bytesOption(options, "current_folder")},
               {"current-file", bytesOption(options, "current_file")},
               {"current-name", stringOption(options, "current_name")},
               {"accept-label", stringOption(options, "accept_label")},
               {"language", g_getenv("LUDASH_LANGUAGE")
                                 ? g_getenv("LUDASH_LANGUAGE")
                                 : ""},
               {"filters", filtersOption(options, filterIndex)},
               {"choices", choicesOption(options)}};
  request["filter-index"] = filterIndex;
  return request;
}

std::optional<std::string> decodeBase64(const std::string &value) {
  gsize length = 0;
  guchar *decoded = g_base64_decode(value.c_str(), &length);
  if (!decoded)
    return std::nullopt;
  std::string result(reinterpret_cast<char *>(decoded), length);
  g_free(decoded);
  return result;
}

std::optional<std::string> fileUri(const std::string &path) {
  if (path.find('\0') != std::string::npos)
    return std::nullopt;
  GError *error = nullptr;
  gchar *uri = g_filename_to_uri(path.c_str(), nullptr, &error);
  if (!uri) {
    g_clear_error(&error);
    return std::nullopt;
  }
  std::string result(uri);
  g_free(uri);
  return result;
}

std::vector<std::string> pickerUris(const Json &payload) {
  std::vector<std::string> uris;
  const auto found = payload.find("paths");
  if (found == payload.end() || !found->is_array())
    return uris;
  for (const auto &encoded : *found) {
    if (!encoded.is_string())
      continue;
    const auto path = decodeBase64(encoded.get<std::string>());
    if (!path)
      continue;
    const auto uri = fileUri(*path);
    if (uri)
      uris.push_back(*uri);
  }
  return uris;
}

GVariant *buildPickerResults(const Json &payload, GVariant *options,
                             const std::vector<std::string> *uriOverride = nullptr) {
  GVariantBuilder results;
  g_variant_builder_init(&results, G_VARIANT_TYPE_VARDICT);
  const auto uris = uriOverride ? *uriOverride : pickerUris(payload);
  std::vector<const gchar *> uriPointers;
  uriPointers.reserve(uris.size());
  for (const auto &uri : uris)
    uriPointers.push_back(uri.c_str());
  g_variant_builder_add(
      &results, "{sv}", "uris",
      g_variant_new_strv(uriPointers.data(),
                         static_cast<gssize>(uriPointers.size())));

  const int filterIndex = payload.value("filter-index", -1);
  GVariant *filters = g_variant_lookup_value(
      options, "filters", G_VARIANT_TYPE("a(sa(us))"));
  if (filters && filterIndex >= 0 &&
      static_cast<gsize>(filterIndex) < g_variant_n_children(filters)) {
    GVariant *filter =
        g_variant_get_child_value(filters, static_cast<gsize>(filterIndex));
    g_variant_builder_add(&results, "{sv}", "current_filter", filter);
    g_variant_unref(filter);
  }
  if (filters)
    g_variant_unref(filters);

  GVariant *choiceOptions =
      g_variant_lookup_value(options, "choices", nullptr);
  if (choiceOptions) {
    g_variant_unref(choiceOptions);
    GVariantBuilder selected;
    g_variant_builder_init(&selected, G_VARIANT_TYPE("a(ss)"));
    const auto found = payload.find("choices");
    if (found != payload.end() && found->is_array()) {
      for (const auto &choice : *found) {
        if (!choice.is_object())
          continue;
        const auto id = choice.value("id", std::string{});
        const auto selectedId = choice.value("selected", std::string{});
        g_variant_builder_add(&selected, "(ss)", id.c_str(),
                              selectedId.c_str());
      }
    }
    g_variant_builder_add(&results, "{sv}", "choices",
                          g_variant_builder_end(&selected));
  }
  return g_variant_builder_end(&results);
}

std::optional<std::vector<std::string>> saveFilesOption(GVariant *options) {
  GVariant *files = g_variant_lookup_value(options, "files",
                                            G_VARIANT_TYPE("aay"));
  if (!files)
    return std::nullopt;
  std::vector<std::string> names;
  GVariantIter iterator;
  g_variant_iter_init(&iterator, files);
  GVariant *bytes = nullptr;
  while ((bytes = g_variant_iter_next_value(&iterator))) {
    gsize length = 0;
    const auto *data = static_cast<const guint8 *>(
        g_variant_get_fixed_array(bytes, &length, sizeof(guint8)));
    names.emplace_back(reinterpret_cast<const char *>(data), length);
    g_variant_unref(bytes);
  }
  g_variant_unref(files);
  return portalSaveFileNames(std::move(names));
}

std::optional<std::string> selectedFolder(const Json &payload) {
  const auto found = payload.find("paths");
  if (found == payload.end() || !found->is_array() || found->empty() ||
      !found->front().is_string())
    return std::nullopt;
  return decodeBase64(found->front().get<std::string>());
}

bool folderWritable(const std::string &folder) {
  struct stat status {};
  return !folder.empty() && ::stat(folder.c_str(), &status) == 0 &&
         S_ISDIR(status.st_mode) && ::access(folder.c_str(), W_OK | X_OK) == 0;
}

std::vector<std::string> saveFileUris(const Json &payload,
                                      const std::vector<std::string> &names) {
  const auto folder = selectedFolder(payload);
  if (!folder || !folderWritable(*folder))
    return {};
  std::vector<std::string> uris;
  std::vector<std::string> reserved;
  for (const auto &name : names) {
    fs::path candidate(name);
    int suffix = 1;
    std::string unique = name;
    while (fs::exists(fs::path(*folder) / unique) ||
           std::find(reserved.begin(), reserved.end(), unique) !=
               reserved.end()) {
      unique = candidate.stem().string() + " (" + std::to_string(suffix++) +
               ")" + candidate.extension().string();
    }
    reserved.push_back(unique);
    const auto uri = fileUri((fs::path(*folder) / unique).string());
    if (!uri)
      return {};
    uris.push_back(*uri);
  }
  return uris;
}

std::string pickerExecutable() {
  std::vector<char> executable(4096);
  const auto length = ::readlink("/proc/self/exe", executable.data(),
                                 executable.size() - 1);
  if (length <= 0)
    return "lunadash-filepicker";
  executable[static_cast<std::size_t>(length)] = '\0';
  return (fs::path(executable.data()).parent_path() / "lunadash-filepicker")
      .string();
}

void finishOperation(std::unique_ptr<PickerOperation> operation,
                     gchar *standardOutput, GError *error) {
  guint response = 2;
  GVariant *results = nullptr;
  if (operation->cancelled) {
    response = 1;
  } else if (!operation->timedOut && !error &&
             g_subprocess_get_successful(operation->process)) {
    Json payload = Json::parse(standardOutput ? standardOutput : "", nullptr,
                               false);
    if (payload.is_object()) {
      if (payload.value("accepted", false)) {
        response = 0;
        if (operation->method == "SaveFiles") {
          const auto names = saveFilesOption(operation->options);
          const auto uris = names ? saveFileUris(payload, *names)
                                  : std::vector<std::string>{};
          if (uris.size() != (names ? names->size() : 0)) {
            response = 2;
          } else {
            results = buildPickerResults(payload, operation->options, &uris);
          }
        } else {
          results = buildPickerResults(payload, operation->options);
        }
      } else {
        response = 1;
      }
    }
  }
  g_clear_error(&error);
  g_free(standardOutput);
  returnPortalResponse(operation->invocation, response, results);
  if (results)
    g_variant_unref(results);
}

void processCommunicated(GObject *source, GAsyncResult *result,
                         gpointer userData) {
  auto operation = std::unique_ptr<PickerOperation>(
      static_cast<PickerOperation *>(userData));
  gchar *standardOutput = nullptr;
  gchar *standardError = nullptr;
  GError *error = nullptr;
  g_subprocess_communicate_utf8_finish(G_SUBPROCESS(source), result,
                                      &standardOutput, &standardError, &error);
  g_free(standardError);
  finishOperation(std::move(operation), standardOutput, error);
}

gboolean timeoutOperation(gpointer userData) {
  auto *operation = static_cast<PickerOperation *>(userData);
  operation->timeout = 0;
  operation->timedOut = true;
  if (operation->process)
    g_subprocess_force_exit(operation->process);
  return G_SOURCE_REMOVE;
}

void requestMethodCall(GDBusConnection *, const gchar *, const gchar *,
                       const gchar *, const gchar *method,
                       GVariant *, GDBusMethodInvocation *invocation,
                       gpointer userData) {
  auto *operation = static_cast<PickerOperation *>(userData);
  if (g_strcmp0(method, "Close") != 0) {
    g_dbus_method_invocation_return_error_literal(
        invocation, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD,
        "Unknown portal request method.");
    return;
  }
  g_dbus_method_invocation_return_value(invocation, nullptr);
  operation->cancelled = true;
  if (operation->process)
    g_subprocess_force_exit(operation->process);
}

const GDBusInterfaceVTable requestVTable{requestMethodCall, nullptr, nullptr,
                                         {nullptr}};

void returnFailed(GDBusMethodInvocation *invocation, const char *message) {
  g_dbus_method_invocation_return_dbus_error(invocation, kFailedError, message);
}

void startPicker(GDBusConnection *connection, GDBusNodeInfo *introspection,
                 GDBusMethodInvocation *invocation, const gchar *requestPath,
                 const gchar *method, const std::string &mode,
                 const std::string &title, GVariant *options) {
  auto operation = std::make_unique<PickerOperation>();
  operation->connection = G_DBUS_CONNECTION(g_object_ref(connection));
  operation->introspection = g_dbus_node_info_ref(introspection);
  operation->invocation = G_DBUS_METHOD_INVOCATION(g_object_ref(invocation));
  operation->requestPath = requestPath ? requestPath : "";
  operation->method = method ? method : "";
  operation->mode = mode;
  operation->options = g_variant_ref(options);
  operation->requestText = pickerRequest(mode, title, options).dump() + "\n";

  const auto executable = pickerExecutable();
  GError *error = nullptr;
  const gchar *arguments[] = {executable.c_str(), nullptr};
  operation->process = g_subprocess_newv(
      arguments,
      static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                    G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                    G_SUBPROCESS_FLAGS_STDERR_SILENCE),
      &error);
  if (!operation->process) {
    const std::string message = error ? error->message : "Could not start the file picker.";
    returnFailed(invocation, message.c_str());
    g_clear_error(&error);
    return;
  }

  if (!operation->requestPath.empty()) {
    GDBusInterfaceInfo *requestInterface = g_dbus_node_info_lookup_interface(
        introspection, kRequestInterface);
    operation->requestRegistration = g_dbus_connection_register_object(
        connection, operation->requestPath.c_str(), requestInterface,
        &requestVTable, operation.get(), nullptr, &error);
    if (!operation->requestRegistration) {
      g_subprocess_force_exit(operation->process);
      const std::string message =
          error ? error->message : "Could not export the portal request.";
      returnFailed(invocation, message.c_str());
      g_clear_error(&error);
      return;
    }
  }

  operation->timeout = g_timeout_add_seconds(180, timeoutOperation,
                                               operation.get());
  auto *owned = operation.release();
  g_subprocess_communicate_utf8_async(owned->process, owned->requestText.c_str(),
                                      nullptr, processCommunicated, owned);
}

void fileChooserMethodCall(GDBusConnection *connection, const gchar *,
                           const gchar *, const gchar *, const gchar *method,
                           GVariant *parameters,
                           GDBusMethodInvocation *invocation,
                           gpointer userData) {
  auto *self = static_cast<FileChooserPortal *>(userData);
  const gchar *requestPath = nullptr;
  const gchar *appId = nullptr;
  const gchar *parentWindow = nullptr;
  const gchar *title = nullptr;
  GVariant *options = nullptr;
  g_variant_get(parameters, "(&o&s&s&s@a{sv})", &requestPath, &appId,
                &parentWindow, &title, &options);
  (void)appId;
  (void)parentWindow;

  const std::string methodName = method ? method : "";
  std::string mode;
  std::string pickerTitle = title ? title : "";
  if (methodName == "OpenFile") {
    mode = booleanOption(options, "directory") ? "directory" : "open";
    if (pickerTitle.empty())
      pickerTitle = mode == "directory" ? "Choose folder" : "Open file";
  } else if (methodName == "SaveFile") {
    mode = "save";
    if (pickerTitle.empty())
      pickerTitle = "Save file";
  } else if (methodName == "SaveFiles") {
    mode = "directory";
    if (!saveFilesOption(options)) {
      returnPortalResponse(invocation, 2, nullptr);
      g_variant_unref(options);
      return;
    }
    if (pickerTitle.empty())
      pickerTitle = "Choose folder";
  } else {
    g_dbus_method_invocation_return_error_literal(
        invocation, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD,
        "Unknown FileChooser method.");
    g_variant_unref(options);
    return;
  }

  if (methodName == "SaveFiles") {
    GVariant *copy = g_variant_ref(options);
    GVariantDict dict;
    g_variant_dict_init(&dict, copy);
    g_variant_dict_insert(&dict, "multiple", "b", FALSE);
    GVariant *folderOptions = g_variant_dict_end(&dict);
    g_variant_unref(copy);
    startPicker(connection,
                static_cast<FileChooserPortal *>(userData)->introspection(),
                invocation, requestPath, method, mode, pickerTitle,
                folderOptions);
    g_variant_unref(folderOptions);
  } else {
    startPicker(connection,
                static_cast<FileChooserPortal *>(userData)->introspection(),
                invocation, requestPath, method, mode, pickerTitle, options);
  }
  g_variant_unref(options);
  (void)self;
}

GVariant *fileChooserGetProperty(GDBusConnection *, const gchar *,
                                 const gchar *, const gchar *,
                                 const gchar *property, GError **error,
                                 gpointer) {
  if (g_strcmp0(property, "version") == 0)
    return g_variant_new_uint32(4);
  g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY,
              "Unknown FileChooser property '%s'.", property);
  return nullptr;
}

const GDBusInterfaceVTable fileChooserVTable{fileChooserMethodCall,
                                             fileChooserGetProperty, nullptr,
                                             {nullptr}};

} // namespace

FileChooserPortal::FileChooserPortal(GDBusConnection *connection,
                                     GDBusNodeInfo *introspection)
    : connection_(G_DBUS_CONNECTION(g_object_ref(connection))),
      introspection_(g_dbus_node_info_ref(introspection)) {}

FileChooserPortal::~FileChooserPortal() {
  if (registration_)
    g_dbus_connection_unregister_object(connection_, registration_);
  g_dbus_node_info_unref(introspection_);
  g_object_unref(connection_);
}

bool FileChooserPortal::registerObject(GError **error) {
  GDBusInterfaceInfo *interface = g_dbus_node_info_lookup_interface(
      introspection_, kFileChooserInterface);
  registration_ = g_dbus_connection_register_object(
      connection_, kDesktopPath, interface, &fileChooserVTable, this, nullptr,
      error);
  return registration_ != 0;
}

} // namespace LunaDash
