#include "desktop/system/UpdateChecker.hpp"

#include "config/BuildConfig.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <string_view>
#include <sys/eventfd.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace LunaDash {
namespace {
constexpr std::string_view kReleaseApi =
    "https://api.github.com/repos/LuYishan-4/LunaDash/releases/latest";
constexpr std::string_view kDevCommitApi =
    "https://api.github.com/repos/LuYishan-4/LunaDash/commits/dev";
constexpr std::string_view kRepositoryUrl =
    "https://github.com/LuYishan-4/LunaDash";
constexpr std::size_t kMaximumReplyBytes = 256 * 1024;

struct ParsedVersion {
  std::vector<int> core;
  std::vector<std::string> prerelease;
  bool valid = false;
};

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

ParsedVersion parseVersion(std::string_view version) {
  if (version.starts_with('v'))
    version.remove_prefix(1);
  ParsedVersion result;
  std::size_t split = version.find('-');
  if (split == std::string_view::npos) {
    for (std::size_t index = 0; index < version.size(); ++index) {
      const char character = version[index];
      if ((character < '0' || character > '9') && character != '.') {
        split = index;
        break;
      }
    }
  }
  const auto core = version.substr(0, split);
  auto prerelease = split == std::string_view::npos ? std::string_view{}
                                                    : version.substr(split);
  if (prerelease.starts_with('-'))
    prerelease.remove_prefix(1);
  if (core.empty())
    return result;

  std::size_t begin = 0;
  while (begin <= core.size()) {
    const auto end = core.find('.', begin);
    const auto part = core.substr(begin, end == std::string_view::npos
                                             ? std::string_view::npos
                                             : end - begin);
    int number = -1;
    const auto [parsed, error] =
        std::from_chars(part.data(), part.data() + part.size(), number);
    if (part.empty() || error != std::errc{} ||
        parsed != part.data() + part.size() || number < 0)
      return result;
    result.core.push_back(number);
    if (end == std::string_view::npos)
      break;
    begin = end + 1;
  }

  begin = 0;
  while (begin < prerelease.size()) {
    const auto end = prerelease.find('.', begin);
    const auto part = prerelease.substr(begin, end == std::string_view::npos
                                                   ? std::string_view::npos
                                                   : end - begin);
    if (!part.empty())
      result.prerelease.emplace_back(part);
    if (end == std::string_view::npos)
      break;
    begin = end + 1;
  }
  result.valid = true;
  return result;
}

bool isNewer(const ParsedVersion &left, const ParsedVersion &right) {
  if (!left.valid || !right.valid)
    return false;
  const auto coreSize = std::max(left.core.size(), right.core.size());
  for (std::size_t index = 0; index < coreSize; ++index) {
    const int leftPart = index < left.core.size() ? left.core[index] : 0;
    const int rightPart = index < right.core.size() ? right.core[index] : 0;
    if (leftPart != rightPart)
      return leftPart > rightPart;
  }

  const auto &leftPre = left.prerelease;
  const auto &rightPre = right.prerelease;
  if (leftPre.empty() || rightPre.empty())
    return leftPre.empty() && !rightPre.empty();
  const auto count = std::min(leftPre.size(), rightPre.size());
  for (std::size_t index = 0; index < count; ++index) {
    int leftNumber = 0;
    int rightNumber = 0;
    const auto [leftEnd, leftError] = std::from_chars(
        leftPre[index].data(), leftPre[index].data() + leftPre[index].size(),
        leftNumber);
    const auto [rightEnd, rightError] = std::from_chars(
        rightPre[index].data(), rightPre[index].data() + rightPre[index].size(),
        rightNumber);
    const bool leftNumeric =
        leftError == std::errc{} &&
        leftEnd == leftPre[index].data() + leftPre[index].size();
    const bool rightNumeric =
        rightError == std::errc{} &&
        rightEnd == rightPre[index].data() + rightPre[index].size();
    if (leftNumeric && rightNumeric) {
      if (leftNumber != rightNumber)
        return leftNumber > rightNumber;
    } else if (leftNumeric != rightNumeric) {
      return rightNumeric;
    } else if (leftPre[index] != rightPre[index]) {
      return leftPre[index] > rightPre[index];
    }
  }
  return leftPre.size() > rightPre.size();
}

std::filesystem::path updateStateDirectory() {
  const char *stateHome = std::getenv("XDG_STATE_HOME");
  if (stateHome && *stateHome)
    return std::filesystem::path(stateHome) / "lunadash/update";
  const char *home = std::getenv("HOME");
  if (!home || !*home)
    return {};
  return std::filesystem::path(home) / ".local/state/lunadash/update";
}

bool updaterProcessAlive(pid_t pid) {
  if (pid <= 1)
    return false;
  std::ifstream commandLine("/proc/" + std::to_string(pid) + "/cmdline",
                            std::ios::binary);
  if (!commandLine)
    return false;
  std::array<char, 4096> command{};
  commandLine.read(command.data(), command.size());
  return std::string_view(command.data(),
                          static_cast<std::size_t>(commandLine.gcount()))
             .find("lunadash-update") != std::string_view::npos;
}

std::string stringValue(const nlohmann::json &object, std::string_view key) {
  const auto value = object.find(key);
  return value != object.end() && value->is_string() ? value->get<std::string>()
                                                     : std::string{};
}

std::int64_t integerValue(const nlohmann::json &object, std::string_view key) {
  const auto value = object.find(key);
  if (value == object.end())
    return 0;
  try {
    if (value->is_number_unsigned()) {
      const auto number = value->get<std::uint64_t>();
      return number <= static_cast<std::uint64_t>(
                           std::numeric_limits<std::int64_t>::max())
                 ? static_cast<std::int64_t>(number)
                 : 0;
    }
    if (value->is_number_integer())
      return value->get<std::int64_t>();
  } catch (const nlohmann::json::exception &) {
  }
  return 0;
}

nlohmann::json installSnapshot() {
  const auto directory = updateStateDirectory();
  const auto path = directory / "progress.json";
  std::error_code existsError;
  const bool progressExists = !directory.empty() &&
                              std::filesystem::exists(path, existsError) &&
                              !existsError;
  nlohmann::json install = {{"state", "idle"},   {"progress", 0},
                            {"stage", "idle"},   {"message", ""},
                            {"channel", ""},     {"target", ""},
                            {"rollback", false}, {"available", progressExists}};
  std::error_code fileError;
  const auto size =
      progressExists ? std::filesystem::file_size(path, fileError) : 0;
  if (!fileError && size <= 32 * 1024) {
    std::ifstream file(path, std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
    try {
      const auto object = nlohmann::json::parse(bytes);
      if (object.is_object()) {
        auto state = stringValue(object, "state");
        auto stage = stringValue(object, "stage");
        auto message = stringValue(object, "message");
        const auto rawPid = integerValue(object, "pid");
        const auto pid = static_cast<pid_t>(std::clamp<std::int64_t>(
            rawPid, 0, std::numeric_limits<pid_t>::max()));
        if (state == "running" && !updaterProcessAlive(pid)) {
          state = "error";
          stage = "interrupted";
          message =
              "The previous update was interrupted and is no longer running.";
        }
        if (state == "idle" || state == "running" || state == "completed" ||
            state == "error")
          install["state"] = state;
        if (!stage.empty() && stage.size() <= 64)
          install["stage"] = stage;
        install["progress"] = static_cast<int>(
            std::clamp<std::int64_t>(integerValue(object, "progress"), 0, 100));
        install["message"] = message.substr(0, 1024);
        install["channel"] = stringValue(object, "channel").substr(0, 16);
        install["target"] = stringValue(object, "target").substr(0, 128);
        const auto rollback = object.find("rollback");
        install["rollback"] = rollback != object.end() && rollback->is_boolean()
                                  ? rollback->get<bool>()
                                  : false;
        install["pid"] = rawPid;
        install["updatedAt"] = integerValue(object, "updatedAt");
        install["active"] = state == "running";
        install["available"] = true;
      }
    } catch (const nlohmann::json::exception &) {
    }
  }

  if (directory.empty())
    return install;
  std::ifstream last(directory / "last-update", std::ios::binary);
  if (last) {
    std::string value;
    value.resize(1024);
    last.read(value.data(), value.size());
    value.resize(static_cast<std::size_t>(last.gcount()));
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first != std::string::npos) {
      const auto end = value.find_last_not_of(" \t\r\n");
      install["lastUpdate"] =
          value.substr(first, std::min<std::size_t>(end - first + 1, 256));
    }
  }
  return install;
}

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
  if (bytes > kMaximumReplyBytes - response->contents.size()) {
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

} // namespace

bool isNewerRelease(std::string_view latest, std::string_view current) {
  return isNewer(parseVersion(latest), parseVersion(current));
}

UpdateChecker::UpdateChecker(wl_event_loop *eventLoop,
                             ChannelProvider channelProvider)
    : channelProvider_(std::move(channelProvider)),
      currentCommit_(LUDASH_GIT_COMMIT) {
  if (currentCommit_.empty())
    currentCommit_ = "unknown";
  completionFd_ = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (completionFd_ >= 0)
    completionSource_ = wl_event_loop_add_fd(
        eventLoop, completionFd_, WL_EVENT_READABLE, handleCompletion, this);
  state_ = {
      {"status", "idle"},
      {"channel", channel_},
      {"currentVersion", LUDASH_VERSION},
      {"currentCommit", currentCommit_},
      {"startedAt", std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count()},
      {"latestVersion", ""},
      {"latestCommit", ""},
      {"latestMessage", ""},
      {"releaseUrl", ""},
      {"repositoryUrl", kRepositoryUrl},
      {"error", ""},
      {"checkedAt", ""}};
}

UpdateChecker::~UpdateChecker() {
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

nlohmann::json UpdateChecker::snapshot() const {
  auto result = state_;
  result["install"] = installSnapshot();
  return result;
}

void UpdateChecker::check() {
  auto selected = channelProvider_ ? channelProvider_() : "stable";
  if (selected != "dev" && selected != "stable")
    selected = "stable";
  if (selected != channel_) {
    channel_ = selected;
    ++requestGeneration_;
    state_["channel"] = channel_;
    state_["status"] = "idle";
    state_["latestVersion"] = "";
    state_["latestCommit"] = "";
    state_["latestMessage"] = "";
    state_["releaseUrl"] = "";
    state_["error"] = "";
    state_["checkedAt"] = "";
    if (requestRunning_)
      requestThread_.request_stop();
  }
  if (requestRunning_)
    return;
  if (completionFd_ < 0 || !completionSource_) {
    state_["status"] = "error";
    state_["error"] = "The update check could not initialize.";
    state_["checkedAt"] = utcTimestamp();
    return;
  }

  state_["status"] = "checking";
  state_["channel"] = channel_;
  state_["latestVersion"] = "";
  state_["latestCommit"] = "";
  state_["latestMessage"] = "";
  state_["releaseUrl"] = "";
  state_["error"] = "";
  const auto generation = requestGeneration_;
  const auto channel = channel_;
  const std::string url(channel == "dev" ? kDevCommitApi : kReleaseApi);
  const std::string userAgent = "LunaDash/" LUDASH_VERSION;
  requestRunning_ = true;
  requestThread_ = std::jthread(
      [this, generation, channel, url, userAgent](std::stop_token stop) {
        auto result = fetch(url, userAgent, stop);
        {
          std::lock_guard lock(completionMutex_);
          completion_ = std::move(result);
          completionChannel_ = channel;
          completionGeneration_ = generation;
        }
        const std::uint64_t notification = 1;
        ssize_t written;
        do {
          written = ::write(completionFd_, &notification, sizeof(notification));
        } while (written < 0 && errno == EINTR);
      });
}

int UpdateChecker::handleCompletion(int fd, std::uint32_t, void *data) {
  auto *checker = static_cast<UpdateChecker *>(data);
  std::uint64_t value = 0;
  while (::read(fd, &value, sizeof(value)) < 0 && errno == EINTR) {
  }
  std::optional<HttpResult> result;
  std::string channel;
  std::uint64_t generation = 0;
  {
    std::lock_guard lock(checker->completionMutex_);
    result = std::move(checker->completion_);
    checker->completion_.reset();
    channel = std::move(checker->completionChannel_);
    generation = checker->completionGeneration_;
  }
  if (!result)
    return 0;
  if (checker->requestThread_.joinable())
    checker->requestThread_.join();
  checker->requestRunning_ = false;
  checker->finishRequest(generation, channel, *result);
  if (generation != checker->requestGeneration_)
    checker->check();
  return 0;
}

UpdateChecker::HttpResult UpdateChecker::fetch(std::string url,
                                               std::string userAgent,
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
  curl_slist *headers =
      curl_slist_append(nullptr, "Accept: application/vnd.github+json");
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 8000L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receiveResponse);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, checkCancellation);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &stop);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

  const auto transfer = curl_easy_perform(curl);
  long responseCode = 0;
  char *effectiveUrl = nullptr;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
  curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effectiveUrl);
  const std::string finalUrl = effectiveUrl ? effectiveUrl : "";
  curl_easy_cleanup(curl);
  if (headers)
    curl_slist_free_all(headers);
  const bool success = transfer == CURLE_OK && responseCode >= 200 &&
                       responseCode < 300 && !response.overflow &&
                       !stop.stop_requested();
  return {success, finalUrl, std::move(response.contents)};
}

