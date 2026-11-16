#include "compositor/client/ClientWindow.hpp"
#include "compositor/commands/CompositorCommands.hpp"
#include "compositor/session/LaunchPolicy.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/xwayland/XWaylandSupport.hpp"
#include "core/text/Url.hpp"
#include "desktop/app/DefaultApplications.hpp"
#include "desktop/system/SystemTools.hpp"

#include <glib.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace LunaDash {
namespace {

// Bounds for commands that arrive over the control channel.
constexpr std::size_t maximumDesktopIdLength = 256;
constexpr std::size_t maximumCommandArguments = 64;
constexpr std::size_t maximumArgumentLength = 1024;
// Characters an http(s) URL keeps when it is passed to the browser.
constexpr const char *urlReservedCharacters = ":/?#[]@!$&'()*+,;=%";

bool isOpenableWebUrl(const std::string &value) {
  const auto scheme = urlScheme(value);
  const bool hasAuthority = value.size() > scheme.size() + 3 &&
                            value.compare(scheme.size(), 3, "://") == 0;
  return (scheme == "https" || scheme == "http") && hasAuthority &&
         std::none_of(value.begin(), value.end(), [](unsigned char character) {
           return character < 0x20 || character == 0x7F;
         });
}

// A JSON array of short strings, or an error message.
CommandResult commandArray(const Json &document, std::vector<std::string> *command) {
  if (!document.is_array() || document.empty())
    return commandError("Expected a non-empty command array.");
  for (const auto &entry : document) {
    if (!entry.is_string() || jsonAsString(entry).size() > maximumArgumentLength)
      return commandError("Command arguments must be short strings.");
    command->push_back(jsonAsString(entry));
  }
  return {};
}

} // namespace

void CompositorCommands::registerLaunchCommands(CommandRegistry &registry) {
  using C = WaylandCompositor;
  using R = CommandRequest;

  registry.add("default-apps", [](C &, const R &r) -> CommandResult {
    const auto changes = parseJsonObject(r.value);
    std::string error;
    if (!changes.is_object() || !setDefaultApplications(changes, &error))
      return commandError(error.empty() ? "Expected a JSON object." : error);
    return {};
  });
  registry.add("open-url", [](C &c, const R &r) -> CommandResult {
    if (!isOpenableWebUrl(r.value))
      return commandError("Only valid HTTP and HTTPS URLs can be opened.");
    std::string error;
    auto command =
        defaultApplicationCommand(DefaultApplicationRole::browser, &error);
    if (!error.empty())
      return commandError(error);
    if (command.empty())
      return commandError("No browser is configured.");
    command.push_back(percentEncode(r.value, urlReservedCharacters));
    if (!c.launchExternalCommand(command, &error))
      return commandError(error);
    return {};
  });
  registry.add("launch-default", [](C &c, const R &r) -> CommandResult {
    if (!isDefaultApplicationRole(r.value))
      return invalidCommand();
    std::string error;
    auto command = defaultApplicationCommand(r.value, &error);
    if (!error.empty())
      return commandError(error);
    if (command.empty())
      return commandError("No application is configured for this role.");
    if (!c.launchExternalCommand(command, &error))
      return commandError(error);
    return {};
  });
  registry.add("system-tool", [](C &c, const R &r) -> CommandResult {
    auto command = systemSettingsCommand(r.value);
    if (command.empty())
      return commandError("This settings tool is unavailable.");
    const std::string program = command.front();
    command.erase(command.begin());
    c.spawn(command, program, false);
    return {};
  });
  registry.add("configure-network", [](C &c, const R &) -> CommandResult {
    auto command = networkConnectionEditorCommand();
    if (command.empty())
      return commandError("No network configuration utility is installed.");
    const std::string program = command.front();
    command.erase(command.begin());
    c.spawn(command, program, false);
    return {};
  });
  // A shell-quoted command line run under XWayland.
  registry.add("launch-x11", [](C &c, const R &r) -> CommandResult {
    gchar **parsed = nullptr;
    gint count = 0;
    GError *parseError = nullptr;
    if (!g_shell_parse_argv(r.value.c_str(), &count, &parsed, &parseError)) {
      const std::string message = parseError ? std::string(parseError->message)
                                             : std::string("Invalid command line.");
      if (parseError)
        g_error_free(parseError);
      return commandError(message);
    }
    std::vector<std::string> command;
    command.reserve(static_cast<std::size_t>(count));
    for (gint index = 0; index < count; ++index)
      command.emplace_back(parsed[index]);
    g_strfreev(parsed);
    std::string error;
    if (!c.xwayland_ || !c.xwayland_->launch(command, &error))
      return commandError(error.empty() ? "XWayland is unavailable." : error);
    for (const auto &client : c.clients_)
      c.updateClientMetadata(client.get());
    c.arrange();
    return {};
  });
  // A desktop entry's command; its launch policy decides whether it needs XWayland.
  registry.add("launch-application", [](C &c, const R &r) -> CommandResult {
    const auto object = parseJsonObject(r.value);
    if (!object.is_object())
      return commandError("Expected an application launch object.");
    if (object.size() != 2 || !jsonValue(object, "desktopId").is_string() ||
        !jsonValue(object, "command").is_array())
      return commandError("Application launch requires desktopId and command.");
    const auto desktopId = jsonString(object, "desktopId");
    if (desktopId.size() > maximumDesktopIdLength)
      return commandError("Desktop application ID is too long.");
    const auto values = jsonArray(object, "command");
    if (values.empty() || values.size() > maximumCommandArguments)
      return commandError("Expected a bounded non-empty command array.");
    std::vector<std::string> command;
    for (const auto &entry : values) {
      if (!entry.is_string() || jsonAsString(entry).empty() ||
          jsonAsString(entry).size() > maximumArgumentLength)
        return commandError("Command arguments must be short strings.");
      command.push_back(jsonAsString(entry));
    }
    const auto capabilities = resolveLaunchCapabilities(desktopId, command);
    std::string error;
    if (!c.launchExternalCommand(command, &error, capabilities.x11Helper))
      return commandError(error);
    return {};
  });
  const auto launchCommand = [](bool x11Helper) {
    return [x11Helper](C &c, const R &r) -> CommandResult {
      std::vector<std::string> command;
      if (auto error = commandArray(parseJson(r.value), &command))
        return error;
      std::string error;
      if (!c.launchExternalCommand(command, &error, x11Helper))
        return commandError(error);
      return {};
    };
  };
  registry.add("launch-command", launchCommand(false));
  registry.add("launch-with-x11", launchCommand(true));
}

} // namespace LunaDash
