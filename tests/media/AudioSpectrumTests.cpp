#include "shell/audio/AudioSpectrum.hpp"
#include <QJsonArray>
#include <QtEndian>
#include <QtTest>
#include <cmath>
#include <numbers>

namespace LunaDash {
class AudioSpectrumTests : public QObject {
  Q_OBJECT
private slots:
  void silence() {
    const auto frame = analyzeAudioSpectrum(QByteArray(4096, '\0'));
    QVERIFY(frame.value("available").toBool());
    QCOMPARE(frame.value("rms").toDouble(), 0.0);
    QCOMPARE(frame.value("bands").toArray().size(), 32);
    for (const auto &band : frame.value("bands").toArray())
      QCOMPARE(band.toDouble(), 0.0);
    QVERIFY(!analyzeAudioSpectrum(QByteArray(7, '\0')).value("available").toBool());
  }
  void stereoTone() {
    // Opposite phase channels must not cancel the measured playback energy.
    QByteArray pcm(4096, '\0');
    for (int i = 0; i < 1024; ++i) {
      const auto sample = qint16(0.08 * 32767 * std::sin(2 * std::numbers::pi * 1000 * i / 24000));
      qToLittleEndian<qint16>(sample, pcm.data() + i * 4);
      qToLittleEndian<qint16>(-sample, pcm.data() + i * 4 + 2);
    }
    const auto frame = analyzeAudioSpectrum(pcm);
    QVERIFY(frame.value("rms").toDouble() > 0.05);
    const auto bands = frame.value("bands").toArray();
    int peak = 0;
    for (int i = 0; i < bands.size(); ++i) {
      QVERIFY(bands[i].toDouble() >= 0 && bands[i].toDouble() <= 1);
      if (bands[i].toDouble() > bands[peak].toDouble()) peak = i;
    }
    QVERIFY(peak >= 17 && peak <= 19);
    QVERIFY(bands[peak].toDouble() > 0.5);
    QVERIFY(bands[0].toDouble() < 0.1);
  }
};
} // namespace LunaDash
QTEST_GUILESS_MAIN(LunaDash::AudioSpectrumTests)
#include "AudioSpectrumTests.moc"
