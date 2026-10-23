#include "desktop/network/NetworkStatus.hpp"

#include <gio/gio.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <ifaddrs.h>
#include <map>
#include <net/if.h>
#include <sstream>
#include <string>
#include <string_view>
#include <unistd.h>
#include <utility>
#include <vector>

namespace LunaDash {
namespace {
using Document = nlohmann::json;

constexpr int kRefreshIntervalMilliseconds = 30000;

std::string findExecutable(std::string_view name) {
  const char *pathValue = std::getenv("PATH");
  if (!pathValue)
    return {};
  const std::string path(pathValue);
  std::size_t begin = 0;
  while (begin <= path.size()) {
    const auto end = path.find(':', begin);
    const auto component = path.substr(
        begin, end == std::string::npos ? std::string::npos : end - begin);
    const std::filesystem::path directory =
        component.empty() ? std::filesystem::current_path()
                          : std::filesystem::path(component);
    const auto candidate = directory / name;
    if (::access(candidate.c_str(), X_OK) == 0)
      return candidate.string();
    if (end == std::string::npos)
      break;
    begin = end + 1;
  }
  return {};
}

bool boundedText(const Document &request, std::string_view key,
                 std::string *value, std::size_t maximum = 512,
                 bool allowEmpty = false) {
  const auto field = request.find(key);
  if (field == request.end() || !field->is_string())
    return false;
  const auto text = field->get<std::string>();
  if ((!allowEmpty && text.empty()) || text.size() > maximum ||
      text.find('\n') != std::string::npos ||
      text.find('\r') != std::string::npos ||
      text.find('\0') != std::string::npos)
    return false;
  if (value)
    *value = text;
  return true;
}

std::vector<std::string> splitLines(const std::string &text) {
  std::vector<std::string> lines;
  std::size_t begin = 0;
  while (begin <= text.size()) {
    const auto end = text.find('\n', begin);
    auto line = text.substr(begin, end == std::string::npos ? std::string::npos
                                                            : end - begin);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    lines.push_back(std::move(line));
    if (end == std::string::npos)
      break;
    begin = end + 1;
  }
  return lines;
}

// nmcli -t escapes ':' as "\:" and '\\' as "\\\\" when escaping is
// enabled. Split only on unescaped delimiters and remove the escape marker.
std::vector<std::string> splitTerse(const std::string &line) {
  std::vector<std::string> fields;
  std::string current;
  bool escaped = false;
  for (const char character : line) {
    if (escaped) {
      current.push_back(character);
      escaped = false;
    } else if (character == '\\') {
      escaped = true;
    } else if (character == ':') {
      fields.push_back(std::move(current));
      current.clear();
    } else {
      current.push_back(character);
    }
  }
  if (escaped)
    current.push_back('\\');
  fields.push_back(std::move(current));
  return fields;
}

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return value;
}

bool isWifiType(const std::string &type) {
  const auto normalized = lowercase(type);
  return normalized == "wifi" || normalized == "802-11-wireless" ||
         normalized == "wireless";
}

bool isEthernetType(const std::string &type) {
  const auto normalized = lowercase(type);
  return normalized == "ethernet" || normalized == "802-3-ethernet" ||
         normalized == "wired";
}

bool wifiSecurityRequiresPassword(const std::string &security) {
  const auto normalized = lowercase(security);
  return !normalized.empty() && normalized != "--" && normalized != "open" &&
         normalized != "none";
}

bool fieldBool(const Document &object, std::string_view key) {
  const auto value = object.find(key);
  return value != object.end() && value->is_boolean() && value->get<bool>();
}

std::string fieldString(const Document &object, std::string_view key) {
  const auto value = object.find(key);
  return value != object.end() && value->is_string() ? value->get<std::string>()
                                                     : std::string{};
}

void setError(std::string *error, std::string message) {
  if (error)
    *error = std::move(message);
}

bool hasActiveNetworkLink() {
  ifaddrs *interfaces = nullptr;
  if (::getifaddrs(&interfaces) != 0)
    return false;
  bool link = false;
  for (auto *entry = interfaces; entry; entry = entry->ifa_next) {
    if (!entry->ifa_name || !entry->ifa_addr ||
        (entry->ifa_flags & IFF_LOOPBACK) != 0 ||
        (entry->ifa_flags & IFF_UP) == 0 ||
        (entry->ifa_flags & IFF_RUNNING) == 0)
      continue;
    if (entry->ifa_addr->sa_family == AF_INET ||
        entry->ifa_addr->sa_family == AF_INET6) {
      link = true;
      break;
    }
  }
  ::freeifaddrs(interfaces);
  return link;
}

Document fallbackNetworkStatus() {
  const bool link = hasActiveNetworkLink();
  return {{"managed", false},
          {"connected", link},
          {"internet", false},
          {"label", link
                        ? "Network link detected; Internet access is unverified"
                        : "NetworkManager is unavailable"},
          {"hasWifi", false},
          {"wifiEnabled", false},
          {"wifiConnected", false},
          {"ethernetConnected", link},
          {"primaryType", link ? "ethernet" : "disconnected"}};
}

Document networkManagerStatus() {
  GError *error = nullptr;
  GDBusConnection *connection =
      g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
  if (!connection) {
    if (error)
      g_error_free(error);
    return fallbackNetworkStatus();
  }
  GVariant *reply = g_dbus_connection_call_sync(
      connection, "org.freedesktop.NetworkManager",
      "/org/freedesktop/NetworkManager", "org.freedesktop.DBus.Properties",
      "GetAll", g_variant_new("(s)", "org.freedesktop.NetworkManager"),
      G_VARIANT_TYPE("(a{sv})"), G_DBUS_CALL_FLAGS_NONE, 1500, nullptr, &error);
  g_object_unref(connection);
  if (!reply) {
    if (error)
      g_error_free(error);
    return fallbackNetworkStatus();
  }

  GVariant *properties = nullptr;
  g_variant_get(reply, "(@a{sv})", &properties);
  guint state = 0;
  guint connectivity = 0;
  g_variant_lookup(properties, "State", "u", &state);
  g_variant_lookup(properties, "Connectivity", "u", &connectivity);
  g_variant_unref(properties);
  g_variant_unref(reply);
  return describeNetwork(state, connectivity);
}

} // namespace

