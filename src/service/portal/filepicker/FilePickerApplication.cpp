#include "service/portal/filepicker/FilePickerApplication.hpp"

#include "core/process/ProcessRunner.hpp"
#include "FilePicker.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gio.h>
#include <nlohmann/json.hpp>
#include <slint.h>
#include <wayland-server-core.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fnmatch.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace LunaDash {
namespace {

using Json = nlohmann::json;
namespace fs = std::filesystem;
using namespace std::string_literals;

volatile std::sig_atomic_t terminateRequested = 0;

void requestTermination(int) { terminateRequested = 1; }

class TemporaryDirectory final {
public:
  TemporaryDirectory() {
    auto pattern = (fs::temp_directory_path() / "lunadash-filepicker-XXXXXX").string();
    std::vector<char> writable(pattern.begin(), pattern.end());
    writable.push_back('\0');
    if (char *created = ::mkdtemp(writable.data()))
      path_ = created;
  }

  ~TemporaryDirectory() {
    if (!path_.empty()) {
      std::error_code error;
      fs::remove_all(path_, error);
    }
  }

  TemporaryDirectory(const TemporaryDirectory &) = delete;
  TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;
  const fs::path &path() const { return path_; }

private:
  fs::path path_;
};

struct ProcessResult {
  bool started = false;
  bool success = false;
  std::string output;
};

ProcessResult runProcess(wl_event_loop *eventLoop, const std::string &program,
                         const std::vector<std::string> &arguments,
                         int timeoutMilliseconds) {
  ProcessResult result;
  ProcessRunner runner(eventLoop);
  bool completed = false;
  result.started = runner.run(
      program, arguments,
      [&result, &completed](bool success, const std::string &output) {
        result.success = success;
        result.output = output;
        completed = true;
      },
      timeoutMilliseconds);
  while (result.started && !completed) {
    if (wl_event_loop_dispatch(eventLoop, 20) < 0) {
      runner.cancel();
      return result;
    }
  }
  return result;
}

std::string findExecutable(const std::string &name) {
  std::error_code error;
  const auto executablePath = fs::read_symlink("/proc/self/exe", error);
  if (!error) {
    const auto sibling = executablePath.parent_path() / name;
    struct stat status {};
    if (::stat(sibling.c_str(), &status) == 0 && S_ISREG(status.st_mode) &&
        ::access(sibling.c_str(), X_OK) == 0)
      return sibling.string();
  }
  const char *pathValue = std::getenv("PATH");
  const std::string_view path = pathValue && *pathValue
                                    ? pathValue
                                    : "/usr/local/bin:/usr/bin:/bin";
  std::size_t start = 0;
  while (start <= path.size()) {
    const auto separator = path.find(':', start);
    const auto directory = path.substr(
        start, separator == std::string_view::npos ? path.size() - start
                                                   : separator - start);
    if (!directory.empty()) {
      const auto candidate = fs::path(directory) / name;
      struct stat status {};
      if (::stat(candidate.c_str(), &status) == 0 && S_ISREG(status.st_mode) &&
          ::access(candidate.c_str(), X_OK) == 0)
        return candidate.string();
    }
    if (separator == std::string_view::npos)
      break;
    start = separator + 1;
  }
  return {};
}

std::string base64(std::string_view value) {
  gchar *encoded = g_base64_encode(
      reinterpret_cast<const guchar *>(value.data()), value.size());
  std::string result(encoded ? encoded : "");
  g_free(encoded);
  return result;
}

std::optional<std::string> decodeBase64(std::string_view value) {
  gsize length = 0;
  guchar *decoded = g_base64_decode(std::string(value).c_str(), &length);
  if (!decoded)
    return std::nullopt;
  std::string result(reinterpret_cast<char *>(decoded), length);
  g_free(decoded);
  return result;
}

std::string fileNameToUtf8(const std::string &value) {
  GError *error = nullptr;
  gsize written = 0;
  gchar *converted = g_filename_to_utf8(value.data(), value.size(), nullptr,
                                          &written, &error);
  if (converted) {
    std::string result(converted, written);
    g_free(converted);
    return result;
  }
  if (error)
    g_error_free(error);
  gchar *valid = g_utf8_make_valid(value.data(), value.size());
  std::string result(valid ? valid : "");
  g_free(valid);
  return result;
}

std::string fileNameFromUtf8(std::string_view value) {
  GError *error = nullptr;
  gsize written = 0;
  gchar *converted = g_filename_from_utf8(value.data(), value.size(), nullptr,
                                           &written, &error);
  if (!converted) {
    if (error)
      g_error_free(error);
    return std::string(value);
  }
  std::string result(converted, written);
  g_free(converted);
  return result;
}

Json loadTranslationDictionary(const std::string &locale) {
  std::vector<fs::path> roots{LUDASH_TRANSLATIONS_SOURCE_DIR,
                              LUDASH_TRANSLATIONS_INSTALL_DIR};
  Json messages = Json::object();
  for (const auto &root : roots) {
    std::error_code error;
    if (!fs::is_directory(root, error) || error)
      continue;
    std::vector<fs::path> catalogs;
    const auto rootCatalog = root / (locale + ".json");
    if (fs::is_regular_file(rootCatalog, error) && !error)
      catalogs.push_back(rootCatalog);
    error.clear();
    const auto features = root / locale;
    if (fs::is_directory(features, error) && !error) {
      for (fs::directory_iterator item(features, error), end;
           !error && item != end; item.increment(error))
        if (item->path().extension() == ".json")
          catalogs.push_back(item->path());
    }
    std::sort(catalogs.begin(), catalogs.end());
    for (const auto &catalog : catalogs) {
      std::ifstream file(catalog);
      if (!file)
        continue;
      Json document = Json::parse(file, nullptr, false);
      if (!document.is_object())
        continue;
      for (auto entry = document.begin(); entry != document.end(); ++entry)
        if (entry.value().is_string() && !messages.contains(entry.key()))
          messages.emplace(entry.key(), entry.value());
    }
  }
  return messages;
}

std::string canonicalLocale(std::string locale) {
  const auto encoding = locale.find_first_of(".@");
  if (encoding != std::string::npos)
    locale.resize(encoding);
  std::replace(locale.begin(), locale.end(), '-', '_');
  std::transform(locale.begin(), locale.end(), locale.begin(),
                 [](unsigned char value) {
                   return static_cast<char>(std::tolower(value));
                 });
  if (locale.starts_with("zh_tw") || locale.starts_with("zh_hant"))
    return "zh_TW";
  if (locale.starts_with("zh_cn") || locale.starts_with("zh_hans"))
    return "zh_CN";
  if (locale.starts_with("ja"))
    return "ja_JP";
  return "en_US";
}

std::string translated(const Json &messages, std::string_view key) {
  const auto found = messages.find(std::string(key));
  return found != messages.end() && found->is_string()
             ? found->get<std::string>()
             : std::string(key);
}

std::string substitute(std::string value, std::string_view token,
                       std::string_view replacement) {
  if (const auto position = value.find(token); position != std::string::npos)
    value.replace(position, token.size(), replacement);
  return value;
}

slint::SharedString slintText(std::string value) {
  return slint::SharedString(value);
}

slint::Color paletteColor(const Json &palette, std::string_view key,
                          std::uint32_t fallback) {
  const auto found = palette.find(std::string(key));
  if (found != palette.end() && found->is_string()) {
    const auto value = found->get<std::string>();
    if (value.size() == 7 && value.front() == '#') {
      try {
        return slint::Color::from_argb_encoded(
            0xff000000u | static_cast<std::uint32_t>(
                              std::stoul(value.substr(1), nullptr, 16)));
      } catch (const std::exception &) {
      }
    }
  }
  return slint::Color::from_argb_encoded(fallback);
}

std::string lowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return value;
}

