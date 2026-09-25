#pragma once

#include <QByteArray>
#include <QJsonObject>

namespace LunaDash {
// 24 kHz stereo signed 16-bit little-endian PCM. No audio is persisted.
QJsonObject analyzeAudioSpectrum(const QByteArray &pcm);
int runAudioSpectrum();
} // namespace LunaDash
