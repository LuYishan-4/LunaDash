#include "config/localization/Localization.hpp"
#include "config/desktop/DesktopPreferenceStore.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;

std::vector<fs::path> catalogPaths(const fs::path &root,
                                   const std::string &locale) {
  std::vector<fs::path> paths;
  const auto main = root / (locale + ".json");
  std::error_code error;
  if (fs::is_regular_file(main, error))
    paths.push_back(main);
  error.clear();
  const auto directory = root / locale;
  if (fs::is_directory(directory, error)) {
    for (fs::directory_iterator entry(directory, error), end;
         !error && entry != end; entry.increment(error))
      if (entry->is_regular_file(error) && entry->path().extension() == ".json")
        paths.push_back(entry->path());
  }
  std::sort(paths.begin(), paths.end());
  return paths;
}

Json sourceDictionary(const fs::path &root, const std::string &locale) {
  Json result = Json::object();
  for (const auto &path : catalogPaths(root, locale)) {
    std::ifstream input(path, std::ios::binary);
    const auto catalog = Json::parse(input, nullptr, false);
    if (!input || catalog.is_discarded() || !catalog.is_object())
      return {};
    for (auto entry = catalog.begin(); entry != catalog.end(); ++entry)
      if (entry.value().is_string() && !entry.value().get<std::string>().empty() &&
          !result.contains(entry.key()))
        result[entry.key()] = entry.value();
  }
  return result;
}

int fail(std::string_view message, int code) {
  std::cerr << message << '\n';
  return code;
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  const fs::path translations(argv[1]);
  const std::vector<std::string> locales{"en_US", "ja_JP", "zh_CN", "zh_TW"};
  for (const auto &locale : locales) {
    if (!LunaDash::isSupportedLanguage(locale))
      return fail("A shipped locale was not found: " + locale, 3);
    const auto expected = sourceDictionary(translations, locale);
    if (locale != "en_US" && expected.empty())
      return fail("Could not read source catalogs for " + locale, 4);
    if (LunaDash::languageDictionary(locale) != expected)
      return fail("Loaded catalogs differ from source catalogs for " + locale,
                  5);
  }
  if (!LunaDash::languageDictionary("unsupported").empty() ||
      LunaDash::isSupportedLanguage("unsupported"))
    return fail("Unsupported locales must use the English source fallback.", 6);

  std::string directoryTemplate =
      (fs::temp_directory_path() / "lunadash-localization-XXXXXX").string();
  std::vector<char> temporary(directoryTemplate.begin(), directoryTemplate.end());
  temporary.push_back('\0');
  const char *temporaryDirectory = ::mkdtemp(temporary.data());
  if (!temporaryDirectory)
    return fail("Could not create a localization test directory.", 7);
  const fs::path config(temporaryDirectory);
  ::setenv("XDG_CONFIG_HOME", config.c_str(), 1);
  ::setenv("HOME", config.c_str(), 1);
  ::setenv("LANG", "C.UTF-8", 1);
  ::unsetenv("LC_ALL");
  ::unsetenv("LC_MESSAGES");
  ::unsetenv("LUDASH_LANGUAGE");

  const auto legacyPath = config / "LunaDash/LunaDash.conf";
  fs::create_directories(legacyPath.parent_path());
  {
    std::ofstream legacy(legacyPath);
    legacy << "[appearance]\nlanguage=zh_TW\n";
  }
  if (LunaDash::selectedLanguage() != "zh_TW")
    return fail("The legacy language preference was not read.", 8);

  std::string error;
  if (!LunaDash::setSelectedLanguage("ja_JP", &error) ||
      LunaDash::selectedLanguage() != "ja_JP")
    return fail("The native language preference was not persisted: " + error,
                9);
  if (LunaDash::setSelectedLanguage("unsupported", &error))
    return fail("An unsupported language was accepted.", 10);

  if (!LunaDash::resetDesktopPreferences(&error) ||
      !LunaDash::legacyLanguagePreferenceIgnored() ||
      LunaDash::selectedLanguage() != "en_US")
    return fail("Reset did not ignore the legacy language preference: " + error,
                11);

  ::setenv("LUDASH_LANGUAGE", "zh-Hans-CN.UTF-8", 1);
  if (LunaDash::selectedLanguage() != "zh_CN")
    return fail("Language environment overrides were not normalized.", 12);

  fs::remove_all(config);
  std::cout << "Verified native locale loading, fallback, legacy migration, reset and persistence.\n";
  return 0;
}