bool readableDirectory(const fs::path &path) {
  std::error_code error;
  return fs::is_directory(path, error) && !error &&
         ::access(path.c_str(), R_OK | X_OK) == 0;
}

std::optional<fs::path> decodeFileUrl(std::string_view value) {
  std::string_view path = value;
  if (path.size() < 7 || lowerAscii(std::string(path.substr(0, 7))) !=
                              "file://")
    return std::nullopt;
  path.remove_prefix(7);
  if (!path.starts_with('/')) {
    const auto slash = path.find('/');
    if (slash == std::string_view::npos || path.substr(0, slash) != "localhost")
      return std::nullopt;
    path.remove_prefix(slash);
  }
  std::string decoded;
  decoded.reserve(path.size());
  for (std::size_t i = 0; i < path.size(); ++i) {
    if (path[i] != '%') {
      decoded.push_back(path[i]);
      continue;
    }
    if (i + 2 >= path.size())
      return std::nullopt;
    const auto digit = [](char value) -> int {
      if (value >= '0' && value <= '9')
        return value - '0';
      if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
      if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
      return -1;
    };
    const int high = digit(path[i + 1]);
    const int low = digit(path[i + 2]);
    if (high < 0 || low < 0 || (high == 0 && low == 0))
      return std::nullopt;
    decoded.push_back(static_cast<char>((high << 4) | low));
    i += 2;
  }
  return fs::path(std::move(decoded));
}

struct Choice {
  std::string id;
  std::string label;
  std::vector<std::pair<std::string, std::string>> options;
  std::size_t selected = 0;
};

struct Entry {
  fs::path path;
  std::string name;
  std::string detail;
  std::string icon;
  bool directory = false;
};

