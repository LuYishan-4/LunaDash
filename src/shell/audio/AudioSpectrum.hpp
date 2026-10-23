#pragma once

#include <nlohmann/json.hpp>

#include <string_view>

namespace LunaDash {
// 24 kHz stereo signed 16-bit little-endian PCM. No audio is persisted.
nlohmann::json analyzeAudioSpectrum(std::string_view pcm);
int runAudioSpectrum();
} // namespace LunaDash
