#include "shell/audio/AudioSpectrum.hpp"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QtEndian>
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <csignal>
#include <numbers>
#include <sys/prctl.h>
#include <unistd.h>

namespace LunaDash {
namespace {
constexpr int sampleCount = 1024;
constexpr int frameBytes = sampleCount * 4;
constexpr int hopBytes = frameBytes / 2;
using Transform = std::array<std::complex<double>, sampleCount>;

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

void publish(const QJsonObject &value) {
  const auto json = QJsonDocument(value).toJson(QJsonDocument::Compact) + '\n';
  if (fwrite(json.constData(), 1, json.size(), stdout) != size_t(json.size()) ||
      fflush(stdout) != 0)
    QCoreApplication::quit();
}
} // namespace

QJsonObject analyzeAudioSpectrum(const QByteArray &pcm) {
  if (pcm.size() != frameBytes)
    return {{"available", false}, {"error", "Invalid PCM frame size"}};
  Transform left{}, right{};
  double energy = 0;
  for (int i = 0; i < sampleCount; ++i) {
    const double l = qFromLittleEndian<qint16>(pcm.constData() + i * 4) / 32768.0;
    const double r = qFromLittleEndian<qint16>(pcm.constData() + i * 4 + 2) / 32768.0;
    energy += (l * l + r * r) / 2;
    const double window = 0.5 - 0.5 * std::cos(2 * std::numbers::pi * i / (sampleCount - 1));
    left[i] = l * window;
    right[i] = r * window;
  }
  const double rms = std::sqrt(energy / sampleCount);
  transform(left);
  transform(right);
  QJsonArray bands;
  for (int band = 0; band < 32; ++band) {
    const double low = 50 * std::pow(200.0, band / 32.0);
    const double high = 50 * std::pow(200.0, (band + 1) / 32.0);
    const int start = std::clamp(int(low * sampleCount / 24000), 1, sampleCount / 2 - 1);
    const int end = std::clamp(int(std::ceil(high * sampleCount / 24000)), start + 1, sampleCount / 2);
    double magnitude = 0;
    for (int bin = start; bin < end; ++bin)
      magnitude = std::max(magnitude, std::max(std::abs(left[bin]), std::abs(right[bin])) * 4 / sampleCount);
    // A bounded logarithmic scale keeps quiet music legible without AGC
    // amplifying silence; each band still follows measured PCM energy.
    const double level = rms < 0.00003 ? 0 : std::clamp(
        (20 * std::log10(std::max(1e-9, magnitude)) + 66) / 60, 0.0, 1.0);
    bands.append(std::pow(level, 1.4));
  }
  return {{"available", true}, {"rms", rms}, {"bands", bands}};
}

int runAudioSpectrum() {
  const auto executable = QStandardPaths::findExecutable("parec");
  if (executable.isEmpty()) {
    publish({{"available", false}, {"error", "Audio Wave needs parec (libpulse / pulseaudio-utils)."}});
    return 2;
  }
  QProcess capture;
  const auto parentPid = getpid();
  capture.setChildProcessModifier([parentPid] {
    // Stop capture even when Quickshell terminates this helper with SIGTERM.
    prctl(PR_SET_PDEATHSIG, SIGTERM);
    if (getppid() != parentPid)
      _exit(1);
  });
  QByteArray pending;
  QObject::connect(&capture, &QProcess::readyReadStandardOutput, &capture, [&] {
    pending += capture.readAllStandardOutput();
    // Skip old frames after a stall instead of flooding the QML event loop.
    if (pending.size() > frameBytes * 4) {
      const auto skip = ((pending.size() - frameBytes) / hopBytes) * hopBytes;
      pending.remove(0, skip);
    }
    QJsonObject latest;
    while (pending.size() >= frameBytes) {
      latest = analyzeAudioSpectrum(pending.first(frameBytes));
      pending.remove(0, hopBytes);
    }
    if (!latest.isEmpty())
      publish(latest);
  });
  QObject::connect(&capture, &QProcess::readyReadStandardError, &capture, [&] {
    capture.readAllStandardError();
  });
  QObject::connect(&capture, &QProcess::errorOccurred, &capture, [&](QProcess::ProcessError) {
    publish({{"available", false}, {"error", "Could not start the output audio monitor."}});
    QCoreApplication::exit(2);
  });
  QObject::connect(&capture, &QProcess::finished, &capture, [&](int, QProcess::ExitStatus) {
    publish({{"available", false}, {"error", "Output audio monitor disconnected."}});
    QCoreApplication::exit(2);
  });
  // Explicitly monitor playback, never the default microphone. The PulseAudio
  // protocol is also provided by PipeWire-Pulse. No shell or MPRIS polling.
  capture.start(executable, {"--device=@DEFAULT_MONITOR@", "--raw",
      "--format=s16le", "--rate=24000", "--channels=2", "--latency-msec=32",
      "--client-name=LunaDash Audio Wave", "--stream-name=Desktop spectrum"});
  const int result = QCoreApplication::exec();
  capture.disconnect();
  if (capture.state() != QProcess::NotRunning) {
    capture.terminate();
    if (!capture.waitForFinished(1000)) {
      capture.kill();
      capture.waitForFinished(1000);
    }
  }
  return result;
}
} // namespace LunaDash
