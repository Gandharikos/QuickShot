#include "quickshot/main_window.hpp"

#include <QApplication>
#include <cstddef>
#include <iostream>
#include <span>
#include <string_view>

namespace {

bool versionRequested(std::span<char*> arguments) {
  for (const char* value : arguments.subspan(1)) {
    const std::string_view argument{value};
    if (argument == "--version" || argument == "-v") {
      return true;
    }
  }

  return false;
}

} // namespace

int main(int argc, char** argv) {
  const std::span arguments{argv, static_cast<std::size_t>(argc)};
  if (versionRequested(arguments)) {
    std::cout << "quickshot " << QUICKSHOT_VERSION << '\n';
    return 0;
  }

  QApplication application(argc, argv);
  QApplication::setApplicationName("Quickshot");
  QApplication::setApplicationVersion(QUICKSHOT_VERSION);

  quickshot::MainWindow window;
  window.show();

  return QApplication::exec();
}
