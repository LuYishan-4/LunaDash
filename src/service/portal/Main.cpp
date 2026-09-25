#include "core/templates/Module.hpp"
#include "service/portal/Portal.hpp"
#include "service/portal/ScreenCastChooser.hpp"

#include <cstring>

int main(int argc, char **argv) {
  if (argc > 1 && std::strcmp(argv[1], "--screencast-chooser") == 0)
    return LunaDash::runScreenCastChooser(argc, argv);
  return LunaDash::Templates::runModule<LunaDash::Portal>(argc, argv);
}
