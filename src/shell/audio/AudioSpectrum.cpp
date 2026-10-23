#include "shell/audio/AudioSpectrum.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <cmath>
#include <complex>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <numbers>
#include <poll.h>
#include <spawn.h>
#include <string>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

namespace LunaDash {
namespace {
constexpr int sampleCount = 1024;
constexpr std::size_t frameBytes = sampleCount * 4;
constexpr std::size_t hopBytes = frameBytes / 2;
using Transform = std::array<std::complex<double>, sampleCount>;

volatile sig_atomic_t stopRequested = 0;

void requestStop(int) { stopRequested = 1; }

void transform(Transform &data) {
  for (int i = 1, j = 0; i < sampleCount; ++i) {
    int bit = sampleCount >> 1;
    for (; j & bit; bit >>= 1)
      j ^= bit;
    j ^= bit;
    if (i < j)
      std::swap(data[i], data[j]);
  }
  for (int length = 2; length <= sampleCount; length <<= 1) {
    const auto step = std::polar(1.0, -2.0 * std::numbers::pi / length);
    for (int i = 0; i < sampleCount; i += length) {
      std::complex<double> weight{1, 0};
      for (int j = 0; j < length / 2; ++j) {
        const auto even = data[i + j];
        const auto odd = data[i + j + length / 2] * weight;
        data[i + j] = even + odd;
        data[i + j + length / 2] = even - odd;
        weight *= step;
      }
    }
  }
}

int16_t littleEndianSample(std::string_view pcm, std::size_t offset) {
  const auto low = static_cast<uint8_t>(pcm[offset]);
  const auto high = static_cast<uint8_t>(pcm[offset + 1]);
  const auto bits = static_cast<uint16_t>(
      static_cast<uint16_t>(low) | (static_cast<uint16_t>(high) << 8));
  return std::bit_cast<int16_t>(bits);
}

bool publish(const nlohmann::json &value) {
  const auto json = value.dump();
  return std::fwrite(json.data(), 1, json.size(), stdout) == json.size() &&
         std::fputc('\n', stdout) != EOF && std::fflush(stdout) == 0;
}

void closeFd(int &fd) {
  if (fd >= 0) {
    ::close(fd);
    fd = -1;
  }
}

bool installSignalHandlers() {
  struct sigaction action {};
  action.sa_handler = requestStop;
  sigemptyset(&action.sa_mask);
  if (::sigaction(SIGTERM, &action, nullptr) != 0 ||
      ::sigaction(SIGINT, &action, nullptr) != 0)
    return false;
  struct sigaction ignorePipe {};
  ignorePipe.sa_handler = SIG_IGN;
  sigemptyset(&ignorePipe.sa_mask);
  return ::sigaction(SIGPIPE, &ignorePipe, nullptr) == 0;
}

int spawnCapture(pid_t *child, int *readFd) {
  int pipeFds[2] = {-1, -1};
  if (::pipe2(pipeFds, O_CLOEXEC) != 0)
    return errno;
  const int readFlags = ::fcntl(pipeFds[0], F_GETFL, 0);
  if (readFlags < 0 ||
      ::fcntl(pipeFds[0], F_SETFL, readFlags | O_NONBLOCK) < 0) {
    const int error = errno;
    closeFd(pipeFds[0]);
    closeFd(pipeFds[1]);
    return error;
  }

  posix_spawn_file_actions_t actions;
  int error = ::posix_spawn_file_actions_init(&actions);
  const bool actionsInitialized = error == 0;
  if (error == 0)
    error = ::posix_spawn_file_actions_adddup2(&actions, pipeFds[1],
                                              STDOUT_FILENO);
  if (error == 0 && pipeFds[0] != STDOUT_FILENO && pipeFds[0] != STDERR_FILENO)
    error = ::posix_spawn_file_actions_addclose(&actions, pipeFds[0]);
  if (error == 0 && pipeFds[1] != STDOUT_FILENO && pipeFds[1] != STDERR_FILENO)
    error = ::posix_spawn_file_actions_addclose(&actions, pipeFds[1]);
  if (error == 0)
    error = ::posix_spawn_file_actions_addopen(&actions, STDERR_FILENO,
                                               "/dev/null", O_WRONLY, 0);

  char program[] = "parec";
  char *arguments[] = {
      program,
      const_cast<char *>("--device=@DEFAULT_MONITOR@"),
      const_cast<char *>("--raw"),
      const_cast<char *>("--format=s16le"),
      const_cast<char *>("--rate=24000"),
      const_cast<char *>("--channels=2"),
      const_cast<char *>("--latency-msec=32"),
      const_cast<char *>("--client-name=LunaDash Audio Wave"),
      const_cast<char *>("--stream-name=Desktop spectrum"),
      nullptr};
  if (error == 0)
    error = ::posix_spawnp(child, program, &actions, nullptr, arguments, environ);
  if (actionsInitialized)
    ::posix_spawn_file_actions_destroy(&actions);
  closeFd(pipeFds[1]);
  if (error != 0) {
    closeFd(pipeFds[0]);
    return error;
  }
  *readFd = pipeFds[0];
  return 0;
}

void stopCapture(pid_t child, int &readFd) {
  closeFd(readFd);
  if (child <= 0)
    return;
  ::kill(child, SIGTERM);
  int status = 0;
  for (int attempt = 0; attempt < 100; ++attempt) {
    const pid_t result = ::waitpid(child, &status, WNOHANG);
    if (result == child || (result < 0 && errno == ECHILD))
      return;
    if (result < 0 && errno != EINTR)
      return;
    ::poll(nullptr, 0, 10);
  }
  ::kill(child, SIGKILL);
  while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {
  }
}

} // namespace

nlohmann::json analyzeAudioSpectrum(std::string_view pcm) {
  if (pcm.size() != frameBytes)
    return {{"available", false}, {"error", "Invalid PCM frame size"}};

  Transform left{}, right{};
  double energy = 0;
  for (int i = 0; i < sampleCount; ++i) {
    const auto offset = static_cast<std::size_t>(i) * 4;
    const double l = littleEndianSample(pcm, offset) / 32768.0;
    const double r = littleEndianSample(pcm, offset + 2) / 32768.0;
    energy += (l * l + r * r) / 2;
    const double window =
        0.5 - 0.5 * std::cos(2 * std::numbers::pi * i / (sampleCount - 1));
    left[i] = l * window;
    right[i] = r * window;
  }
  const double rms = std::sqrt(energy / sampleCount);
  transform(left);
  transform(right);
  auto bands = nlohmann::json::array();
  for (int band = 0; band < 32; ++band) {
    const double low = 50 * std::pow(200.0, band / 32.0);
    const double high = 50 * std::pow(200.0, (band + 1) / 32.0);
    const int start =
        std::clamp(int(low * sampleCount / 24000), 1, sampleCount / 2 - 1);
    const int end = std::clamp(int(std::ceil(high * sampleCount / 24000)),
                               start + 1, sampleCount / 2);
    double magnitude = 0;
    for (int bin = start; bin < end; ++bin)
      magnitude = std::max(
          magnitude,
          std::max(std::abs(left[bin]), std::abs(right[bin])) * 4 /
              sampleCount);
    // A bounded logarithmic scale keeps quiet music legible without AGC
    // amplifying silence; each band still follows measured PCM energy.
    const double level =
        rms < 0.00003
            ? 0
            : std::clamp((20 * std::log10(std::max(1e-9, magnitude)) + 66) / 60,
                         0.0, 1.0);
    bands.push_back(std::pow(level, 1.4));
  }
  return {{"available", true}, {"rms", rms}, {"bands", std::move(bands)}};
}

int runAudioSpectrum() {
  const pid_t parentPid = ::getppid();
  if (::prctl(PR_SET_PDEATHSIG, SIGTERM) == 0 && ::getppid() != parentPid)
    return 2;
  if (!installSignalHandlers()) {
    publish({{"available", false},
             {"error", "Could not install audio monitor signal handlers."}});
    return 2;
  }

  pid_t child = -1;
  int captureFd = -1;
  const int spawnError = spawnCapture(&child, &captureFd);
  if (spawnError != 0) {
    const bool missing = spawnError == ENOENT;
    publish({{"available", false},
             {"error", missing
                           ? "Audio Wave needs parec (libpulse / pulseaudio-utils)."
                           : "Could not start the output audio monitor."}});
    return 2;
  }

  std::string pending;
  std::array<char, 8192> buffer{};
  bool disconnected = false;
  bool outputFailed = false;
  int childStatus = 0;
  while (!stopRequested) {
    pollfd descriptor{captureFd, POLLIN | POLLHUP | POLLERR, 0};
    const int ready = ::poll(&descriptor, 1, 1000);
    if (ready < 0 && errno != EINTR) {
      disconnected = true;
      break;
    }
    if (ready > 0 && descriptor.revents != 0) {
      for (;;) {
        const ssize_t count = ::read(captureFd, buffer.data(), buffer.size());
        if (count > 0) {
          pending.append(buffer.data(), static_cast<std::size_t>(count));
          if (pending.size() > frameBytes * 4) {
            const auto skip =
                ((pending.size() - frameBytes) / hopBytes) * hopBytes;
            pending.erase(0, skip);
          }
          nlohmann::json latest;
          while (pending.size() >= frameBytes) {
            latest = analyzeAudioSpectrum(
                std::string_view(pending.data(), frameBytes));
            pending.erase(0, hopBytes);
          }
          if (!latest.is_null() && !publish(latest)) {
            outputFailed = true;
            break;
          }
          continue;
        }
        if (count == 0) {
          disconnected = true;
          break;
        }
        if (errno == EINTR)
          continue;
        if (errno != EAGAIN && errno != EWOULDBLOCK)
          disconnected = true;
        else if (descriptor.revents & (POLLHUP | POLLERR))
          disconnected = true;
        break;
      }
      if (disconnected || outputFailed)
        break;
    }

    const pid_t result = ::waitpid(child, &childStatus, WNOHANG);
    if (result == child || (result < 0 && errno == ECHILD)) {
      child = -1;
      disconnected = true;
      break;
    }
    if (result < 0 && errno != EINTR) {
      child = -1;
      disconnected = true;
      break;
    }
  }

  stopCapture(child, captureFd);
  if (outputFailed)
    return 2;
  if (!stopRequested || disconnected) {
    publish({{"available", false},
             {"error", "Output audio monitor disconnected."}});
    return 2;
  }
  return 0;
}

} // namespace LunaDash
