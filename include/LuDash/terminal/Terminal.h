#pragma once
#include <QWidget>

namespace LuDash {

// Builds the built-in terminal used by the "terminal" default action. It runs
// an interactive Fish shell in a pseudo-terminal, draws a translucent surface
// that follows the desktop accent, and reads palette overrides from the same
// QSettings store the rest of the desktop uses.
QWidget *createTerminal();

} // namespace LuDash
