#pragma once
#include <QProcessEnvironment>
namespace LuDash {
bool configureShellRendering(QProcessEnvironment& environment, bool nvidiaDriver);
}
