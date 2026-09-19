#include "core/templates/Module.hpp"
#include "desktop/app/DesktopApplication.hpp"

int main(int argc, char **argv) {
  return LunaDash::Templates::runModule<LunaDash::DesktopApplication>(argc,
                                                                      argv);
}
