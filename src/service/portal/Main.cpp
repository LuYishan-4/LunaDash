#include "core/templates/Module.hpp"
#include "service/portal/Portal.hpp"
#include "service/portal/ScreenCastChooser.hpp"

#include <QCoreApplication>
#include <cstring>

int main(int argc, char **argv) {
  // Apply to both the D-Bus backend and the xdpw chooser before Qt starts.
  // Neither UI may recursively open its own portal or use native dialogs.
  qputenv("QT_QPA_PLATFORMTHEME", "generic");
  // Newer Qt also probes/registers with the host portal independently of the
  // platform theme. A portal implementation must never activate itself.
  qputenv("QT_NO_XDG_DESKTOP_PORTAL", "1");
  qunsetenv("GTK_USE_PORTAL");
  QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  if (argc > 1 && std::strcmp(argv[1], "--screencast-chooser") == 0)
    return LunaDash::runScreenCastChooser(argc, argv);
  return LunaDash::Templates::runModule<LunaDash::Portal>(argc, argv);
}