class PickerState final {
public:
  PickerState(slint::ComponentHandle<FilePickerWindow> &window,
              const Json &request, Json translations,
              Json palette, const fs::path &temporaryPath)
      : window_(window), request_(request), translations_(std::move(translations)),
        temporaryPath_(temporaryPath) {
    mode_ = request_.value("mode", "open");
    multiple_ = mode_ != "save" && request_.value("multiple", false);
    locale_ = request_.value("locale", "en_US");
    searchText_ = "";
    showHidden_ = false;
    gridView_ = false;
    const auto language = request_.value("language", "");
    if (!language.empty())
      translations_ = loadTranslationDictionary(canonicalLocale(language));
    else if (!translations_.is_object() || translations_.empty())
      translations_ = loadTranslationDictionary(canonicalLocale(locale_));

    for (const auto &filter : request_.value("filters", Json::array())) {
      std::vector<std::string> patterns;
      std::vector<std::string> mimeTypes;
      for (const auto &pattern : filter.value("patterns", Json::array()))
        if (pattern.is_string())
          patterns.push_back(pattern.get<std::string>());
      for (const auto &mime : filter.value("mime-types", Json::array()))
        if (mime.is_string())
          mimeTypes.push_back(mime.get<std::string>());
      filters_.push_back(
          {filter.value("label", std::string{}), std::move(patterns),
           std::move(mimeTypes)});
    }
    filterIndex_ = filters_.empty()
                       ? -1
                       : std::clamp(request_.value("filter-index", 0), 0,
                                    static_cast<int>(filters_.size()) - 1);

    for (const auto &choice : request_.value("choices", Json::array())) {
      Choice item;
      item.id = choice.value("id", ""s);
      item.label = choice.value("label", ""s);
      for (const auto &option : choice.value("values", Json::array()))
        item.options.emplace_back(option.value("id", ""s),
                                  option.value("label", ""s));
      if (item.options.empty())
        item.options = {{"false", translated(translations_, "No")},
                        {"true", translated(translations_, "Yes")}};
      const auto selected = choice.value("selected", ""s);
      const auto found = std::find_if(
          item.options.begin(), item.options.end(),
          [&selected](const auto &option) { return option.first == selected; });
      if (found != item.options.end())
        item.selected = static_cast<std::size_t>(found - item.options.begin());
      choices_.push_back(std::move(item));
    }

    palette_ = std::move(palette);
    window_->set_background_color(
        paletteColor(palette_, "background", 0xff171922));
    window_->set_surface_color(paletteColor(palette_, "surface", 0xff20232e));
    window_->set_elevated_color(
        paletteColor(palette_, "surfaceElevated", 0xff292d39));
    window_->set_text_color(paletteColor(palette_, "text", 0xfff1f1f5));
    window_->set_muted_color(paletteColor(palette_, "muted", 0xffa5a8b5));
    window_->set_border_color(paletteColor(palette_, "border", 0xff4c5060));
    window_->set_accent_color(paletteColor(palette_, "accent", 0xff9ccbfb));
    window_->set_accent_ink_color(
        paletteColor(palette_, "accentInk", 0xff171922));

    configureStrings();
    configurePlaces();
    configureFilters();
    configureChoices();
    initializeFolder();
  }

  void connect() {
    window_->on_location_accepted(
        [this](const slint::SharedString &value) { locationAccepted(text(value)); });
    window_->on_manual_path_accepted(
        [this](const slint::SharedString &value) { manualPathAccepted(text(value)); });
    window_->on_manual_path_changed([this](const slint::SharedString &) {
      selectedPaths_.clear();
      selectedIndex_ = -1;
      window_->set_selected_index(-1);
      updateItemsModel();
      updateStatus();
    });
    window_->on_search_changed([this](const slint::SharedString &value) {
      searchText_ = text(value);
      refreshEntries();
    });
    window_->on_select_item([this](int index) { selectItem(index); });
    window_->on_activate_item([this](int index) { activateItem(index); });
    window_->on_select_place([this](const slint::SharedString &value) {
      navigate(fs::path(fileNameFromUtf8(text(value))));
    });
    window_->on_navigate_back([this] { navigateHistory(-1); });
    window_->on_navigate_forward([this] { navigateHistory(1); });
    window_->on_navigate_up([this] {
      if (folder_.has_parent_path())
        navigate(folder_.parent_path());
    });
    window_->on_toggle_hidden([this] {
      showHidden_ = !showHidden_;
      window_->set_show_hidden(showHidden_);
      refreshEntries();
    });
    window_->on_toggle_view([this] {
      gridView_ = !gridView_;
      window_->set_grid_view(gridView_);
    });
    window_->on_select_filter([this](int index) {
      if (index >= 0 && static_cast<std::size_t>(index) < filters_.size()) {
        filterIndex_ = index;
        window_->set_filter_index(index);
        refreshEntries();
      }
    });
    window_->on_select_choice([this](int index) {
      if (index < 0 || static_cast<std::size_t>(index) >= choices_.size())
        return;
      auto &choice = choices_[static_cast<std::size_t>(index)];
      if (!choice.options.empty())
        choice.selected = (choice.selected + 1) % choice.options.size();
      configureChoices();
    });
    window_->on_maximize_requested([this] {
      auto &handle = window_->window();
      handle.set_maximized(!handle.is_maximized());
    });
    window_->on_confirm_requested([this] { confirm(); });
    window_->on_cancel_requested([this] { quit(); });
    window_->on_overwrite_response([this](bool replace) {
      window_->set_overwrite_pending(false);
      if (replace)
        finishAccepted(pendingPaths_);
    });
    window_->window().on_close_requested([this] {
      quit();
      return slint::CloseRequestResponse::HideWindow;
    });
    window_->fn_focus_manual_path();
    refreshEntries();
  }

  Json result() const { return result_; }

private:
  struct Filter {
    std::string label;
    std::vector<std::string> patterns;
    std::vector<std::string> mimeTypes;
  };

