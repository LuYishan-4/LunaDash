#include "desktop/weather/WeatherStatus.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>
#include <string_view>
#include <sys/eventfd.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace LunaDash {
namespace {
constexpr int kRefreshIntervalMilliseconds = 15 * 60 * 1000;
constexpr std::size_t kMaximumResponseBytes = 64 * 1024;

struct ResponseBuffer {
  std::string contents;
  bool overflow = false;
};

std::size_t receiveResponse(char *data, std::size_t size, std::size_t count,
                            void *context) {
  auto *response = static_cast<ResponseBuffer *>(context);
  if (size != 0 && count > std::numeric_limits<std::size_t>::max() / size)
    return 0;
  const auto bytes = size * count;
  if (bytes > kMaximumResponseBytes - response->contents.size()) {
    response->overflow = true;
    return 0;
  }
  response->contents.append(data, bytes);
  return bytes;
}

int checkCancellation(void *context, curl_off_t, curl_off_t, curl_off_t,
                      curl_off_t) {
  const auto *stop = static_cast<const std::stop_token *>(context);
  return stop->stop_requested() ? 1 : 0;
}

std::string condition(int code) {
  if (code == 0)
    return "Clear";
  if (code == 1 || code == 2)
    return "Partly cloudy";
  if (code == 3)
    return "Overcast";
  if (code == 45 || code == 48)
    return "Fog";
  if (code >= 51 && code <= 57)
    return "Drizzle";
  if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82))
    return "Rain";
  if ((code >= 71 && code <= 77) || code == 85 || code == 86)
    return "Snow";
  if (code >= 95 && code <= 99)
    return "Thunderstorm";
  return "Weather data unavailable";
}

std::string configurationKey(const WeatherPreferences &preferences) {
  std::ostringstream key;
  key << std::fixed << std::setprecision(3) << preferences.latitude << ','
      << preferences.longitude << ',' << preferences.location;
  return key.str();
}

std::string utcTimestamp() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
  if (!::gmtime_r(&timestamp, &utc))
    return {};
  std::array<char, 32> buffer{};
  if (::strftime(buffer.data(), buffer.size(), "%Y-%m-%dT%H:%M:%SZ", &utc) == 0)
    return {};
  return buffer.data();
}

std::string encodeQueryValue(std::string_view value) {
  static constexpr char hex[] = "0123456789ABCDEF";
  std::string encoded;
  for (const unsigned char character : value) {
    const bool unreserved = (character >= 'a' && character <= 'z') ||
                            (character >= 'A' && character <= 'Z') ||
                            (character >= '0' && character <= '9') ||
                            character == '-' || character == '.' ||
                            character == '_' || character == '~';
    if (unreserved) {
      encoded.push_back(static_cast<char>(character));
    } else {
      encoded.push_back('%');
      encoded.push_back(hex[character >> 4]);
      encoded.push_back(hex[character & 0x0f]);
    }
  }
  return encoded;
}

std::string requestUrl(const WeatherPreferences &preferences) {
  std::ostringstream latitude;
  std::ostringstream longitude;
  latitude << std::fixed << std::setprecision(3) << preferences.latitude;
  longitude << std::fixed << std::setprecision(3) << preferences.longitude;
  return "https://api.open-meteo.com/v1/forecast?latitude=" +
         encodeQueryValue(latitude.str()) +
         "&longitude=" + encodeQueryValue(longitude.str()) +
         "&current=temperature_2m%2Cweather_code&forecast_days=1";
}

} // namespace

WeatherStatus::WeatherStatus(wl_event_loop *eventLoop,
                             PreferencesProvider preferences)
    : preferences_(preferences) {
  completionFd_ = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (completionFd_ >= 0)
    completionSource_ = wl_event_loop_add_fd(
        eventLoop, completionFd_, WL_EVENT_READABLE, handleCompletion, this);
  refreshTimer_ = wl_event_loop_add_timer(eventLoop, handleRefresh, this);
  if (refreshTimer_)
    wl_event_source_timer_update(refreshTimer_, kRefreshIntervalMilliseconds);
  refresh();
}

WeatherStatus::~WeatherStatus() {
  if (refreshTimer_)
    wl_event_source_remove(refreshTimer_);
  refreshTimer_ = nullptr;
  if (requestThread_.joinable()) {
    requestThread_.request_stop();
    requestThread_.join();
  }
  if (completionSource_)
    wl_event_source_remove(completionSource_);
  completionSource_ = nullptr;
  if (completionFd_ >= 0)
    ::close(completionFd_);
  completionFd_ = -1;
}

nlohmann::json WeatherStatus::snapshot() const { return state_; }