Document describeNetwork(unsigned int state, unsigned int connectivity) {
  const bool connected = state >= 50;
  const std::string label =
      connected
          ? (connectivity == 4   ? "Connected to the Internet"
             : connectivity == 2 ? "Sign-in required"
                                 : "Connected; Internet access is unverified")
      : state == 40 ? "Connecting"
                    : "Not connected";
  return {{"managed", true},
          {"connected", connected},
          {"internet", connected && connectivity == 4},
          {"label", label},
          {"hasWifi", false},
          {"wifiEnabled", false},
          {"wifiConnected", false},
          {"ethernetConnected", false},
          {"primaryType", connected ? "network" : "disconnected"}};
}

NetworkStatus::NetworkStatus(wl_event_loop *eventLoop)
    : status_({{"label", "Checking network"}, {"managed", false}}),
      command_(eventLoop), bluetoothCommand_(eventLoop) {
  nmcli_ = findExecutable("nmcli");
  bluetoothctl_ = findExecutable("bluetoothctl");
  refresh();
  refreshTimer_ = wl_event_loop_add_timer(eventLoop, handleRefresh, this);
  if (refreshTimer_)
    wl_event_source_timer_update(refreshTimer_, kRefreshIntervalMilliseconds);
}

NetworkStatus::~NetworkStatus() {
  if (refreshTimer_)
    wl_event_source_remove(refreshTimer_);
  refreshTimer_ = nullptr;
}

Document NetworkStatus::snapshot() const { return status_; }

int NetworkStatus::handleRefresh(void *data) {
  auto *status = static_cast<NetworkStatus *>(data);
  status->refresh();
  if (status->refreshTimer_)
    wl_event_source_timer_update(status->refreshTimer_,
                                 kRefreshIntervalMilliseconds);
  return 0;
}

bool NetworkStatus::runNmcli(const std::vector<std::string> &arguments,
                             Completion completion, int timeoutMilliseconds) {
  return !nmcli_.empty() &&
         command_.run(nmcli_, arguments, std::move(completion),
                      timeoutMilliseconds);
}