  static std::string text(const slint::SharedString &value) {
    return std::string(value.data(), value.size());
  }

  std::string tr(std::string_view key) const {
    return translated(translations_, key);
  }

  void configureStrings() {
    const auto title = request_.value("title", tr("Open file"));
    window_->set_window_title(slintText(title));
    window_->set_caption(slintText(tr("Browse, preview and choose your files")));
    window_->set_location_placeholder(
        slintText(tr("Paste an absolute path or file:// URL")));
    window_->set_search_placeholder(slintText(tr("Search this folder")));
    window_->set_location_label(slintText(tr("Location")));
    window_->set_search_label(slintText(tr("Search")));
    window_->set_show_hidden_label(slintText(tr("Show hidden files")));
    window_->set_grid_label(slintText(tr("Grid view")));
    window_->set_list_label(slintText(tr("List view")));
    window_->set_preview_label(slintText(tr("Preview")));
    window_->set_manual_path_label(slintText(
        tr(mode_ == "directory" ? "Folder" : "File name")));
    window_->set_footer_hint(
        slintText(tr("Ctrl+L · Location    Ctrl+F · Search")));
    window_->set_maximize_label(slintText(tr("Maximize or restore")));
    window_->set_preview_hint(slintText(
        tr("Double-click a folder to open it. Select a file, then confirm below.")));
    window_->set_empty_label(slintText(tr("No matching files in this folder.")));
    window_->set_cancel_label(slintText(tr("Cancel")));
    std::string acceptLabel = request_.value("accept-label", ""s);
    if (acceptLabel.empty())
      acceptLabel = tr(mode_ == "save" ? "Save"
                       : mode_ == "directory" ? "Choose folder"
                                               : "Open file");
    window_->set_accept_label(slintText(acceptLabel));
    window_->set_back_label(slintText(tr("Back")));
    window_->set_forward_label(slintText(tr("Forward")));
    window_->set_up_label(slintText(tr("Up")));
    window_->set_go_label(slintText(tr("Go")));
    window_->set_replace_heading(slintText(tr("Replace file?")));
    window_->set_replace_description(
        slintText(tr("A file with this name already exists. Replace it?")));
    window_->set_replace_label(slintText(tr("Replace")));
    window_->set_keep_label(slintText(tr("Keep existing")));
    window_->set_status_label(slintText(""));
    window_->set_error_label(slintText(""));
  }

  void configurePlaces() {
    const char *homeValue = std::getenv("HOME");
    home_ = homeValue && *homeValue ? fs::path(homeValue) : fs::path("/");
    std::vector<FilePickerPlace> places;
    auto appendPlace = [&](const fs::path &path, std::string_view label,
                           std::string_view icon) {
      if (!readableDirectory(path))
        return;
      if (std::any_of(places.begin(), places.end(), [&](const auto &place) {
            return text(place.path) == path.string();
          }))
        return;
      FilePickerPlace place;
      place.label = slintText(tr(label));
      place.path = slintText(fileNameToUtf8(path.string()));
      place.icon = slintText(std::string(icon));
      places.push_back(std::move(place));
    };
    appendPlace(home_, "Home", "home");
    for (const auto &[environment, label] :
         std::array<std::pair<const char *, std::string_view>, 5>{
             {{"XDG_DESKTOP_DIR", "Desktop"},
              {"XDG_DOWNLOAD_DIR", "Downloads"},
              {"XDG_DOCUMENTS_DIR", "Documents"},
              {"XDG_PICTURES_DIR", "Pictures"},
              {"XDG_MUSIC_DIR", "Music"}}}) {
      // Icon names from data/icons (see src/desktop/theme/Icon.slint).
      const std::string_view icon = label == "Desktop"     ? "monitor"
                                    : label == "Downloads" ? "download"
                                    : label == "Documents" ? "document"
                                    : label == "Pictures"  ? "image"
                                                           : "music";
      if (const char *path = std::getenv(environment); path && *path)
        appendPlace(fs::path(path), label, icon);
      else
        appendPlace(home_ / std::string(label), label, icon);
    }
    appendPlace(home_ / "Videos", "Videos", "video");
    appendPlace("/", "File system", "disk");
    const auto mediaRoot = fs::path("/media");
    std::error_code error;
    for (fs::directory_iterator entry(mediaRoot, error), end;
         !error && entry != end && places.size() < 18; entry.increment(error))
      appendPlace(entry->path(), fileNameToUtf8(entry->path().filename().string()),
                  "usb");
    error.clear();
    for (const auto &root : {fs::path("/mnt"),
                             fs::path("/run/media") / fileNameToUtf8(
                                                          home_.filename().string())}) {
      for (fs::directory_iterator entry(root, error), end;
           !error && entry != end && places.size() < 18; entry.increment(error))
        appendPlace(entry->path(),
                    fileNameToUtf8(entry->path().filename().string()), "usb");
      error.clear();
    }
    window_->set_places(
        std::make_shared<slint::VectorModel<FilePickerPlace>>(std::move(places)));
  }

