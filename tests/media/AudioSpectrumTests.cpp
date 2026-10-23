#include "shell/audio/AudioSpectrum.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <string>

namespace {
void writeLittleEndian(std::string &pcm, std::size_t offset, int16_t sample) {
  const auto bits = std::bit_cast<uint16_t>(sample);
  pcm[offset] = static_cast<char>(bits & 0xff);
  pcm[offset + 1] = static_cast<char>(bits >> 8);
}
} // namespace

int main() {
  const auto silence = LunaDash::analyzeAudioSpectrum(std::string(4096, '\0'));
  if (!silence.value("available", false) ||
      silence.value("rms", -1.0) != 0.0 ||
      silence.value("bands", nlohmann::json::array()).size() != 32 ||
      std::any_of(silence["bands"].begin(), silence["bands"].end(),
                  [](const nlohmann::json &band) {
                    return band.get<double>() != 0.0;
                  }) ||
      LunaDash::analyzeAudioSpectrum(std::string(7, '\0'))
          .value("available", true)) {
    std::cerr << "Audio spectrum silence/size contract failed.\n";
    return 1;
  }

  // Opposite phase channels must not cancel the measured playback energy.
  std::string pcm(4096, '\0');
  for (int i = 0; i < 1024; ++i) {
    const auto sample = static_cast<int16_t>(
        0.08 * 32767 * std::sin(2 * std::numbers::pi * 1000 * i / 24000));
    const auto offset = static_cast<std::size_t>(i) * 4;
    writeLittleEndian(pcm, offset, sample);
    writeLittleEndian(pcm, offset + 2, static_cast<int16_t>(-sample));
  }
  const auto tone = LunaDash::analyzeAudioSpectrum(pcm);
  const auto &bands = tone["bands"];
  std::size_t peak = 0;
  for (std::size_t index = 0; index < bands.size(); ++index) {
    const double value = bands[index].get<double>();
    if (value < 0 || value > 1) {
      std::cerr << "Audio spectrum band is out of range.\n";
      return 1;
    }
    if (value > bands[peak].get<double>())
      peak = index;
  }
  if (tone.value("rms", 0.0) <= 0.05 || peak < 17 || peak > 19 ||
      bands[peak].get<double>() <= 0.5 || bands[0].get<double>() >= 0.1) {
    std::cerr << "Audio spectrum stereo tone contract failed.\n";
    return 1;
  }

  std::cout << "Native audio spectrum silence and stereo tone tests passed.\n";
  return 0;
}
