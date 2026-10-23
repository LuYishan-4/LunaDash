#include "desktop/display/DdcBrightnessSettings.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>
#include <string_view>
#include <unistd.h>
#include <utility>
#include <vector>

namespace LunaDash {
namespace {
constexpr int kRefreshIntervalMilliseconds = 60000;
constexpr std::size_t kMaximumDisplays = 16;

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
    const std::string candidate =
        (component.empty() ? std::string(".") : component) + "/" +
        std::string(name);
    if (::access(candidate.c_str(), X_OK) == 0)
      return candidate;
    if (end == std::string::npos)
      break;
    begin = end + 1;
  }
  return {};
}

std::string_view trim(std::string_view text) {
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos)
    return {};
  const auto last = text.find_last_not_of(" \t\r\n");
  return text.substr(first, last - first + 1);
}

bool parseUnsigned(std::string_view text, int *value) {
  const auto [parsed, error] =
      std::from_chars(text.data(), text.data() + text.size(), *value);
  return error == std::errc{} && parsed == text.data() + text.size() &&
         *value >= 0;
}

bool isDisplayHeader(std::string_view line) {
  constexpr std::string_view prefix = "Display ";
  if (!line.starts_with(prefix))
    return false;
  const auto number = line.substr(prefix.size());
  int value = 0;
  return parseUnsigned(number, &value) && value > 0;
}

int deviceIndexOf(const std::vector<nlohmann::json> &devices,
                  std::string_view id) {
  for (std::size_t index = 0; index < devices.size(); ++index) {
    if (devices[index].value("id", std::string{}) == id)
      return static_cast<int>(index);
  }
  return -1;
}
} // namespace

nlohmann::json parseDdcDisplays(std::string_view text) {
  nlohmann::json result = nlohmann::json::array();
  std::vector<int> buses;
  nlohmann::json device;
  bool valid = false;

  const auto append = [&] {
    if (!valid || !device.contains("bus") || !device.contains("label") ||
        result.size() >= kMaximumDisplays)
      return;
    const int number = device.at("bus").get<int>();
    if (std::find(buses.begin(), buses.end(), number) != buses.end())
      return;
    buses.push_back(number);
    device["id"] = "i2c-" + std::to_string(number) + ":" +
                   device.at("label").get<std::string>();
    device["available"] = false;
    device["percent"] = 0;
    device["error"] = "";
    result.push_back(device);
  };

  std::size_t begin = 0;
  while (begin <= text.size()) {
    const auto end = text.find('\n', begin);
    const auto line = trim(text.substr(begin, end == std::string_view::npos
                                                  ? std::string_view::npos
                                                  : end - begin));
    if (isDisplayHeader(line) || line.starts_with("Invalid display")) {
      append();
      device = nlohmann::json::object();
      valid = isDisplayHeader(line);
    } else if (valid) {
      constexpr std::string_view busPrefix = "I2C bus: /dev/i2c-";
      if (line.starts_with(busPrefix)) {
        int number = -1;
        if (parseUnsigned(line.substr(busPrefix.size()), &number))
          device["bus"] = number;
      } else if (line.starts_with("Monitor:")) {
        const auto label =
            trim(line.substr(std::string_view("Monitor:").size()));
        if (!label.empty())
          device["label"] = std::string(label);
      }
    }
    if (end == std::string_view::npos)
      break;
    begin = end + 1;
  }
  append();
  return result;
}

nlohmann::json parseDdcBrightness(std::string_view text) {
  std::size_t begin = 0;
  while (begin <= text.size()) {
    const auto end = text.find('\n', begin);
    const auto line = trim(text.substr(begin, end == std::string_view::npos
                                                  ? std::string_view::npos
                                                  : end - begin));
    std::istringstream stream{std::string(line)};
    std::string vcp;
    std::string code;
    std::string type;
    std::string currentText;
    std::string maximumText;
    std::string extra;
    if ((stream >> vcp >> code >> type >> currentText >> maximumText) &&
        !(stream >> extra) && vcp == "VCP" && code == "10" && type == "C") {
      int current = -1;
      int maximum = -1;
      if (parseUnsigned(currentText, &current) &&
          parseUnsigned(maximumText, &maximum) && maximum > 0 &&
          maximum <= 65535 && current <= maximum)
        return {{"available", true},
                {"maximum", maximum},
                {"percent",
                 static_cast<int>(std::round(current * 100.0 / maximum))}};
    }
    if (end == std::string_view::npos)
      break;
    begin = end + 1;
  }
  return {{"available", false}};
}

DdcBrightnessSettings::DdcBrightnessSettings(wl_event_loop *eventLoop)
    : runner_(eventLoop) {
  refreshTimer_ = wl_event_loop_add_timer(eventLoop, handleRefresh, this);
  if (refreshTimer_)
    wl_event_source_timer_update(refreshTimer_, kRefreshIntervalMilliseconds);
  refresh();
}

DdcBrightnessSettings::~DdcBrightnessSettings() {
  if (refreshTimer_)
    wl_event_source_remove(refreshTimer_);
  refreshTimer_ = nullptr;
}

int DdcBrightnessSettings::handleRefresh(void *data) {
  auto *settings = static_cast<DdcBrightnessSettings *>(data);
  settings->refresh();
  if (settings->refreshTimer_)
    wl_event_source_timer_update(settings->refreshTimer_,
                                 kRefreshIntervalMilliseconds);
  return 0;
}