  void configureFilters() {
    std::vector<FilePickerFilter> model;
    model.reserve(filters_.size());
    for (const auto &filter : filters_) {
      FilePickerFilter row;
      row.label = slintText(filter.label);
      model.push_back(std::move(row));
    }
    window_->set_filters(
        std::make_shared<slint::VectorModel<FilePickerFilter>>(std::move(model)));
    if (!filters_.empty())
      window_->set_filter_index(filterIndex_);
  }

  void configureChoices() {
    std::vector<FilePickerChoice> model;
    model.reserve(choices_.size());
    for (const auto &choice : choices_) {
      FilePickerChoice row;
      row.id = slintText(choice.id);
      row.label = slintText(choice.label);
      row.selected_label = choice.options.empty()
                               ? slintText("")
                               : slintText(choice.options[choice.selected].second);
      model.push_back(std::move(row));
    }
    window_->set_choices(
        std::make_shared<slint::VectorModel<FilePickerChoice>>(std::move(model)));
  }

  fs::path decodeRequestPath(std::string_view key) const {
    const auto value = request_.value(std::string(key), std::string{});
    if (value.empty())
      return {};
    const auto bytes = decodeBase64(value);
    return bytes ? fs::path(*bytes) : fs::path{};
  }

  void initializeFolder() {
    auto initial = decodeRequestPath("current-folder");
    const auto currentFile = decodeRequestPath("current-file");
    if (initial.empty() && !currentFile.empty())
      initial = currentFile.parent_path();
    if (!readableDirectory(initial))
      initial = home_;
    navigate(initial, false);
    if (mode_ == "save")
      window_->set_manual_path(slintText(
          request_.value("current-name", currentFile.filename().string())));
  }

  std::optional<fs::path> resolvePath(std::string_view value) const {
    if (value.empty() || value.find('\0') != std::string_view::npos)
      return std::nullopt;
    fs::path path;
    if (value.starts_with("file:")) {
      auto url = decodeFileUrl(value);
      if (!url)
        return std::nullopt;
      path = *url;
    } else if (value.find("://") != std::string_view::npos) {
      return std::nullopt;
    } else {
      std::string local = fileNameFromUtf8(value);
      if (local == "~")
        local = home_.string();
      else if (local.starts_with("~/"))
        local = home_.string() + local.substr(1);
      path = fs::path(std::move(local));
    }
    if (path.is_relative())
      path = folder_ / path;
    return path.lexically_normal();
  }

  void initializeHistory(const fs::path &path) {
    if (historyIndex_ >= 0 && history_[static_cast<std::size_t>(historyIndex_)] == path)
      return;
    history_.resize(static_cast<std::size_t>(historyIndex_ + 1));
    history_.push_back(path);
    historyIndex_ = static_cast<int>(history_.size()) - 1;
  }

  void navigate(const fs::path &path, bool remember = true) {
    if (!readableDirectory(path)) {
      setError(tr("This folder does not exist or cannot be read."));
      return;
    }
    folder_ = fs::absolute(path).lexically_normal();
    if (remember)
      initializeHistory(folder_);
    selectedPaths_.clear();
    selectedIndex_ = -1;
    searchText_.clear();
    window_->set_location(slintText(fileNameToUtf8(folder_.string())));
    window_->set_search_text(slintText(""));
    if (mode_ != "save")
      window_->set_manual_path(slintText(""));
    window_->set_selected_index(-1);
    setError("");
    window_->set_preview_name(slintText(fileNameToUtf8(folder_.filename().string())));
    window_->set_preview_detail(slintText(tr("Folder")));
    window_->set_preview_icon(slintText("folder"));
    window_->set_preview_image(slint::Image{});
    refreshEntries();
  }

  void navigateHistory(int direction) {
    const int next = historyIndex_ + direction;
    if (next < 0 || next >= static_cast<int>(history_.size()))
      return;
    historyIndex_ = next;
    navigate(history_[static_cast<std::size_t>(historyIndex_)], false);
  }

  bool matchesFilter(const fs::path &path, bool directory) const {
    if (directory)
      return true;
    if (filters_.empty() || filterIndex_ < 0 ||
        static_cast<std::size_t>(filterIndex_) >= filters_.size())
      return true;
    const auto &filter = filters_[static_cast<std::size_t>(filterIndex_)];
    if (filter.patterns.empty() && filter.mimeTypes.empty())
      return true;
    const auto name = fileNameToUtf8(path.filename().string());
    if (std::any_of(filter.patterns.begin(), filter.patterns.end(), [&](const auto &pattern) {
      return ::fnmatch(pattern.c_str(), name.c_str(), FNM_CASEFOLD) == 0;
    }))
      return true;
    if (filter.mimeTypes.empty())
      return false;
    gboolean uncertain = FALSE;
    gchar *contentType =
        g_content_type_guess(name.c_str(), nullptr, 0, &uncertain);
    (void)uncertain;
    if (!contentType)
      return false;
    gchar *mime = g_content_type_get_mime_type(contentType);
    const std::string_view detected = mime ? mime : "";
    const bool matches = std::any_of(
        filter.mimeTypes.begin(), filter.mimeTypes.end(), [&](const auto &type) {
          if (type.ends_with("/*"))
            return detected.starts_with(
                std::string_view(type).substr(0, type.size() - 1));
          return detected == type;
        });
    g_free(mime);
    g_free(contentType);
    return matches;
  }