void NetworkStatus::refresh() {
  refreshBluetooth();
  if (nmcli_.empty()) {
    refreshWithoutNmcli();
    return;
  }
  refreshNmcli();
}

void NetworkStatus::refreshBluetooth() {
  if (bluetoothctl_.empty() || bluetoothCommand_.busy())
    return;
  bluetoothCommand_.run(
      bluetoothctl_, {"devices"},
      [this](bool success, const std::string &output) {
        Document devices = Document::array();
        if (success) {
          for (const auto &line : splitLines(output)) {
            std::istringstream fields(line);
            std::string device;
            std::string address;
            if (!(fields >> device >> address) || device != "Device")
              continue;
            std::string name;
            std::getline(fields, name);
            const auto first = name.find_first_not_of(" \t");
            if (first != std::string::npos)
              name.erase(0, first);
            devices.push_back({{"address", address}, {"name", name}});
          }
        }
        status_["bluetooth"] = std::move(devices);
      });
}

void NetworkStatus::refreshWithoutNmcli() { status_ = networkManagerStatus(); }

void NetworkStatus::refreshNmcli() {
  if (pending_ || command_.busy())
    return;
  pending_ = true;
  if (!runNmcli(
          {"-t", "--escape", "yes", "-f", "DEVICE,TYPE,STATE,CONNECTION",
           "device"},
          [this](bool success, const std::string &output) {
            pending_ = false;
            Document devices = Document::array();
            bool connected = false;
            bool hasWifi = false;
            bool wifiEnabled = false;
            bool wifiConnected = false;
            bool ethernetConnected = false;
            std::string primaryConnection;
            std::string primaryType = "disconnected";

            if (success) {
              for (const auto &line : splitLines(output)) {
                const auto fields = splitTerse(line);
                if (fields.size() < 4 || fields[0].empty())
                  continue;
                const auto &device = fields[0];
                const auto &type = fields[1];
                const auto &state = fields[2];
                const auto &connection = fields[3];
                const bool deviceConnected = state == "connected";
                connected = connected || deviceConnected;

                if (isWifiType(type)) {
                  hasWifi = true;
                  wifiEnabled = wifiEnabled || state != "unavailable";
                  if (deviceConnected) {
                    wifiConnected = true;
                    if (primaryConnection.empty()) {
                      primaryConnection = connection;
                      primaryType = "wifi";
                    }
                  }
                } else if (isEthernetType(type) && deviceConnected) {
                  ethernetConnected = true;
                  primaryConnection = connection;
                  primaryType = "ethernet";
                }
                devices.push_back({{"device", device},
                                   {"type", type},
                                   {"state", state},
                                   {"connection", connection}});
              }
            }

            status_ = {
                {"managed", success},
                {"connected", connected},
                {"internet", false},
                {"label", success ? (connected ? "Connected" : "Not connected")
                                  : "NetworkManager is unavailable"},
                {"devices", std::move(devices)},
                {"hasWifi", hasWifi},
                {"wifiEnabled", hasWifi && wifiEnabled},
                {"wifiConnected", wifiConnected},
                {"ethernetConnected", ethernetConnected},
                {"primaryType", primaryType},
                {"primaryConnection", primaryConnection}};
            if (!success)
              return;

            pending_ = true;
            if (!runNmcli(
                    {"-t", "--escape", "yes", "-f",
                     "NAME,UUID,TYPE,DEVICE,STATE,AUTOCONNECT", "connection",
                     "show"},
                    [this](bool connectionSuccess,
                           const std::string &connectionOutput) {
                      pending_ = false;
                      Document connections = Document::array();
                      if (connectionSuccess)
                        for (const auto &line : splitLines(connectionOutput)) {
                          const auto fields = splitTerse(line);
                          if (fields.size() >= 6 && !fields[0].empty())
                            connections.push_back(
                                {{"name", fields[0]},
                                 {"uuid", fields[1]},
                                 {"type", fields[2]},
                                 {"device", fields[3]},
                                 {"state", fields[4]},
                                 {"autoconnect", fields[5] == "yes"}});
                        }
                      status_["connections"] = std::move(connections);
                      if (!fieldBool(status_, "hasWifi")) {
                        status_["wifi"] = Document::array();
                        return;
                      }

                      pending_ = true;
                      if (!runNmcli(
                              {"-t", "--escape", "yes", "-f",
                               "IN-USE,SSID,SIGNAL,SECURITY,DEVICE", "device",
                               "wifi", "list"},
                              [this](bool wifiSuccess,
                                     const std::string &wifiOutput) {
                                pending_ = false;
                                std::map<std::string, Document> strongestBySsid;
                                if (wifiSuccess)
                                  for (const auto &line :
                                       splitLines(wifiOutput)) {
                                    const auto fields = splitTerse(line);
                                    if (fields.size() < 5 || fields[1].empty())
                                      continue;
                                    int signal = 0;
                                    const auto [end, error] = std::from_chars(
                                        fields[2].data(),
                                        fields[2].data() + fields[2].size(),
                                        signal);
                                    if (error != std::errc{} ||
                                        end !=
                                            fields[2].data() + fields[2].size())
                                      signal = 0;
                                    const auto &ssid = fields[1];
                                    Document candidate{
                                        {"active", fields[0] == "*"},
                                        {"ssid", ssid},
                                        {"signal", signal},
                                        {"security", fields[3]},
                                        {"secured",
                                         wifiSecurityRequiresPassword(
                                             fields[3])},
                                        {"device", fields[4]}};
                                    const auto current =
                                        strongestBySsid.find(ssid);
                                    if (current == strongestBySsid.end() ||
                                        current->second["signal"].get<int>() <
                                            signal ||
                                        fieldBool(candidate, "active"))
                                      strongestBySsid[ssid] =
                                          std::move(candidate);
                                  }
                                std::vector<Document> sorted;
                                for (auto &[ssid, network] : strongestBySsid) {
                                  (void)ssid;
                                  sorted.push_back(std::move(network));
                                }
                                std::sort(sorted.begin(), sorted.end(),
                                          [](const Document &left,
                                             const Document &right) {
                                            if (fieldBool(left, "active") !=
                                                fieldBool(right, "active"))
                                              return fieldBool(left, "active");
                                            return left["signal"].get<int>() >
                                                   right["signal"].get<int>();
                                          });
                                status_["wifi"] = sorted;
                              })) {
                        pending_ = false;
                        status_["wifi"] = Document::array();
                      }
                    })) {
              pending_ = false;
              status_["connections"] = Document::array();
            }
          })) {
    pending_ = false;
    status_ = {{"managed", false},
               {"connected", false},
               {"internet", false},
               {"label", "NetworkManager is unavailable"},
               {"devices", Document::array()},
               {"hasWifi", false},
               {"wifiEnabled", false},
               {"wifiConnected", false},
               {"ethernetConnected", false},
               {"primaryType", "disconnected"}};
  }
}

