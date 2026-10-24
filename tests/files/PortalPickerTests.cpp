#include "service/portal/FileChooserOptions.hpp"

#include <array>
#include "support/TestHarness.hpp"

namespace LunaDash {

class PortalPickerTests final {
public:
  void saveFilesAcceptOnlyBasenames() {
    const auto valid =
        portalSaveFileNames(
            {"draft.txt", std::string("archive.tar.gz\0", 15)});
    VERIFY(valid.has_value());
    COMPARE(valid->size(), std::size_t{2});
    VERIFY((*valid)[0] == "draft.txt");
    VERIFY((*valid)[1] == "archive.tar.gz");

    const std::array<std::string, 5> invalidNames{
        "../escape.txt", "subdir/file.txt", ".", "..",
        std::string("bad\0name", 8)};
    for (const auto &invalid : invalidNames)
      VERIFY(!portalSaveFileNames({invalid}).has_value());
    VERIFY(!portalSaveFileNames({}).has_value());
  }
};

} // namespace LunaDash

int main(int argc, char **argv) {
  return LunaDash::Test::run<LunaDash::PortalPickerTests>(
      "PortalPickerTests",
      {
      {"saveFilesAcceptOnlyBasenames", &LunaDash::PortalPickerTests::saveFilesAcceptOnlyBasenames}},
      argc, argv);
}
