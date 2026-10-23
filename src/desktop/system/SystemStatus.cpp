#include "desktop/system/SystemStatus.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <pwd.h>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>

namespace LunaDash {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;

constexpr std::size_t kStatusFileLimit = 128 * 1024;
constexpr std::size_t kHistoryLimit = 20;
constexpr int kRefreshIntervalMilliseconds = 3000;

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::string readStatusFile(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return {};
  std::string contents(kStatusFileLimit, '\0');
  input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
  contents.resize(static_cast<std::size_t>(input.gcount()));
  return trim(std::move(contents));
}

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
    const fs::path directory =
        component.empty() ? fs::current_path() : fs::path(component);
    const auto candidate = directory / name;
    if (::access(candidate.c_str(), X_OK) == 0)
      return candidate.string();
    if (end == std::string::npos)
      break;
    begin = end + 1;
  }
  return {};
}

std::string currentUser() {
  if (const char *user = std::getenv("USER"); user && *user)
    return user;
  const auto *entry = ::getpwuid(::getuid());
  return entry && entry->pw_name ? entry->pw_name : std::string{};
}

std::string displayNameForUser(const std::string &user) {
  std::ifstream input("/etc/passwd");
  std::string line;
  while (std::getline(input, line)) {
    std::array<std::string, 7> fields;
    std::size_t field = 0;
    std::size_t start = 0;
    for (std::size_t index = 0; index <= line.size() && field < fields.size();
         ++index) {
      if (index == line.size() || line[index] == ':') {
        fields[field++] = line.substr(start, index - start);
        start = index + 1;
      }
    }
    if (field < 5 || fields[0] != user)
      continue;
    const auto comma = fields[4].find(',');
    const auto name = trim(fields[4].substr(0, comma));
    return name.empty() ? user : name;
  }
  return user;
}

std::string osPrettyName() {
  std::ifstream input("/etc/os-release");
  std::string line;
  while (std::getline(input, line)) {
    if (!line.starts_with("PRETTY_NAME="))
      continue;
    auto value = line.substr(std::string_view("PRETTY_NAME=").size());
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                              (value.front() == '\'' && value.back() == '\'')))
      value = value.substr(1, value.size() - 2);
    return value;
  }
  struct utsname system{};
  return ::uname(&system) == 0 ? system.sysname : "Linux";
}

std::string fileUrl(const fs::path &path) {
  static constexpr char hex[] = "0123456789ABCDEF";
  std::string url = "file://";
  for (const unsigned char character : path.string()) {
    if (std::isalnum(character) || character == '/' || character == '-' ||
        character == '_' || character == '.' || character == '~') {
      url.push_back(static_cast<char>(character));
    } else {
      url.push_back('%');
      url.push_back(hex[character >> 4]);
      url.push_back(hex[character & 0x0f]);
    }
  }
  return url;
}

std::string avatarForUser(const std::string &user) {
  const char *homeValue = std::getenv("HOME");
  if (!homeValue || !*homeValue)
    return {};
  const fs::path home(homeValue);
  const std::array candidates{home / ".face.icon", home / ".face",
                              fs::path("/var/lib/AccountsService/icons") /
                                  user};
  for (const auto &path : candidates) {
    std::error_code error;
    if (fs::is_regular_file(path, error) && ::access(path.c_str(), R_OK) == 0) {
      const auto canonical = fs::canonical(path, error);
      if (!error)
        return fileUrl(canonical);
    }
  }
  return {};
}

std::string driverForCard(const fs::path &card) {
  std::error_code error;
  const auto target = fs::read_symlink(card / "device/driver", error);
  return error ? std::string{} : target.filename().string();
}

std::string friendlyCardName(const fs::path &card) {
  for (const auto *name : {"product_name", "model", "name"}) {
    const auto value = readStatusFile(card / "device" / name);
    if (!value.empty())
      return value;
  }
  return {};
}

