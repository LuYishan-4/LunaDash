#include "desktop/audio/AudioSettings.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <vector>

namespace LunaDash {
namespace {
constexpr int kRefreshIntervalMilliseconds = 2500;

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

std::string trim(std::string_view text) {
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos)
    return {};
  const auto last = text.find_last_not_of(" \t\r\n");
  return std::string(text.substr(first, last - first + 1));
}

bool parseNonNegativeInteger(std::string_view text, int *value) {
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), *value);
  return error == std::errc{} && end == text.data() + text.size() &&
         *value >= 0;
}

} // namespace

nlohmann::json parseAudioDevices(std::string_view text) {
  nlohmann::json devices = nlohmann::json::array();
  bool sinks = false;
  std::size_t begin = 0;
  while (begin <= text.size()) {
    const auto end = text.find('\n', begin);
    auto line = text.substr(begin, end == std::string_view::npos
                                       ? std::string_view::npos
                                       : end - begin);
    if (line.find("Sinks:") != std::string_view::npos) {
      sinks = true;
    } else if (line.find("Sources:") != std::string_view::npos) {
      sinks = false;
    } else if (sinks) {
      const auto volume = line.find("[vol:");
      if (volume != std::string_view::npos) {
        auto description = trim(line.substr(0, volume));
        const auto marker = description.find('*');
        const bool isDefault = marker != std::string::npos;
        if (isDefault)
          description.erase(0, marker + 1);
        const auto idStart = description.find_first_of("0123456789");
        if (idStart == std::string::npos)
          continue;
        const auto separator = description.find('.', idStart);
        int id = -1;
        if (separator != std::string::npos &&
            parseNonNegativeInteger(std::string_view(description)
                                        .substr(idStart, separator - idStart),
                                    &id)) {
          const auto name =
              trim(std::string_view(description).substr(separator + 1));
          devices.push_back(
              {{"id", id}, {"name", name}, {"default", isDefault}});
        }
      }
    }
    if (end == std::string_view::npos)
      break;
    begin = end + 1;
  }
  return devices;
}

nlohmann::json parseAudioVolume(std::string_view text) {
  auto valueText = trim(text);
  constexpr std::string_view prefix = "Volume: ";
  if (!std::string_view(valueText).starts_with(prefix))
    return {{"available", false}};
  valueText.erase(0, prefix.size());
  bool muted = false;
  constexpr std::string_view mutedSuffix = " [MUTED]";
  if (std::string_view(valueText).ends_with(mutedSuffix)) {
    muted = true;
    valueText.resize(valueText.size() - mutedSuffix.size());
  }

  std::size_t parsed = 0;
  double value = -1;
  try {
    value = std::stod(valueText, &parsed);
  } catch (...) {
    return {{"available", false}};
  }
  if (parsed != valueText.size() || !std::isfinite(value) || value < 0 ||
      value > 10)
    return {{"available", false}};
  return {
      {"available", true},
      {"volume", std::clamp(static_cast<int>(std::round(value * 100)), 0, 100)},
      {"muted", muted}};
}

std::vector<std::string> audioCommand(const nlohmann::json &change) {
  if (!change.is_object() || change.size() != 2 || !change.contains("device") ||
      !change["device"].is_string())
    return {};
  const auto device = change["device"].get<std::string>();
  if (device != "output" && device != "input")
    return {};

  if (device == "output" && change.contains("id") && change["id"].is_number()) {
    const double rawId = change["id"].get<double>();
    if (!std::isfinite(rawId) || std::floor(rawId) != rawId || rawId < 0 ||
        rawId > 2147483647)
      return {};
    return {"set-default", std::to_string(static_cast<int>(rawId))};
  }

  const auto target =
      device == "output" ? "@DEFAULT_AUDIO_SINK@" : "@DEFAULT_AUDIO_SOURCE@";
  if (change.contains("mute") && change["mute"].is_boolean())
    return {"set-mute", target, change["mute"].get<bool>() ? "1" : "0"};

  const auto value = change.find("volume");
  if (value == change.end() || !value->is_number())
    return {};
  const auto number = value->get<double>();
  if (!std::isfinite(number) || number < 0 || number > 100 ||
      std::floor(number) != number)
    return {};
  return {"set-volume", target, std::to_string(static_cast<int>(number)) + "%",
          "--limit", "1.0"};
}

AudioSettings::AudioSettings(wl_event_loop *eventLoop)
    : outputProbe_(eventLoop), inputProbe_(eventLoop), action_(eventLoop),
      devicesProbe_(eventLoop) {
  executable_ = findExecutable("wpctl");
  refreshTimer_ = wl_event_loop_add_timer(eventLoop, handleRefresh, this);
  if (refreshTimer_)
    wl_event_source_timer_update(refreshTimer_, kRefreshIntervalMilliseconds);
  refresh();
}

AudioSettings::~AudioSettings() {
  if (refreshTimer_)
    wl_event_source_remove(refreshTimer_);
  refreshTimer_ = nullptr;
}

int AudioSettings::handleRefresh(void *data) {
  auto *settings = static_cast<AudioSettings *>(data);
  settings->refresh();
  if (settings->refreshTimer_)
    wl_event_source_timer_update(settings->refreshTimer_,
                                 kRefreshIntervalMilliseconds);
  return 0;
}

void AudioSettings::refresh() {
  if (executable_.empty())
    return;
  outputProbe_.run(executable_, {"get-volume", "@DEFAULT_AUDIO_SINK@"},
                   [this](bool success, const std::string &output) {
                     output_ = success ? parseAudioVolume(output)
                                       : nlohmann::json{{"available", false}};
                   });
  inputProbe_.run(executable_, {"get-volume", "@DEFAULT_AUDIO_SOURCE@"},
                  [this](bool success, const std::string &output) {
                    input_ = success ? parseAudioVolume(output)
                                     : nlohmann::json{{"available", false}};
                  });
  if ((refreshCount_++ % 4) == 0)
    devicesProbe_.run(executable_, {"status"},
                      [this](bool success, const std::string &output) {
                        outputDevices_ = success ? parseAudioDevices(output)
                                                 : nlohmann::json::array();
                      });
}

nlohmann::json AudioSettings::snapshot() const {
  return {{"installed", !executable_.empty()},
          {"output", output_},
          {"input", input_},
          {"outputDevices", outputDevices_},
          {"busy", action_.busy()},
          {"error", error_}};
}

bool AudioSettings::apply(const nlohmann::json &change, std::string *error) {
  if (error)
    error->clear();
  const auto arguments = audioCommand(change);
  if (arguments.empty() || executable_.empty() || action_.busy()) {
    if (error)
      *error = "Invalid audio setting, unavailable WirePlumber, or another "
               "audio change is pending.";
    return false;
  }
  error_.clear();
  if (!action_.run(
          executable_, arguments, [this](bool success, const std::string &) {
            error_ = success ? ""
                             : "Audio change failed. Check the selected "
                               "device and PipeWire service.";
            refresh();
          })) {
    if (error)
      *error = "Could not start the audio request.";
    return false;
  }
  return true;
}

} // namespace LunaDash
