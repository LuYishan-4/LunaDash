#pragma once

#include <string>

namespace LunaDash {

// The directory holding the session shell's shell.slint: next to the executable first (also
// a staged/relocated installation in CI), then the build's LunaDash-Shell checkout and the
// XDG data directories. Empty when no shell is installed.
std::string locateSessionShell();

// The SlintLu revision installed with LunaDash (libexec/lunadash/slintlu, or the build
// tree's copy) first: it is the one the pinned shell was tested with. Then PATH and the
// places `cargo install` uses.
std::string findSlintLu();

// A LunaDash executable next to the compositor, preferring the lunadash- spelling over
// the legacy ludash- one (for example "desktop" or "welcome").
std::string bundledExecutable(const std::string &name);

// The clipboard helper that publishes screenshots, from PATH or the source tree.
std::string findClipboardHelper();

} // namespace LunaDash
