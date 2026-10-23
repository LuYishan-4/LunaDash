#include "service/portal/filepicker/FilePickerApplication.hpp"

int main(int argc, char **argv) {
  LunaDash::FilePickerApplication application;
  return application.run(argc, argv);
}