nlohmann::json DdcBrightnessSettings::snapshot() const {
  auto devices = nlohmann::json::array();
  for (auto device : devices_) {
    const auto id = device.value("id", std::string{});
    device["busy"] = id == activeId_ || pending_.contains(id);
    devices.push_back(std::move(device));
  }
  return {{"installed", !executable_.empty()},
          {"scanning", scanning_},
          {"busy", runner_.busy()},
          {"error", error_},
          {"devices", std::move(devices)}};
}

void DdcBrightnessSettings::refresh() {
  if (scanning_)
    return;
  if (runner_.busy()) {
    refreshRequested_ = true;
    return;
  }
  executable_ = findExecutable("ddcutil");
  if (executable_.empty()) {
    devices_.clear();
    error_ = "Install ddcutil to control external monitor brightness.";
    return;
  }
  refreshRequested_ = true;
  advance();
}

bool DdcBrightnessSettings::setPercent(std::string_view id, int percent,
                                       std::string *error) {
  if (error)
    error->clear();
  for (const auto &device : devices_) {
    if (device.value("id", std::string{}) != std::string(id))
      continue;
    if (percent < 0 || percent > 100 || scanning_ ||
        !device.value("available", false) || executable_.empty())
      break;
    pending_[std::string(id)] = percent;
    advance();
    return true;
  }
  if (error)
    *error =
        "Choose an available DDC/CI monitor and a brightness from 0 to 100.";
  return false;
}

void DdcBrightnessSettings::advance() {
  if (runner_.busy())
    return;
  activeId_.clear();
  if (!pending_.empty()) {
    const auto id = pending_.begin()->first;
    const int percent = pending_.begin()->second;
    pending_.erase(pending_.begin());
    const int index = deviceIndexOf(devices_, id);
    if (index < 0) {
      advance();
      return;
    }
    auto &device = devices_[static_cast<std::size_t>(index)];
    activeId_ = id;
    device["error"] = "";
    const int maximum = device.value("maximum", 0);
    if (maximum <= 0) {
      device["error"] = "Monitor brightness range is unavailable.";
      advance();
      return;
    }
    const int value = static_cast<int>(std::round(percent * maximum / 100.0));
    const int bus = device.value("bus", -1);
    if (!runner_.run(
            executable_,
            {"setvcp", "10", std::to_string(value), "--bus",
             std::to_string(bus), "--verify"},
            [this, id, value, maximum](bool success, const std::string &) {
              const int deviceIndex = deviceIndexOf(devices_, id);
              if (deviceIndex < 0) {
                pending_.erase(id);
                advance();
                return;
              }
              auto &updated = devices_[static_cast<std::size_t>(deviceIndex)];
              if (success)
                updated["percent"] =
                    static_cast<int>(std::round(value * 100.0 / maximum));
              else {
                updated["error"] =
                    "DDC/CI brightness change failed. Check the monitor "
                    "connection, DDC/CI setting and I2C permissions.";
                pending_.erase(id);
              }
              advance();
            },
            12000)) {
      device["error"] = "Could not start the DDC/CI brightness request.";
      activeId_.clear();
      advance();
    }
    return;
  }

  if (queryIndex_) {
    if (*queryIndex_ >= devices_.size()) {
      queryIndex_.reset();
      scanning_ = false;
      return;
    }
    const std::size_t index = (*queryIndex_)++;
    const auto device = devices_[index];
    const auto id = device.value("id", std::string{});
    activeId_ = id;
    const int bus = device.value("bus", -1);
    if (!runner_.run(
            executable_,
            {"getvcp", "10", "--brief", "--bus", std::to_string(bus)},
            [this, id](bool success, const std::string &output) {
              const auto value = parseDdcBrightness(output);
              const int index = deviceIndexOf(devices_, id);
              if (index < 0) {
                advance();
                return;
              }
              auto &device = devices_[static_cast<std::size_t>(index)];
              const bool available = success && value.value("available", false);
              device["available"] = available;
              if (available) {
                device["maximum"] = value.value("maximum", 0);
                device["percent"] = value.value("percent", 0);
                device["error"] = "";
              } else
                device["error"] = "This monitor did not report DDC/CI "
                                  "brightness. Enable DDC/CI "
                                  "and check I2C permissions, then refresh.";
              advance();
            },
            12000)) {
      auto &failed = devices_[index];
      failed["available"] = false;
      failed["error"] = "Could not start the DDC/CI brightness query.";
      advance();
    }
    return;
  }

  if (!refreshRequested_)
    return;
  refreshRequested_ = false;
  scanning_ = true;
  error_.clear();
  if (!runner_.run(
          executable_, {"detect", "--brief", "--disable-usb"},
          [this](bool success, const std::string &output) {
            devices_.clear();
            const auto parsed = parseDdcDisplays(output);
            for (const auto &device : parsed)
              devices_.push_back(device);
            if (!success)
              error_ =
                  "DDC/CI detection failed for one or more displays. Check "
                  "DDC/CI, the i2c-dev module and I2C permissions.";
            queryIndex_ = 0;
            advance();
          },
          20000)) {
    scanning_ = false;
    error_ = "Could not start DDC/CI display detection.";
  }
}

} // namespace LunaDash
