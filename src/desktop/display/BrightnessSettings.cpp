#include "desktop/display/BrightnessSettings.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

namespace LunaDash {
namespace {
constexpr int kRefreshIntervalMilliseconds = 5000;

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

std::vector<std::string_view> split(std::string_view text, char separator) {
  std::vector<std::string_view> fields;
  std::size_t begin = 0;
  while (begin <= text.size()) {
    const auto end = text.find(separator, begin);
    fields.push_back(text.substr(begin, end == std::string_view::npos
                                            ? std::string_view::npos
                                            : end - begin));
    if (end == std::string_view::npos)
      break;
    begin = end + 1;
  }
  return fields;
}
} // namespace

nlohmann::json parseBacklight(std::string_view text) {
  std::size_t begin = 0;
  while (begin <= text.size()) {
    const auto end = text.find('\n', begin);
    const auto line = text.substr(begin, end == std::string_view::npos
                                             ? std::string_view::npos
                                             : end - begin);
    const auto fields = split(line, ',');
    if (fields.size() == 5 && !trim(fields[0]).empty() &&
        trim(fields[1]) == "backlight") {
      const auto percent = trim(fields[3]);
      if (percent.size() > 1 && percent.back() == '%') {
        int value = -1;
        const auto number = percent.substr(0, percent.size() - 1);
        const auto [parsed, error] = std::from_chars(
            number.data(), number.data() + number.size(), value);
        if (error == std::errc{} && parsed == number.data() + number.size() &&
            value >= 0 && value <= 100)
          return {{"available", true},
                  {"device", std::string(trim(fields[0]))},
                  {"percent", value}};
      }
    }
    if (end == std::string_view::npos)
      break;
    begin = end + 1;
  }
  return {{"available", false}};
}

BrightnessSettings::BrightnessSettings(wl_event_loop *eventLoop)
    : query_(eventLoop), action_(eventLoop) {
  executable_ = findExecutable("brightnessctl");
  refreshTimer_ = wl_event_loop_add_timer(eventLoop, handleRefresh, this);
  if (refreshTimer_)
    wl_event_source_timer_update(refreshTimer_, kRefreshIntervalMilliseconds);
  refresh();
}

BrightnessSettings::~BrightnessSettings() {
  if (refreshTimer_)
    wl_event_source_remove(refreshTimer_);
  refreshTimer_ = nullptr;
}

int BrightnessSettings::handleRefresh(void *data) {
  auto *settings = static_cast<BrightnessSettings *>(data);
  settings->refresh();
  if (settings->refreshTimer_)
    wl_event_source_timer_update(settings->refreshTimer_,
                                 kRefreshIntervalMilliseconds);
  return 0;
}

nlohmann::json BrightnessSettings::snapshot() const {
  auto result = data_;
  result["installed"] = !executable_.empty();
  result["busy"] = action_.busy();
  result["error"] = error_;
  return result;
}

void BrightnessSettings::refresh() {
  if (executable_.empty() || action_.busy() || query_.busy() || pending_ >= 0)
    return;
  query_.run(executable_, {"-m", "-c", "backlight"},
             [this](bool success, const std::string &output) {
               data_ = success ? parseBacklight(output)
                               : nlohmann::json{{"available", false}};
             });
}

bool BrightnessSettings::setPercent(int percent, std::string *error) {
  if (error)
    error->clear();
  if (percent < 1 || percent > 100 || !data_.value("available", false) ||
      executable_.empty()) {
    if (error)
      *error = "Brightness requires an available backlight and a value from 1 "
               "to 100.";
    return false;
  }
  pending_ = percent;
  error_.clear();
  applyPending();
  return true;
}

void BrightnessSettings::applyPending() {
  if (pending_ < 0 || action_.busy())
    return;
  const int target = pending_;
  pending_ = -1;
  const auto device = data_.value("device", std::string{});
  if (!action_.run(executable_,
                   {"-m", "-c", "backlight", "-d", device, "set",
                    std::to_string(target) + "%"},
                   [this](bool success, const std::string &output) {
                     if (success) {
                       const auto updated = parseBacklight(output);
                       if (updated.value("available", false))
                         data_ = updated;
                     } else {
                       error_ =
                           "Brightness change was refused. Check backlight "
                           "permissions for this session.";
                       pending_ = -1;
                     }
                     if (pending_ >= 0)
                       applyPending();
                     else
                       refresh();
                   })) {
    error_ = "Could not start the brightness request.";
    refresh();
  }
}

} // namespace LunaDash