  std::string fileDetails(const fs::directory_entry &entry, bool directory) const {
    if (directory)
      return tr("Folder");
    std::error_code error;
    const auto size = entry.file_size(error);
    std::string detail;
    if (!error) {
      std::ostringstream formatted;
      if (size >= 1024 * 1024)
        formatted << std::fixed << std::setprecision(1)
                  << static_cast<double>(size) / (1024.0 * 1024.0) << " MiB";
      else if (size >= 1024)
        formatted << std::fixed << std::setprecision(1)
                  << static_cast<double>(size) / 1024.0 << " KiB";
      else
        formatted << size << " B";
      detail = formatted.str();
    }
    const auto modified = entry.last_write_time(error);
    if (!error) {
      const auto systemTime = std::chrono::file_clock::to_sys(modified);
      const auto time = std::chrono::system_clock::to_time_t(systemTime);
      std::tm local{};
      if (::localtime_r(&time, &local)) {
        std::ostringstream formatted;
        formatted << std::put_time(&local, "%Y-%m-%d %H:%M");
        if (!detail.empty())
          detail += " · ";
        detail += formatted.str();
      }
    }
    return detail;
  }

  void refreshEntries() {
    entries_.clear();
    std::error_code error;
    for (fs::directory_iterator item(
             folder_, fs::directory_options::skip_permission_denied, error),
         end;
         !error && item != end && entries_.size() < 5000; item.increment(error)) {
      const auto name = item->path().filename().string();
      if (!showHidden_ && !name.empty() && name.front() == '.')
        continue;
      const bool directory = item->is_directory(error);
      if (error) {
        error.clear();
        continue;
      }
      if (mode_ == "directory" && !directory)
        continue;
      if (!matchesFilter(item->path(), directory))
        continue;
      const auto displayName = fileNameToUtf8(name);
      if (!searchText_.empty() &&
          lowerAscii(displayName).find(lowerAscii(searchText_)) == std::string::npos)
        continue;
      entries_.push_back({item->path(), displayName,
                          fileDetails(*item, directory),
                          directory ? "folder" : "file", directory});
    }
    std::sort(entries_.begin(), entries_.end(), [](const Entry &left,
                                                   const Entry &right) {
      if (left.directory != right.directory)
        return left.directory;
      return lowerAscii(left.name) < lowerAscii(right.name);
    });
    updateItemsModel();
    updateStatus();
  }

  void updateItemsModel() {
    std::vector<FilePickerItem> rows;
    rows.reserve(entries_.size());
    for (std::size_t index = 0; index < entries_.size(); ++index) {
      FilePickerItem item;
      item.name = slintText(entries_[index].name);
      item.detail = slintText(entries_[index].detail);
      item.icon = slintText(entries_[index].icon);
      item.is_directory = entries_[index].directory;
      item.selected = std::find(selectedPaths_.begin(), selectedPaths_.end(),
                                entries_[index].path) != selectedPaths_.end();
      rows.push_back(std::move(item));
    }
    window_->set_items(
        std::make_shared<slint::VectorModel<FilePickerItem>>(std::move(rows)));
  }

  void updateStatus() {
    const auto selectedCount = selectedPaths_.size();
    auto value = substitute(tr("%1 items · %2 selected"), "%1",
                            std::to_string(entries_.size()));
    value = substitute(std::move(value), "%2", std::to_string(selectedCount));
    window_->set_status_label(slintText(value));
  }

  void setError(std::string value) {
    window_->set_error_label(slintText(value));
  }

  void updatePreview(const fs::path &path) {
    std::error_code error;
    const bool directory = fs::is_directory(path, error) && !error;
    window_->set_preview_name(
        slintText(fileNameToUtf8(path.filename().empty() ? path.string()
                                                         : path.filename().string())));
    window_->set_preview_icon(slintText(directory ? "folder" : "file"));
    if (directory) {
      window_->set_preview_detail(slintText(tr("Folder")));
      window_->set_preview_image(slint::Image{});
      return;
    }
    const auto fileSize = fs::file_size(path, error);
    if (error) {
      window_->set_preview_detail(slintText(""));
      window_->set_preview_image(slint::Image{});
      return;
    }
    window_->set_preview_detail(slintText(fileDetailsForPath(path, fileSize)));
    if (fileSize > 64 * 1024 * 1024) {
      window_->set_preview_image(slint::Image{});
      return;
    }
    int imageWidth = 0;
    int imageHeight = 0;
    if (!gdk_pixbuf_get_file_info(path.c_str(), &imageWidth, &imageHeight) ||
        imageWidth <= 0 || imageHeight <= 0 ||
        static_cast<std::uint64_t>(imageWidth) *
                static_cast<std::uint64_t>(imageHeight) >
            32u * 1024u * 1024u) {
      window_->set_preview_image(slint::Image{});
      return;
    }
    const auto target = temporaryPath_ / "preview.png";
    GError *decodeError = nullptr;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(
        path.c_str(), 360, 280, TRUE, &decodeError);
    if (!pixbuf) {
      if (decodeError)
        g_error_free(decodeError);
      window_->set_preview_image(slint::Image{});
      return;
    }
    GError *saveError = nullptr;
    const gboolean saved =
        gdk_pixbuf_save(pixbuf, target.c_str(), "png", &saveError, nullptr);
    g_object_unref(pixbuf);
    if (saveError)
      g_error_free(saveError);
    if (saved)
      window_->set_preview_image(
          slint::Image::load_from_path(slintText(target.string())));
    else
      window_->set_preview_image(slint::Image{});
  }

