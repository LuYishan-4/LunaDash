#include "service/portal/screencast/ScreenCastChooserApplication.hpp"

#include "core/process/ProcessRunner.hpp"
#include "ScreenCastChooser.h"

#include <nlohmann/json.hpp>
#include <slint.h>
#include <wayland-server-core.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace LunaDash {
namespace {

using Json = nlohmann::json;
using namespace std::string_literals;

struct Source {
  std::string raw;
  std::string label;
  std::filesystem::path preview;
};

class TemporaryDirectory final {
public:
  TemporaryDirectory() {
    auto pattern = (std::filesystem::temp_directory_path() /
                    "lunadash-screencast-XXXXXX")
                       .string();
    std::vector<char> writable(pattern.begin(), pattern.end());
    writable.push_back('\0');
    if (char *created = ::mkdtemp(writable.data()))
      path_ = created;
  }

  ~TemporaryDirectory() {
    if (!path_.empty()) {
      std::error_code error;
      std::filesystem::remove_all(path_, error);
    }
  }

  TemporaryDirectory(const TemporaryDirectory &) = delete;
  TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

  const std::filesystem::path &path() const { return path_; }

private:
  std::filesystem::path path_;
};

struct ProcessResult {
  bool started = false;
  bool success = false;
  std::string output;
};

ProcessResult runProcess(wl_event_loop *eventLoop, const std::string &program,
                         const std::vector<std::string> &arguments,
                         int timeoutMilliseconds, bool collectOutput = true) {
  ProcessResult result;
  ProcessRunner runner(eventLoop, collectOutput);
  bool completed = false;
  result.started = runner.run(
      program, arguments,
      [&result, &completed](bool success, const std::string &output) {
        result.success = success;
        result.output = output;
        completed = true;
      },
      timeoutMilliseconds);
  if (!result.started)
    return result;

  while (!completed) {
    if (wl_event_loop_dispatch(eventLoop, 20) < 0) {
      runner.cancel();
      return result;
    }
  }
  return result;
}

std::string findExecutable(const std::string &name) {
  std::error_code error;
  const auto executablePath = std::filesystem::read_symlink("/proc/self/exe", error);
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
      const auto candidate = std::filesystem::path(directory) / name;
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

std::vector<std::string> readSources() {
  std::vector<std::string> sources;
  for (std::string line; std::getline(std::cin, line);)
    if (!line.empty())
      sources.push_back(std::move(line));
  return sources;
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

Json loadTranslationDictionary(const std::string &locale) {
  std::vector<std::filesystem::path> roots{
      LUDASH_TRANSLATIONS_SOURCE_DIR, LUDASH_TRANSLATIONS_INSTALL_DIR};
  for (const auto &root : roots) {
    std::error_code error;
    if (!std::filesystem::is_directory(root, error) || error)
      continue;

    Json messages = Json::object();
    std::vector<std::filesystem::path> catalogs;
    const auto rootCatalog = root / (locale + ".json");
    if (std::filesystem::is_regular_file(rootCatalog, error) && !error)
      catalogs.push_back(rootCatalog);
    error.clear();
    const auto features = root / locale;
    if (std::filesystem::is_directory(features, error) && !error) {
      for (std::filesystem::directory_iterator item(features, error), end;
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
    return messages;
  }
  return Json::object();
}

std::string translated(const Json &messages, std::string_view key) {
  const auto found = messages.find(std::string(key));
  return found != messages.end() && found->is_string()
             ? found->get<std::string>()
             : std::string(key);
}

std::string substitute(std::string value, std::string_view replacement) {
  constexpr std::string_view token = "%1";
  const auto position = value.find(token);
  if (position != std::string::npos)
    value.replace(position, token.size(), replacement);
  return value;
}

std::string trimmed(std::string value) {
  const auto first = std::find_if_not(value.begin(), value.end(),
                                      [](unsigned char character) {
                                        return std::isspace(character);
                                      });
  const auto last = std::find_if_not(value.rbegin(), value.rend(),
                                     [](unsigned char character) {
                                       return std::isspace(character);
                                     })
                        .base();
  return first < last ? std::string(first, last) : std::string{};
}

std::string displayLabel(const std::string &raw, const Json &messages) {
  if (raw.starts_with("Monitor: "))
    return substitute(translated(messages, "Screen — %1"), raw.substr(9));
  if (raw.starts_with("Window: "))
    return substitute(translated(messages, "Window — %1"), raw.substr(8));
  return raw;
}

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

std::optional<Json> matchingWindow(const Json &state, std::string_view title) {
  const auto clients = state.find("clients");
  if (clients == state.end() || !clients->is_array())
    return std::nullopt;
  for (const auto &client : *clients) {
    if (!client.is_object())
      continue;
    const std::string candidate = client.value("title", ""s);
    if (candidate == title || (!candidate.empty() &&
                               (title.find(candidate) != std::string_view::npos ||
                                candidate.find(title) != std::string::npos)))
      return client;
  }
  return std::nullopt;
}

slint::Color color(const Json &palette, std::string_view key,
                   std::uint32_t fallback) {
  const auto found = palette.find(std::string(key));
  if (found != palette.end() && found->is_string()) {
    const auto value = found->get<std::string>();
    if (value.size() == 7 && value.front() == '#') {
      try {
        return slint::Color::from_argb_encoded(
            0xff000000u |
            static_cast<std::uint32_t>(std::stoul(value.substr(1), nullptr, 16)));
      } catch (const std::exception &) {
      }
    }
  }
  return slint::Color::from_argb_encoded(fallback);
}

std::vector<Source> prepareSources(
    wl_event_loop *eventLoop, const std::vector<std::string> &rawSources,
    const Json &state, const std::filesystem::path &temporaryPath) {
  std::vector<Source> sources;
  sources.reserve(rawSources.size());
  const auto messages = [&state] {
    const auto translations = state.find("translations");
    if (translations != state.end() && translations->is_object())
      return *translations;
    const auto language = state.value("language", std::string{});
    return loadTranslationDictionary(
        canonicalLocale(language.empty()
                            ? (std::getenv("LUDASH_LANGUAGE")
                                   ? std::getenv("LUDASH_LANGUAGE")
                                   : (std::getenv("LANG") ? std::getenv("LANG")
                                                          : "en_US"))
                            : language));
  }();

  std::optional<std::filesystem::path> desktopPreview;
  if (std::any_of(rawSources.begin(), rawSources.end(), [](const auto &source) {
        return source.starts_with("Monitor: ");
      })) {
    const auto path = temporaryPath / "desktop.png";
    const auto result = runProcess(eventLoop, findExecutable("grim"),
                                   {path.string()}, 1800, false);
    if (result.success)
      desktopPreview = path;
  }

  std::vector<std::pair<std::size_t, std::vector<std::string>>> captures;
  constexpr std::size_t kMaximumWindowPreviews = 12;
  for (std::size_t index = 0; index < rawSources.size(); ++index) {
    const auto &raw = rawSources[index];
    Source source{raw, displayLabel(raw, messages), {}};
    if (raw.starts_with("Monitor: ") && desktopPreview)
      source.preview = *desktopPreview;
    if (raw.starts_with("Window: ")) {
      const auto title = trimmed(raw.substr(8));
      if (const auto client = matchingWindow(state, title)) {
        const int x = client->value("x", 0);
        const int y = client->value("y", 0);
        const int width = client->value("width", 0);
        const int height = client->value("height", 0);
        if (width > 8 && height > 8 && !client->value("minimized", false) &&
            !client->value("hiddenByMaximize", false) &&
            client->value("visible", true) &&
            captures.size() < kMaximumWindowPreviews) {
          const auto path = temporaryPath / ("window-" +
                                              std::to_string(index) + ".png");
          source.preview = path;
          captures.push_back(
              {index, {"-g", std::to_string(x) + "," + std::to_string(y) +
                                " " + std::to_string(width) + "x" +
                                std::to_string(height),
                       path.string()}});
        }
      }
    }
    sources.push_back(std::move(source));
  }

  std::vector<std::unique_ptr<ProcessRunner>> runners;
  int pending = 0;
  for (const auto &[index, arguments] : captures) {
    auto runner = std::make_unique<ProcessRunner>(eventLoop, false);
    ++pending;
    const bool started = runner->run(
        findExecutable("grim"), arguments,
        [&pending, &sources, index](bool success, const std::string &) {
          if (!success)
            sources[index].preview.clear();
          --pending;
        },
        900);
    if (!started) {
      sources[index].preview.clear();
      --pending;
    } else {
      runners.push_back(std::move(runner));
    }
  }
  while (pending > 0)
    if (wl_event_loop_dispatch(eventLoop, 20) < 0) {
      for (auto &runner : runners)
        runner->cancel();
      for (const auto &[index, arguments] : captures) {
        (void)arguments;
        sources[index].preview.clear();
      }
      runners.clear();
      break;
    }
  return sources;
}

void applyTheme(slint::ComponentHandle<ScreenCastChooserWindow> &window,
                const Json &state) {
  const Json palette = state.value("palette", Json::object());
  window->set_background_color(color(palette, "background", 0xff171922));
  window->set_surface_color(color(palette, "surface", 0xff20232e));
  window->set_elevated_color(color(palette, "surfaceElevated", 0xff292d39));
  window->set_text_color(color(palette, "text", 0xfff1f1f5));
  window->set_muted_color(color(palette, "muted", 0xffa5a8b5));
  window->set_border_color(color(palette, "border", 0xff4c5060));
  window->set_accent_color(color(palette, "accent", 0xff9ccbfb));
  window->set_accent_ink_color(color(palette, "accentInk", 0xff171922));
}

} // namespace

int ScreenCastChooserApplication::run(int argc, char **argv) {
  (void)argc;
  (void)argv;
  const auto rawSources = readSources();
  if (rawSources.empty())
    return 1;

  if (const char *autopick = std::getenv("LUNADASH_SCREENCAST_CHOOSER_AUTOPICK");
      autopick && std::string_view(autopick) == "1") {
    std::cout << rawSources.front() << '\n' << std::flush;
    return 0;
  }

  wl_event_loop *eventLoop = wl_event_loop_create();
  if (!eventLoop) {
    std::cerr << "LunaDash ScreenCast chooser could not create its process loop.\n";
    return 1;
  }
  const auto destroyEventLoop = [](wl_event_loop *loop) {
    wl_event_loop_destroy(loop);
  };
  std::unique_ptr<wl_event_loop, decltype(destroyEventLoop)> loop(eventLoop,
                                                                  destroyEventLoop);
  const auto state = compositorState(eventLoop).value_or(Json::object());
  TemporaryDirectory temporary;
  std::vector<Source> sources;
  if (temporary.path().empty()) {
    for (const auto &raw : rawSources)
      sources.push_back(
          {raw, displayLabel(raw, state.value("translations", Json::object())), {}});
  } else {
    sources = prepareSources(eventLoop, rawSources, state, temporary.path());
  }

  auto window = ScreenCastChooserWindow::create();
  const auto translations = [&state] {
    const auto found = state.find("translations");
    if (found != state.end() && found->is_object())
      return *found;
    const char *language = std::getenv("LUDASH_LANGUAGE");
    if (!language || !*language)
      language = std::getenv("LANG");
    return loadTranslationDictionary(canonicalLocale(language ? language : "en_US"));
  }();
  window->set_window_title(
      slint::SharedString(translated(translations, "Share your screen")));
  window->set_heading(
      slint::SharedString(translated(translations, "Choose what to share")));
  window->set_description(slint::SharedString(translated(
      translations,
      "Select a screen or application window. Visible LunaDash windows use a live frame preview captured when this chooser opens; unavailable or minimized windows keep a labelled card.")));
  const bool hasPreview = std::any_of(
      sources.begin(), sources.end(), [](const Source &source) {
        return !source.preview.empty() &&
               std::filesystem::is_regular_file(source.preview);
      });
  window->set_preview_note(slint::SharedString(translated(
      translations, hasPreview
                       ? "Previews are local and are not shared until you confirm."
                       : "Preview capture is unavailable; source selection still works.")));
  window->set_cancel_label(
      slint::SharedString(translated(translations, "Cancel")));
  window->set_share_label(
      slint::SharedString(translated(translations, "Share")));
  applyTheme(window, state);

  std::vector<ShareSource> model;
  model.reserve(sources.size());
  for (const auto &source : sources) {
    ShareSource row;
    row.label = slint::SharedString(source.label);
    if (!source.preview.empty() &&
        std::filesystem::is_regular_file(source.preview))
      row.preview = slint::Image::load_from_path(
          slint::SharedString(source.preview.string()));
    model.push_back(std::move(row));
  }
  window->set_sources(std::make_shared<slint::VectorModel<ShareSource>>(
      std::move(model)));

  std::optional<std::string> selected;
  window->on_confirm([&](int index) {
    if (index >= 0 && static_cast<std::size_t>(index) < sources.size())
      selected = sources[static_cast<std::size_t>(index)].raw;
    slint::quit_event_loop();
  });
  window->on_cancel([] { slint::quit_event_loop(); });
  window->fn_reset_focus();
  try {
    window->run();
  } catch (const std::exception &error) {
    std::cerr << "LunaDash ScreenCast chooser could not start: " << error.what()
              << '\n';
    return 1;
  }
  if (selected)
    std::cout << *selected << '\n' << std::flush;
  return 0;
}

} // namespace LunaDash