std::string mountSourceFor(const fs::path &path) {
  std::ifstream input("/proc/self/mounts");
  std::string source;
  std::string mountPoint;
  std::string filesystem;
  std::string options;
  int dump = 0;
  int pass = 0;
  std::string bestSource;
  std::size_t bestLength = 0;
  const auto target = path.lexically_normal().string();
  while (input >> source >> mountPoint >> filesystem >> options >> dump >>
         pass) {
    for (std::size_t index = 0;
         (index = mountPoint.find('\\', index)) != std::string::npos; ++index) {
      if (index + 3 < mountPoint.size() && mountPoint[index + 1] >= '0' &&
          mountPoint[index + 1] <= '7' && mountPoint[index + 2] >= '0' &&
          mountPoint[index + 2] <= '7' && mountPoint[index + 3] >= '0' &&
          mountPoint[index + 3] <= '7') {
        const int decoded = (mountPoint[index + 1] - '0') * 64 +
                            (mountPoint[index + 2] - '0') * 8 +
                            (mountPoint[index + 3] - '0');
        mountPoint.replace(index, 4, 1, static_cast<char>(decoded));
      }
    }
    const bool contains =
        target == mountPoint ||
        (target.starts_with(mountPoint) &&
         (mountPoint == "/" || target.size() == mountPoint.size() ||
          target[mountPoint.size()] == '/'));
    if (contains && mountPoint.size() >= bestLength) {
      bestLength = mountPoint.size();
      bestSource = source;
    }
  }
  return bestSource.empty() ? target : bestSource;
}

bool parseInteger(std::string_view text, int *value) {
  const auto cleaned = trim(std::string(text));
  const auto [end, error] =
      std::from_chars(cleaned.data(), cleaned.data() + cleaned.size(), *value);
  return error == std::errc{} && end == cleaned.data() + cleaned.size();
}

void appendHistory(Json &history, int value) {
  history.push_back(value);
  if (history.size() > kHistoryLimit)
    history.erase(history.begin());
}

} // namespace

SystemStatus::SystemStatus(wl_event_loop *eventLoop)
    : eventLoop_(eventLoop), gpuQuery_(eventLoop) {
  struct utsname system{};
  const bool systemKnown = ::uname(&system) == 0;
  const auto user = currentUser();
  data_ = {{"os", osPrettyName()},
           {"kernel", systemKnown ? system.release : "unknown"},
           {"architecture", systemKnown ? system.machine : "unknown"},
           {"host", systemKnown ? system.nodename : "unknown"},
           {"user", user},
           {"displayName", displayNameForUser(user)},
           {"avatar", avatarForUser(user)},
           {"gpuAvailable", false},
           {"gpuPercent", -1}};

  const auto cpuInfo = readStatusFile("/proc/cpuinfo");
  std::istringstream cpuLines(cpuInfo);
  std::string line;
  while (std::getline(cpuLines, line)) {
    const auto separator = line.find(':');
    if (separator == std::string::npos)
      continue;
    const auto field = trim(line.substr(0, separator));
    if (field == "model name" || field == "Hardware") {
      data_["cpuModel"] = trim(line.substr(separator + 1));
      break;
    }
  }

  nvidiaSmi_ = findExecutable("nvidia-smi");
  refresh();
  refreshTimer_ = wl_event_loop_add_timer(eventLoop_, handleRefresh, this);
  if (refreshTimer_)
    wl_event_source_timer_update(refreshTimer_, kRefreshIntervalMilliseconds);
}

SystemStatus::~SystemStatus() {
  if (refreshTimer_)
    wl_event_source_remove(refreshTimer_);
  refreshTimer_ = nullptr;
}

Json SystemStatus::snapshot() const { return data_; }

int SystemStatus::handleRefresh(void *data) {
  auto *status = static_cast<SystemStatus *>(data);
  status->refresh();
  if (status->refreshTimer_)
    wl_event_source_timer_update(status->refreshTimer_,
                                 kRefreshIntervalMilliseconds);
  return 0;
}

void SystemStatus::refreshGpu() {
  int busy = -1;
  std::string model;
  std::string driver;
  bool nvidia = false;
  std::error_code error;
  const fs::path drm("/sys/class/drm");
  fs::directory_iterator entry(drm, error);
  const fs::directory_iterator end;
  for (; !error && entry != end; entry.increment(error)) {
    const auto name = entry->path().filename().string();
    if (!name.starts_with("card") || name.size() <= 4 ||
        !std::all_of(name.begin() + 4, name.end(), [](unsigned char character) {
          return std::isdigit(character) != 0;
        }))
      continue;
    const auto &card = entry->path();
    const auto cardDriver = driverForCard(card);
    if (driver.empty() && !cardDriver.empty())
      driver = cardDriver;
    const auto vendor = readStatusFile(card / "device/vendor");
    nvidia = nvidia || vendor == "0x10de" || cardDriver == "nvidia";

    int current = -1;
    if (parseInteger(readStatusFile(card / "device/gpu_busy_percent"),
                     &current))
      busy = std::max(busy, std::clamp(current, 0, 100));
    if (model.empty())
      model = friendlyCardName(card);
  }

  if (!driver.empty())
    data_["gpuDriver"] = driver;
  if (!model.empty())
    data_["gpuModel"] = model;

  if (busy >= 0) {
    data_["gpuAvailable"] = true;
    data_["gpuPercent"] = busy;
    appendHistory(data_["gpuHistory"], busy);
    return;
  }

  if ((nvidia || driver == "nvidia") && !nvidiaSmi_.empty()) {
    if (!gpuQuery_.busy())
      gpuQuery_.run(
          nvidiaSmi_,
          {"--query-gpu=name,utilization.gpu", "--format=csv,noheader,nounits"},
          [this](bool success, const std::string &output) {
            finishGpuQuery(success, output);
          },
          1500);
    return;
  }

  data_["gpuAvailable"] = false;
  data_["gpuPercent"] = -1;
}