  std::string fileDetailsForPath(const fs::path &path, std::uintmax_t size) const {
    const auto found = std::find_if(entries_.begin(), entries_.end(),
                                    [&path](const Entry &entry) {
                                      return entry.path == path;
                                    });
    if (found != entries_.end())
      return found->detail;
    std::ostringstream formatted;
    if (size >= 1024 * 1024)
      formatted << std::fixed << std::setprecision(1)
                << static_cast<double>(size) / (1024.0 * 1024.0) << " MiB";
    else if (size >= 1024)
      formatted << std::fixed << std::setprecision(1)
                << static_cast<double>(size) / 1024.0 << " KiB";
    else
      formatted << size << " B";
    return formatted.str();
  }

  void selectItem(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= entries_.size())
      return;
    const auto &entry = entries_[static_cast<std::size_t>(index)];
    selectedIndex_ = index;
    window_->set_selected_index(index);
    const auto found = std::find(selectedPaths_.begin(), selectedPaths_.end(),
                                 entry.path);
    if (multiple_) {
      if (found == selectedPaths_.end())
        selectedPaths_.push_back(entry.path);
      else
        selectedPaths_.erase(found);
    } else {
      selectedPaths_ = {entry.path};
    }
    if (!multiple_ && !entry.directory)
      window_->set_manual_path(slintText(entry.name));
    updateItemsModel();
    updatePreview(entry.path);
    setError("");
    updateStatus();
  }

  void activateItem(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= entries_.size())
      return;
    const auto entry = entries_[static_cast<std::size_t>(index)];
    if (entry.directory) {
      if (mode_ == "directory") {
        selectedPaths_ = {entry.path};
        selectedIndex_ = index;
        updateItemsModel();
        updateStatus();
      } else {
        navigate(entry.path);
      }
      return;
    }
    selectItem(index);
    if (mode_ == "open")
      confirm();
  }

  void locationAccepted(std::string value) {
    const auto path = resolvePath(value);
    if (!path) {
      setError(tr("Enter a local file or folder path."));
      return;
    }
    std::error_code error;
    if (fs::is_directory(*path, error) && !error) {
      navigate(*path);
      return;
    }
    if (mode_ == "directory") {
      setError(tr("Choose a folder, not a file."));
      return;
    }
    if ((fs::is_regular_file(*path, error) && !error) || mode_ == "save") {
      navigate(path->parent_path());
      window_->set_manual_path(slintText(fileNameToUtf8(path->filename().string())));
      updatePreview(*path);
      return;
    }
    setError(tr("This file does not exist or cannot be read."));
  }

  void manualPathAccepted(std::string value) {
    if (mode_ == "directory" && value.empty()) {
      confirm();
      return;
    }
    const auto path = resolvePath(value);
    if (!path) {
      setError(tr("Enter a local file or folder path."));
      return;
    }
    std::error_code error;
    if (mode_ != "directory" && fs::is_directory(*path, error) && !error) {
      navigate(*path);
      return;
    }
    confirm(*path);
  }

  std::vector<fs::path> candidatePaths(const std::optional<fs::path> &overridePath = {}) {
    if (overridePath)
      return {*overridePath};
    if (!selectedPaths_.empty() && mode_ != "save")
      return selectedPaths_;
    const auto manual = text(window_->get_manual_path());
    if (!manual.empty()) {
      const auto path = resolvePath(manual);
      return path ? std::vector<fs::path>{*path} : std::vector<fs::path>{};
    }
    if (mode_ == "directory")
      return {folder_};
    return {};
  }

  void confirm(std::optional<fs::path> overridePath = {}) {
    const auto paths = candidatePaths(overridePath);
    if (paths.empty()) {
      setError(tr("Select a file or enter its path."));
      return;
    }
    if (paths.size() == 1 && mode_ != "directory") {
      std::error_code error;
      if (fs::is_directory(paths.front(), error) && !error) {
        navigate(paths.front());
        return;
      }
    }
    for (const auto &path : paths) {
      std::error_code error;
      if (mode_ == "directory") {
        if (!readableDirectory(path)) {
          setError(tr("This folder does not exist or cannot be read."));
          return;
        }
      } else if (mode_ == "open") {
        if (!fs::is_regular_file(path, error) || error ||
            ::access(path.c_str(), R_OK) != 0) {
          setError(tr("This file does not exist or cannot be read."));
          return;
        }
      } else {
        const auto parent = path.parent_path().empty() ? folder_ : path.parent_path();
        if (!readableDirectory(parent) || ::access(parent.c_str(), W_OK) != 0) {
          setError(tr("This location cannot be written to."));
          return;
        }
        if (fs::exists(path, error) && !error) {
          if (!fs::is_regular_file(path, error) || error ||
              ::access(path.c_str(), W_OK) != 0) {
            setError(tr("This location cannot be written to."));
            return;
          }
          pendingPaths_ = paths;
          window_->set_overwrite_pending(true);
          return;
        }
      }
    }
    finishAccepted(paths);
  }

  void finishAccepted(const std::vector<fs::path> &paths) {
    Json output;
    output["accepted"] = true;
    output["paths"] = Json::array();
    for (const auto &path : paths)
      output["paths"].push_back(base64(path.string()));
    output["filter-index"] = filterIndex_;
    output["choices"] = Json::array();
    for (const auto &choice : choices_)
      output["choices"].push_back(
          {{"id", choice.id},
           {"selected", choice.options.empty()
                            ? ""
                            : choice.options[choice.selected].first}});
    result_ = std::move(output);
    quit();
  }

  void quit() {
    if (!quitting_) {
      quitting_ = true;
      slint::quit_event_loop();
    }
  }

  slint::ComponentHandle<FilePickerWindow> &window_;
  const Json &request_;
  Json translations_;
  Json palette_ = Json::object();
  const fs::path &temporaryPath_;
  std::string mode_ = "open";
  std::string locale_ = "en_US";
  bool multiple_ = false;
  bool showHidden_ = false;
  bool gridView_ = false;
  bool quitting_ = false;
  fs::path home_;
  fs::path folder_;
  std::vector<fs::path> history_;
  int historyIndex_ = -1;
  std::string searchText_;
  std::vector<Entry> entries_;
  std::vector<fs::path> selectedPaths_;
  int selectedIndex_ = -1;
  std::vector<Filter> filters_;
  int filterIndex_ = 0;
  std::vector<Choice> choices_;
  std::vector<fs::path> pendingPaths_;
  Json result_ = Json{{"accepted", false}};
};

