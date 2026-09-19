#include "compositor/session/SessionApplication.hpp"
#include "core/templates/Module.hpp"

int main(int argc, char **argv) {
  return LunaDash::Templates::runModule<LunaDash::SessionApplication>(argc,
                                                                      argv);
}
