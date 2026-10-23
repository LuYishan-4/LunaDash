#include "desktop/app/DesktopApplication.hpp"

#include "config/BuildConfig.hpp"
#include "desktop/app/DefaultApplications.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

namespace LunaDash {
namespace {

std::filesystem::path executableDirectory(int argc, char **argv) {
  std::vector<char> buffer(4096);
  const auto count = ::readlink("/proc/self/exe", buffer.data(),
                                buffer.size() - 1);
  if (count > 0) {
    buffer[static_cast<std::size_t>(count)] = '\0';
    return std::filesystem::path(buffer.data()).parent_path();
  }
  if (argc > 0 && argv && argv[0]) {
    std::error_code error;
    auto path = std::filesystem::absolute(argv[0], error);
    if (!error)
      return path.parent_path();
  }
  return {};
}

int execCommand(const std::vector<std::string> &command) {
  if (command.empty() || command.front().empty()) {
    std::fprintf(stderr, "No executable was configured for this action.\n");
    return 2;
  }
  std::vector<char *> arguments;
  arguments.reserve(command.size() + 1);
  for (const auto &argument : command)
    arguments.push_back(const_cast<char *>(argument.c_str()));
  arguments.push_back(nullptr);
  ::execvp(command.front().c_str(), arguments.data());
  std::fprintf(stderr, "Could not start %s: %s\n", command.front().c_str(),
               std::strerror(errno));
  return errno == ENOENT ? 127 : 126;
}

int execSibling(const std::filesystem::path &directory,
                std::string_view executable,
                const std::vector<std::string> &arguments = {}) {
  const auto path = directory / executable;
  std::vector<std::string> command;
  command.reserve(arguments.size() + 1);
  command.push_back(path.string());
  command.insert(command.end(), arguments.begin(), arguments.end());
  std::vector<char *> rawArguments;
  rawArguments.reserve(command.size() + 1);
  for (const auto &argument : command)
    rawArguments.push_back(const_cast<char *>(argument.c_str()));
  rawArguments.push_back(nullptr);
  ::execv(path.c_str(), rawArguments.data());
  std::fprintf(stderr, "Could not start %s: %s\n", path.c_str(),
               std::strerror(errno));
  return errno == ENOENT ? 127 : 126;
}

void printUsage() {
  std::puts("Usage: lunadash-desktop [--app welcome|settings|plugins|files|packages] [--path folder]");
  std::puts("Without --app, start the LunaDash Wayland compositor.");
}

} // namespace

int DesktopApplication::run(int argc, char **argv) {
  std::string app;
  bool appSpecified = false;
  std::string path;
  std::vector<std::string> forwarded;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (argument == "--help" || argument == "-h") {
      printUsage();
      return 0;
    }
    if (argument == "--version") {
      std::puts(BuildConfig::version);
      return 0;
    }
    if (argument == "--app") {
      if (index + 1 >= argc) {
        std::fputs("--app requires an application name.\n", stderr);
        return 2;
      }
      app = argv[++index];
      appSpecified = true;
      continue;
    }
    if (argument.starts_with("--app=")) {
      app = argument.substr(6);
      appSpecified = true;
      continue;
    }
    if (argument == "--path") {
      if (index + 1 >= argc) {
        std::fputs("--path requires a folder.\n", stderr);
        return 2;
      }
      path = argv[++index];
      continue;
    }
    if (argument.starts_with("--path=")) {
      path = argument.substr(7);
      continue;
    }
    forwarded.emplace_back(argument);
  }

  const auto directory = executableDirectory(argc, argv);
  if (!appSpecified) {
    if (::access((directory / "lunadash-compositor").c_str(), X_OK) == 0)
      return execSibling(directory, "lunadash-compositor");
    return execSibling(directory, "ludash-compositor");
  }
  if (app == "welcome")
    return execSibling(directory, "lunadash-welcome", forwarded);
  if (app == "packages")
    return execSibling(directory, "lunadash-packages", forwarded);
  if (app == "settings" || app == "plugins") {
    std::vector<std::string> command{
        (directory / "lunadashctl").string(), "open-settings",
        app == "plugins" ? "plugins" : "general"};
    return execCommand(command);
  }
  if (app == "files") {
    std::string error;
    auto command = defaultApplicationCommand("files", &error);
    if (command.empty()) {
      std::fprintf(stderr, "%s\n", error.empty()
                                       ? "No file manager is configured."
                                       : error.c_str());
      return 2;
    }
    if (!path.empty()) {
      std::error_code filesystemError;
      const auto absolute = std::filesystem::absolute(path, filesystemError);
      if (filesystemError) {
        std::fprintf(stderr, "Could not resolve the requested folder: %s\n",
                     filesystemError.message().c_str());
        return 2;
      }
      command.push_back(absolute.string());
    }
    return execCommand(command);
  }

  std::fprintf(stderr, "Unknown LunaDash application: %s\n", app.c_str());
  return 2;
}

} // namespace LunaDash
