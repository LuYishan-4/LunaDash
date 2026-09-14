#include <LuDash/file_picker/FilePicker.h>

#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

namespace LuDash {
class FilePickerTests : public QObject {
  Q_OBJECT
private slots:
  void previewSizePreservesAspectRatio() {
    QCOMPARE(boundedPreviewSize({4000, 2000}, {800, 600}), QSize(800, 400));
    QCOMPARE(boundedPreviewSize({200, 100}, {800, 600}), QSize(200, 100));
    QCOMPARE(boundedPreviewSize({}, {800, 600}), QSize());
    QCOMPARE(boundedPreviewSize({100, 100}, {}), QSize());
  }

  void eligibilityRequiresARegularSupportedImage() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString pngPath = directory.filePath("image.png");
    QImage image(16, 12, QImage::Format_RGB32);
    image.fill(Qt::cyan);
    QVERIFY(image.save(pngPath));
    QVERIFY(isEligibleImageFile(pngPath));
    QVERIFY(!isEligibleImageFile(directory.path()));
    QVERIFY(!isEligibleImageFile(directory.filePath("missing.png")));

    const QString disguisedPath = directory.filePath("image.txt");
    QVERIFY(QFile::copy(pngPath, disguisedPath));
    QVERIFY(!isEligibleImageFile(disguisedPath));
  }
};
} // namespace LuDash

QTEST_GUILESS_MAIN(LuDash::FilePickerTests)
#include "FilePickerTests.moc"