int WeatherStatus::handleRefresh(void *data) {
  auto *status = static_cast<WeatherStatus *>(data);
  status->refresh();
  if (status->refreshTimer_)
    wl_event_source_timer_update(status->refreshTimer_,
                                 kRefreshIntervalMilliseconds);
  return 0;
}

int WeatherStatus::handleCompletion(int fd, std::uint32_t, void *data) {
  auto *status = static_cast<WeatherStatus *>(data);
  std::uint64_t value = 0;
  while (::read(fd, &value, sizeof(value)) < 0 && errno == EINTR) {
  }

  std::optional<HttpResult> result;
  std::uint64_t generation = 0;
  {
    std::lock_guard lock(status->completionMutex_);
    result = std::move(status->completion_);
    status->completion_.reset();
    generation = status->completionGeneration_;
  }
  if (!result)
    return 0;

  if (status->requestThread_.joinable())
    status->requestThread_.join();
  status->requestRunning_ = false;
  status->finishRequest(generation, result->success, result->output);
  if (generation != status->requestGeneration_)
    status->refresh();
  return 0;
}

WeatherStatus::HttpResult WeatherStatus::fetch(std::string url,
                                               std::stop_token stop) {
  static std::once_flag initialized;
  static CURLcode globalStatus = CURLE_FAILED_INIT;
  std::call_once(initialized,
                 [] { globalStatus = curl_global_init(CURL_GLOBAL_DEFAULT); });
  if (globalStatus != CURLE_OK)
    return {};

  CURL *curl = curl_easy_init();
  if (!curl)
    return {};
  ResponseBuffer response;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 15000L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "LunaDash/1.0.1a");
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receiveResponse);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, checkCancellation);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &stop);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

  const auto transfer = curl_easy_perform(curl);
  long responseCode = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
  curl_easy_cleanup(curl);
  const bool success = transfer == CURLE_OK && responseCode >= 200 &&
                       responseCode < 300 && !response.overflow &&
                       !stop.stop_requested();
  return {success, std::move(response.contents)};
}

void WeatherStatus::refresh() {
  const WeatherPreferences preferences =
      preferences_ ? preferences_() : WeatherPreferences{};
  if (!preferences.enabled) {
    if (!configuration_.empty()) {
      configuration_.clear();
      ++requestGeneration_;
    }
    if (requestRunning_)
      requestThread_.request_stop();
    state_ = {{"enabled", false}, {"available", false}};
    return;
  }

  const auto nextConfiguration = configurationKey(preferences);
  if (configuration_ != nextConfiguration) {
    configuration_ = nextConfiguration;
    ++requestGeneration_;
    if (requestRunning_)
      requestThread_.request_stop();
    state_ = nlohmann::json::object();
  }
  if (requestRunning_)
    return;
  if (completionFd_ < 0 || !completionSource_) {
    state_["enabled"] = true;
    state_["available"] = false;
    state_["error"] = "Weather service could not initialize.";
    return;
  }

  state_["enabled"] = true;
  state_["location"] = preferences.location;
  state_["provider"] = "Open-Meteo";
  const auto generation = requestGeneration_;
  const auto url = requestUrl(preferences);
  requestRunning_ = true;
  requestThread_ = std::jthread([this, generation, url](std::stop_token stop) {
    auto result = fetch(url, stop);
    {
      std::lock_guard lock(completionMutex_);
      completion_ = std::move(result);
      completionGeneration_ = generation;
    }
    const std::uint64_t notification = 1;
    ssize_t written;
    do {
      written = ::write(completionFd_, &notification, sizeof(notification));
    } while (written < 0 && errno == EINTR);
  });
}

void WeatherStatus::finishRequest(std::uint64_t generation, bool success,
                                  const std::string &output) {
  if (generation != requestGeneration_)
    return;

  try {
    const auto document = nlohmann::json::parse(output);
    const auto current = document.value("current", nlohmann::json::object());
    const auto temperature = current.value("temperature_2m", nlohmann::json{});
    const auto weatherCode = current.value("weather_code", nlohmann::json{});
    if (success && output.size() <= kMaximumResponseBytes &&
        current.is_object() && temperature.is_number() &&
        std::isfinite(temperature.get<double>()) &&
        temperature.get<double>() >= -150 && temperature.get<double>() <= 150 &&
        weatherCode.is_number_integer()) {
      state_["available"] = true;
      state_["temperature"] = std::round(temperature.get<double>());
      state_["condition"] = condition(weatherCode.get<int>());
      state_["updatedAt"] = utcTimestamp();
      state_["error"] = "";
      return;
    }
  } catch (const nlohmann::json::exception &) {
  }

  state_["error"] = "Weather data unavailable";
  state_["available"] = false;
}

} // namespace LunaDash
