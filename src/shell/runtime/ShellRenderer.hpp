#pragma once
#include <QProcessEnvironment>
namespace LunaDash {
bool configureShellRendering(QProcessEnvironment &environment,
                             bool nvidiaDriver);
}