void UpdateChecker::finishRequest(std::uint64_t generation,
                                  const std::string &channel,
                                  const HttpResult &result) {
  if (generation != requestGeneration_ || channel != channel_)
    return;
  if (!result.success || result.body.size() > kMaximumReplyBytes) {
    state_["status"] = "error";
    state_["error"] = "GitHub could not be reached.";
    state_["checkedAt"] = utcTimestamp();
    return;
  }

  const std::string expectedUrl(channel == "dev" ? kDevCommitApi : kReleaseApi);
  if (result.effectiveUrl != expectedUrl) {
    state_["status"] = "error";
    state_["error"] = "The update response was rejected.";
    state_["checkedAt"] = utcTimestamp();
    return;
  }

  nlohmann::json object;
  try {
    object = nlohmann::json::parse(result.body);
  } catch (const nlohmann::json::exception &) {
    state_["status"] = "error";
    state_["error"] = "GitHub returned invalid update information.";
    state_["checkedAt"] = utcTimestamp();
    return;
  }
  if (!object.is_object()) {
    state_["status"] = "error";
    state_["error"] = "GitHub returned invalid update information.";
    state_["checkedAt"] = utcTimestamp();
    return;
  }

  if (channel == "dev") {
    const auto latestCommit = stringValue(object, "sha");
    const auto commit = object.find("commit");
    const auto message = commit != object.end() && commit->is_object()
                             ? stringValue(*commit, "message")
                             : std::string{};
    const auto firstLine = message.substr(0, message.find('\n'));
    const auto releaseUrl = stringValue(object, "html_url");
    constexpr std::string_view commitPrefix =
        "https://github.com/LuYishan-4/LunaDash/commit/";
    if (latestCommit.size() < 12 || !releaseUrl.starts_with(commitPrefix) ||
        releaseUrl.size() <= commitPrefix.size()) {
      state_["status"] = "error";
      state_["error"] = "GitHub returned unexpected commit information.";
      state_["checkedAt"] = utcTimestamp();
      return;
    }
    state_["latestCommit"] = latestCommit;
    state_["latestMessage"] = firstLine;
    state_["releaseUrl"] = releaseUrl;
    state_["status"] =
        currentCommit_ != "unknown" && latestCommit.starts_with(currentCommit_)
            ? "upToDate"
            : "available";
  } else {
    const auto latestVersion = stringValue(object, "tag_name");
    const auto releaseUrl = stringValue(object, "html_url");
    constexpr std::string_view releasePrefix =
        "https://github.com/LuYishan-4/LunaDash/releases/";
    if (!parseVersion(latestVersion).valid ||
        !releaseUrl.starts_with(releasePrefix)) {
      state_["status"] = "error";
      state_["error"] = "GitHub returned unexpected release information.";
      state_["checkedAt"] = utcTimestamp();
      return;
    }
    state_["latestVersion"] = latestVersion;
    state_["releaseUrl"] = releaseUrl;
    state_["status"] = isNewerRelease(latestVersion, LUDASH_VERSION)
                           ? "available"
                           : "upToDate";
  }
  state_["error"] = "";
  state_["checkedAt"] = utcTimestamp();
}

} // namespace LunaDash
