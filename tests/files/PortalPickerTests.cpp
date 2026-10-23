#include "service/portal/FileChooserOptions.hpp"

#include <array>
#include <QtTest>

namespace LunaDash {

class PortalPickerTests final : public QObject {
  Q_OBJECT

private slots:
  void saveFilesAcceptOnlyBasenames() {
    const auto valid =
        portalSaveFileNames(
            {"draft.txt", std::string("archive.tar.gz\0", 15)});
    QVERIFY(valid.has_value());
    QCOMPARE(valid->size(), std::size_t{2});
    QVERIFY((*valid)[0] == "draft.txt");
    QVERIFY((*valid)[1] == "archive.tar.gz");

    const std::array<std::string, 5> invalidNames{
        "../escape.txt", "subdir/file.txt", ".", "..",
        std::string("bad\0name", 8)};
    for (const auto &invalid : invalidNames)
      QVERIFY(!portalSaveFileNames({invalid}).has_value());
    QVERIFY(!portalSaveFileNames({}).has_value());
  }
};

} // namespace LunaDash

QTEST_GUILESS_MAIN(LunaDash::PortalPickerTests)
#include "PortalPickerTests.moc"
