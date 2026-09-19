#include "core/templates/Module.hpp"
#include "service/portal/Portal.hpp"

int main(int argc, char **argv) {
  return LunaDash::Templates::runModule<LunaDash::Portal>(argc, argv);
}