std::optional<Json> compositorState(wl_event_loop *eventLoop) {
  const char *control = std::getenv("LUNADASH_CONTROL");
  const char *legacyControl = std::getenv("LUDASH_CONTROL");
  if ((!control || !*control) && (!legacyControl || !*legacyControl))
    return std::nullopt;
  const auto executable = findExecutable("lunadashctl");
  if (executable.empty())
    return std::nullopt;
  const auto result = runProcess(eventLoop, executable, {"status"}, 1800);
  if (!result.success)
    return std::nullopt;
  Json state = Json::parse(result.output, nullptr, false);
  return state.is_object() ? std::optional<Json>(std::move(state))
                           : std::nullopt;
}

Json readRequest() {
  std::string line;
  if (!std::getline(std::cin, line) || line.size() > 1024 * 1024)
    return Json::object();
  Json request = Json::parse(line, nullptr, false);
  return request.is_object() ? request : Json::object();
}

} // namespace

int FilePickerApplication::run(int argc, char **argv) {
  (void)argc;
  (void)argv;
  const Json request = readRequest();
  if (request.empty())
    return 2;

  if (const char *hold = std::getenv("LUNADASH_FILE_PICKER_TEST_HOLD");
      hold && std::string_view(hold) == "1") {
    struct sigaction action {};
    action.sa_handler = requestTermination;
    ::sigemptyset(&action.sa_mask);
    ::sigaction(SIGTERM, &action, nullptr);
    while (!terminateRequested)
      ::pause();
    std::cout << R"({"accepted":false})" << '\n' << std::flush;
    return 0;
  }

  auto eventLoop = std::unique_ptr<wl_event_loop, decltype(&wl_event_loop_destroy)>(
      wl_event_loop_create(), wl_event_loop_destroy);
  if (!eventLoop) {
    std::cerr << "LunaDash FileChooser could not create its process loop.\n";
    return 2;
  }
  const auto state = compositorState(eventLoop.get()).value_or(Json::object());
  const auto translationLocale = request.value(
      "language", std::getenv("LUDASH_LANGUAGE")
                      ? std::getenv("LUDASH_LANGUAGE")
                      : (std::getenv("LANG") ? std::getenv("LANG") : "en_US"));
  Json translations = loadTranslationDictionary(canonicalLocale(translationLocale));
  if (state.contains("translations") && state["translations"].is_object())
    translations = state["translations"];
  Json palette = state.value("palette", Json::object());

  TemporaryDirectory temporary;
  if (temporary.path().empty()) {
    std::cerr << "LunaDash FileChooser could not create a preview directory.\n";
    return 2;
  }
  auto window = FilePickerWindow::create();
  PickerState picker(window, request, std::move(translations), std::move(palette),
                     temporary.path());
  picker.connect();
  struct sigaction action {};
  action.sa_handler = requestTermination;
  ::sigemptyset(&action.sa_mask);
  ::sigaction(SIGTERM, &action, nullptr);
  slint::Timer terminationTimer(std::chrono::milliseconds(50), [] {
    if (terminateRequested)
      slint::quit_event_loop();
  });
  if (terminateRequested)
    slint::quit_event_loop();
  try {
    window->run();
  } catch (const std::exception &error) {
    std::cerr << "LunaDash FileChooser could not start: " << error.what() << '\n';
    return 2;
  }
  const auto result = picker.result();
  std::cout << result.dump() << '\n' << std::flush;
  return 0;
}

} // namespace LunaDash