void SystemStatus::finishGpuQuery(bool success, const std::string &output) {
  if (!success) {
    data_["gpuAvailable"] = false;
    data_["gpuPercent"] = -1;
    return;
  }

  int busy = -1;
  std::vector<std::string> models;
  std::istringstream lines(output);
  std::string line;
  while (std::getline(lines, line)) {
    line = trim(std::move(line));
    if (line.empty())
      continue;
    const auto separator = line.rfind(',');
    if (separator == std::string::npos || separator == 0)
      continue;
    int current = -1;
    if (!parseInteger(line.substr(separator + 1), &current))
      continue;
    const auto name = trim(line.substr(0, separator));
    if (!name.empty() &&
        std::find(models.begin(), models.end(), name) == models.end())
      models.push_back(name);
    busy = std::max(busy, std::clamp(current, 0, 100));
  }

  if (busy < 0) {
    data_["gpuAvailable"] = false;
    data_["gpuPercent"] = -1;
    return;
  }

  data_["gpuAvailable"] = true;
  data_["gpuPercent"] = busy;
  data_["gpuDriver"] = "nvidia";
  if (!models.empty()) {
    std::string joined;
    for (const auto &model : models) {
      if (!joined.empty())
        joined += " / ";
      joined += model;
    }
    data_["gpuModel"] = joined;
  }
  appendHistory(data_["gpuHistory"], busy);
}

void SystemStatus::refresh() {
  const auto cpuText = readStatusFile("/proc/stat");
  LuDashCpuCounters counters{};
  int cpu = 0;
  if (ludash_parse_cpu(cpuText.data(), cpuText.size(), &counters)) {
    cpu = ludash_cpu_percent(previous_, counters);
    previous_ = counters;
  }
  appendHistory(data_["cpuHistory"], cpu);
  data_["cpuPercent"] = cpu;

  const auto memoryText = readStatusFile("/proc/meminfo");
  LuDashMemoryCounters memory{};
  ludash_parse_memory(memoryText.data(), memoryText.size(), &memory);
  data_["memoryUsed"] =
      static_cast<double>(ludash_memory_used(memory)) / (1024.0 * 1024.0);
  data_["memoryTotal"] =
      static_cast<double>(memory.total_kib) / (1024.0 * 1024.0);
  data_["memoryPercent"] = ludash_memory_percent(memory);

  const bool slowSample = (refreshCount_ % 3) == 0;
  const bool gpuSample = (refreshCount_ % 2) == 0;
  if (slowSample) {
    const char *homeValue = std::getenv("HOME");
    const fs::path home = homeValue && *homeValue ? homeValue : "/";
    struct statvfs storage{};
    if (::statvfs(home.c_str(), &storage) == 0) {
      const auto total =
          static_cast<std::uint64_t>(storage.f_blocks) * storage.f_frsize;
      const auto available =
          static_cast<std::uint64_t>(storage.f_bavail) * storage.f_frsize;
      const auto used = total > available ? total - available : 0;
      data_["diskUsed"] =
          static_cast<double>(used) / (1024.0 * 1024.0 * 1024.0);
      data_["diskTotal"] =
          static_cast<double>(total) / (1024.0 * 1024.0 * 1024.0);
      data_["diskPercent"] =
          total > 0 ? static_cast<int>(100.0 * static_cast<double>(used) /
                                       static_cast<double>(total))
                    : 0;
      data_["diskDevice"] = mountSourceFor(home);
    }

    int battery = -1;
    std::error_code error;
    for (fs::directory_iterator entry("/sys/class/power_supply", error), end;
         !error && entry != end; entry.increment(error)) {
      const auto base = entry->path();
      if (readStatusFile(base / "type") != "Battery")
        continue;
      int capacity = -1;
      if (parseInteger(readStatusFile(base / "capacity"), &capacity)) {
        battery = std::clamp(capacity, 0, 100);
        break;
      }
    }
    data_["batteryPercent"] = battery;
  }

  if (gpuSample)
    refreshGpu();
  ++refreshCount_;
}

} // namespace LunaDash
