#include "config/localization/Localization.hpp"

#include "config/desktop/DesktopPreferenceStore.hpp"

#include <gio/gio.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#ifndef LUDASH_TRANSLATIONS_SOURCE_DIR
#define LUDASH_TRANSLATIONS_SOURCE_DIR ""
#endif

#ifndef LUDASH_TRANSLATIONS_INSTALL_DIR
#define LUDASH_TRANSLATIONS_INSTALL_DIR ""
#endif

namespace LunaDash {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;

fs::path configDirectory() {
  if (const char *xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg &&
      fs::path(xdg).is_absolute())
    return xdg;
  if (const char *home = std::getenv("HOME"); home && *home)
    return fs::path(home) / ".config";
  return {};
}

fs::path legacyLanguagePath() {
  const auto root = configDirectory();
  return root.empty() ? fs::path{} : root / "LunaDash/LunaDash.conf";
}

std::string normalizeLocale(std::string locale) {
  const auto first = locale.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return {};
  const auto last = locale.find_last_not_of(" \t\r\n");
  locale = locale.substr(first, last - first + 1);
  const auto suffix = locale.find_first_of(".@");
  if (suffix != std::string::npos)
    locale.erase(suffix);
  std::replace(locale.begin(), locale.end(), '-', '_');
  return locale;
}

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return value;
}

fs::path translationRoot() {
  const fs::path source(LUDASH_TRANSLATIONS_SOURCE_DIR);
  std::error_code error;
  if (!source.empty() && fs::is_directory(source, error))
    return source;
  return LUDASH_TRANSLATIONS_INSTALL_DIR;
}

std::string canonicalLanguage(std::string locale) {
  locale = normalizeLocale(std::move(locale));
  if (isSupportedLanguage(locale))
    return locale;
  const auto lower = lowercase(locale);
  if (lower.starts_with("zh_cn") || lower.starts_with("zh_hans"))
    return isSupportedLanguage("zh_CN") ? "zh_CN" : "en_US";
  if (lower.starts_with("zh"))
    return isSupportedLanguage("zh_TW") ? "zh_TW" : "en_US";
  if (lower.starts_with("ja"))
    return isSupportedLanguage("ja_JP") ? "ja_JP" : "en_US";
  if (lower.starts_with("en"))
    return "en_US";
  return "en_US";
}

std::string requestedLanguage(std::string locale) {
  locale = normalizeLocale(std::move(locale));
  if (isSupportedLanguage(locale))
    return locale;
  const auto lower = lowercase(locale);
  if (lower.starts_with("zh_cn") || lower.starts_with("zh_hans"))
    return isSupportedLanguage("zh_CN") ? "zh_CN" : std::string{};
  if (lower.starts_with("zh"))
    return isSupportedLanguage("zh_TW") ? "zh_TW" : std::string{};
  if (lower.starts_with("ja"))
    return isSupportedLanguage("ja_JP") ? "ja_JP" : std::string{};
  if (lower.starts_with("en"))
    return "en_US";
  return {};
}

std::string legacyLanguage() {
  const auto path = legacyLanguagePath();
  if (path.empty())
    return {};
  auto *file = g_key_file_new();
  GError *error = nullptr;
  if (!g_key_file_load_from_file(file, path.c_str(), G_KEY_FILE_NONE, &error)) {
    if (error)
      g_error_free(error);
    g_key_file_unref(file);
    return {};
  }
  char *value = g_key_file_get_string(file, "appearance", "language", &error);
  g_key_file_unref(file);
  if (error) {
    g_error_free(error);
    return {};
  }
  std::string result = value ? value : "";
  g_free(value);
  return result;
}

std::vector<fs::path> catalogPaths(std::string_view language) {
  const auto root = translationRoot();
  std::vector<fs::path> paths;
  const auto main = root / (std::string(language) + ".json");
  std::error_code error;
  if (fs::is_regular_file(main, error))
    paths.push_back(main);
  error.clear();
  const auto featureDirectory = root / language;
  if (fs::is_directory(featureDirectory, error)) {
    for (fs::directory_iterator entry(featureDirectory, error), end;
         !error && entry != end; entry.increment(error)) {
      if (entry->is_regular_file(error) && entry->path().extension() == ".json")
        paths.push_back(entry->path());
      error.clear();
    }
  }
  std::sort(paths.begin(), paths.end());
  return paths;
}

Json readCatalog(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return Json::object();
  auto catalog = Json::parse(input, nullptr, false);
  if (catalog.is_discarded() || !catalog.is_object()) {
    std::cerr << "Invalid translation catalog: " << path << '\n';
    return Json::object();
  }
  return catalog;
}

} // namespace

bool isSupportedLanguage(std::string_view language) {
  const auto locale = normalizeLocale(std::string(language));
  if (locale == "en_US")
    return true;
  const auto root = translationRoot();
  if (root.empty() || locale.empty())
    return false;
  std::error_code error;
  return fs::is_regular_file(root / (locale + ".json"), error) ||
         fs::is_directory(root / locale, error);
}

Json languageDictionary(std::string_view language) {
  const auto locale = normalizeLocale(std::string(language));
  if (!isSupportedLanguage(locale) || locale == "en_US")
    return Json::object();

  static std::mutex cacheMutex;
  static std::map<std::string, Json, std::less<>> cache;
  std::lock_guard lock(cacheMutex);
  if (const auto cached = cache.find(locale); cached != cache.end())
    return cached->second;

  Json messages = Json::object();
  for (const auto &path : catalogPaths(locale)) {
    const auto catalog = readCatalog(path);
    for (auto entry = catalog.begin(); entry != catalog.end(); ++entry) {
      if (!entry.value().is_string() ||
          entry.value().get_ref<const std::string &>().empty())
        continue;
      if (messages.contains(entry.key())) {
        if (messages.at(entry.key()) != entry.value())
          std::cerr << "Conflicting translation key: " << path << " "
                    << entry.key() << '\n';
        continue;
      }
      messages[entry.key()] = entry.value();
    }
  }
  cache.emplace(locale, messages);
  return messages;
}

std::string selectedLanguage() {
  if (const char *environment = std::getenv("LUDASH_LANGUAGE");
      environment && *environment)
    return canonicalLanguage(environment);

  const auto preferences = loadDesktopPreferences();
  const auto configured = preferences.find("language");
  if (configured != preferences.end() && configured->is_string())
    return canonicalLanguage(configured->get<std::string>());

  if (!legacyLanguagePreferenceIgnored()) {
    const auto legacy = legacyLanguage();
    if (!legacy.empty())
      return canonicalLanguage(legacy);
  }

  for (const auto *name : {"LC_ALL", "LC_MESSAGES", "LANG"})
    if (const char *locale = std::getenv(name); locale && *locale)
      return canonicalLanguage(locale);
  return "en_US";
}

bool setSelectedLanguage(std::string_view language, std::string *error) {
  if (error)
    error->clear();
  const auto locale = requestedLanguage(std::string(language));
  if (locale.empty()) {
    if (error)
      *error = "The requested language is not supported.";
    return false;
  }
  return saveDesktopPreferences(Json{{"language", locale}}, error);
}

} // namespace LunaDash
