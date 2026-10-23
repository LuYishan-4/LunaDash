#include "desktop/power/PowerSettings.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <unistd.h>

namespace LunaDash {
namespace {
constexpr int kRefreshIntervalMilliseconds = 10000;

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

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

} // namespace

nlohmann::json parsePowerProfiles(std::string_view text) {
  nlohmann::json profiles = nlohmann::json::array();
  std::string current;
  std::size_t begin = 0;
  while (begin <= text.size()) {
    const auto end = text.find('\n', begin);
    auto line = trim(std::string(text.substr(
        begin,
        end == std::string_view::npos ? std::string_view::npos : end - begin)));
    bool selected = false;
    if (!line.empty() && line.front() == '*') {
      selected = true;
      line = trim(line.substr(1));
    }
    if (!line.empty() && line.back() == ':') {
      line.pop_back();
      if (line == "power-saver" || line == "balanced" ||
          line == "performance") {
        profiles.push_back(line);
        if (selected)
          current = line;
      }
    }
    if (end == std::string_view::npos)
      break;
    begin = end + 1;
  }
  return {{"available", !profiles.empty()},
          {"profiles", std::move(profiles)},
          {"current", current}};
}

PowerSettings::PowerSettings(wl_event_loop *eventLoop)
    : probe_(eventLoop), action_(eventLoop) {
  executable_ = findExecutable("powerprofilesctl");
  refreshTimer_ = wl_event_loop_add_timer(eventLoop, handleRefresh, this);
  if (refreshTimer_)
    wl_event_source_timer_update(refreshTimer_, kRefreshIntervalMilliseconds);
  refresh();
}

PowerSettings::~PowerSettings() {
  if (refreshTimer_)
    wl_event_source_remove(refreshTimer_);
  refreshTimer_ = nullptr;
}

int PowerSettings::handleRefresh(void *data) {
  auto *settings = static_cast<PowerSettings *>(data);
  settings->refresh();
  if (settings->refreshTimer_)
    wl_event_source_timer_update(settings->refreshTimer_,
                                 kRefreshIntervalMilliseconds);
  return 0;
}

void PowerSettings::refresh() {
  if (executable_.empty() || probe_.busy())
    return;
  probe_.run(
      executable_, {"list"}, [this](bool success, const std::string &output) {
        data_ = success ? parsePowerProfiles(output)
                        : nlohmann::json{{"available", false},
                                         {"profiles", nlohmann::json::array()},
                                         {"current", ""}};
      });
}

nlohmann::json PowerSettings::snapshot() const {
  auto result = data_;
  result["installed"] = !executable_.empty();
  result["busy"] = action_.busy();
  result["error"] = error_;
  return result;
}

bool PowerSettings::apply(std::string_view profile, std::string *error) {
  if (error)
    error->clear();
  const auto profiles = data_.value("profiles", nlohmann::json::array());
  const bool known = std::any_of(
      profiles.begin(), profiles.end(), [profile](const nlohmann::json &entry) {
        return entry.is_string() && entry.get<std::string>() == profile;
      });
  if (!known || executable_.empty() || action_.busy()) {
    if (error)
      *error = "Power profile is unavailable or another change is pending.";
    return false;
  }
  error_.clear();
  if (!action_.run(
          executable_, {"set", std::string(profile)},
          [this](bool success, const std::string &) {
            error_ =
                success
                    ? ""
                    : "Power profile change was refused by the system service.";
            refresh();
          })) {
    if (error)
      *error = "Could not start the power-profile request.";
    return false;
  }
  return true;
}

} // namespace LunaDash