bool NetworkStatus::execute(const Document &request, std::string *error) {
  if (error)
    error->clear();
  const auto action = fieldString(request, "action");
  if (action == "bluetooth-scan") {
    if (bluetoothctl_.empty() || bluetoothCommand_.busy()) {
      setError(error, "Bluetooth control is unavailable or busy.");
      return false;
    }
    return bluetoothCommand_.run(
        bluetoothctl_, {"scan", "on"},
        [this](bool, const std::string &) { refresh(); });
  }

  if (nmcli_.empty() || command_.busy()) {
    setError(error, "NetworkManager/nmcli is unavailable or busy.");
    return false;
  }

  std::vector<std::string> arguments;
  std::string name;
  std::string device;
  if (action == "network-reset") {
    return runNmcli(
        {"networking", "off"},
        [this](bool, const std::string &) {
          if (!runNmcli(
                  {"networking", "on"},
                  [this](bool, const std::string &) { refresh(); }, 15000))
            refresh();
        },
        15000);
  }
  if (action == "connection-reconnect" &&
      boundedText(request, "name", &name, 256)) {
    return runNmcli(
        {"connection", "down", name},
        [this, name](bool, const std::string &) {
          if (!runNmcli(
                  {"connection", "up", name},
                  [this](bool, const std::string &) { refresh(); }, 15000))
            refresh();
        },
        15000);
  }
  if (action == "wifi-scan") {
    if (!fieldBool(status_, "hasWifi")) {
      setError(error, "No wireless network adapter is available.");
      return false;
    }
    arguments = {"device", "wifi", "rescan"};
  } else if (action == "wifi-connect") {
    std::string ssid;
    std::string password;
    if (!boundedText(request, "ssid", &ssid, 128) ||
        !boundedText(request, "password", &password, 512, true)) {
      setError(error, "Invalid Wi-Fi request.");
      return false;
    }
    arguments = {"device", "wifi", "connect", ssid};
    if (!password.empty()) {
      arguments.push_back("password");
      arguments.push_back(password);
    }
  } else if ((action == "connection-up" || action == "connection-down" ||
              action == "connection-delete") &&
             boundedText(request, "name", &name, 256)) {
    arguments = {"connection",
                 action == "connection-up"     ? "up"
                 : action == "connection-down" ? "down"
                                               : "delete",
                 name};
  } else if ((action == "device-connect" || action == "device-disconnect") &&
             boundedText(request, "device", &device, 128)) {
    arguments = {"device",
                 action == "device-connect" ? "connect" : "disconnect", device};
  } else if ((action == "networking-enable" || action == "wifi-enable") &&
             request.contains("enabled") && request["enabled"].is_boolean()) {
    const std::string value = fieldBool(request, "enabled") ? "on" : "off";
    arguments = action == "networking-enable"
                    ? std::vector<std::string>{"networking", value}
                    : std::vector<std::string>{"radio", "wifi", value};
  } else if (action == "connection-autoconnect" &&
             boundedText(request, "name", &name, 256) &&
             request.contains("enabled") && request["enabled"].is_boolean()) {
    arguments = {"connection", "modify", name, "connection.autoconnect",
                 fieldBool(request, "enabled") ? "yes" : "no"};
  } else if (action == "connection-ipv4-auto" &&
             boundedText(request, "name", &name, 256)) {
    arguments = {"connection", "modify",         name, "ipv4.method",
                 "auto",       "ipv4.addresses", "",   "ipv4.gateway",
                 "",           "ipv4.dns",       "",   "ipv4.ignore-auto-dns",
                 "no"};
  } else if (action == "connection-ipv4-manual" &&
             boundedText(request, "name", &name, 256)) {
    std::string address;
    std::string gateway;
    std::string dns;
    if (!boundedText(request, "address", &address, 128) ||
        !boundedText(request, "gateway", &gateway, 128, true) ||
        !boundedText(request, "dns", &dns, 256, true)) {
      setError(error, "Invalid IPv4 settings.");
      return false;
    }
    arguments = {"connection",
                 "modify",
                 name,
                 "ipv4.method",
                 "manual",
                 "ipv4.addresses",
                 address,
                 "ipv4.gateway",
                 gateway,
                 "ipv4.dns",
                 dns,
                 "ipv4.ignore-auto-dns",
                 dns.empty() ? "no" : "yes"};
  } else if ((action == "connection-ipv6-auto" ||
              action == "connection-ipv6-disabled") &&
             boundedText(request, "name", &name, 256)) {
    arguments = {"connection", "modify", name, "ipv6.method",
                 action == "connection-ipv6-auto" ? "auto" : "disabled"};
  } else if (action == "connection-route-metric" &&
             boundedText(request, "name", &name, 256) &&
             request.contains("metric") && request["metric"].is_number()) {
    const double rawMetric = request["metric"].get<double>();
    if (!std::isfinite(rawMetric) || std::floor(rawMetric) != rawMetric ||
        rawMetric < -1 || rawMetric > 65535) {
      setError(error, "Invalid route metric.");
      return false;
    }
    arguments = {"connection", "modify", name, "ipv4.route-metric",
                 std::to_string(static_cast<int>(rawMetric))};
  } else {
    setError(error, "Invalid network request.");
    return false;
  }

  // NetworkManager operations routinely outlast the short status-query
  // timeout. Keep the longer limit for actions that can reconfigure links.
  if (!runNmcli(
          arguments, [this](bool, const std::string &) { refresh(); }, 15000)) {
    setError(error, "Could not start the NetworkManager request.");
    return false;
  }
  return true;
}

} // namespace LunaDash
