cmake_minimum_required(VERSION 3.24)

project(
  quickshot
  VERSION 0.1.0
  DESCRIPTION "A minimal Qt 6 application template"
  LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)

option(QUICKSHOT_ENABLE_CLANG_TIDY "Run clang-tidy while compiling" OFF)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

if(QUICKSHOT_ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)
  set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE}")
endif()

add_library(
  quickshot_ui STATIC
  src/main_window.cpp src/qdrawwidget.cpp include/quickshot/main_window.hpp
  include/quickshot/qdrawwidget.hpp)
target_include_directories(quickshot_ui PUBLIC include)
target_link_libraries(quickshot_ui PUBLIC Qt6::Widgets)
target_compile_features(quickshot_ui PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(quickshot_ui PRIVATE -Wall -Wextra -Wpedantic
                                              -Wconversion -Wsign-conversion)
endif()

qt_add_executable(quickshot src/main.cpp)
target_link_libraries(quickshot PRIVATE quickshot_ui)
target_compile_definitions(quickshot
                           PRIVATE QUICKSHOT_VERSION="${PROJECT_VERSION}")

install(TARGETS quickshot RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(BUILD_TESTING)
  find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

  qt_add_executable(quickshot_tests tests/main_window_test.cpp)
  target_link_libraries(quickshot_tests PRIVATE quickshot_ui Qt6::Test)

  add_test(NAME quickshot_tests COMMAND quickshot_tests)
  set_tests_properties(quickshot_tests PROPERTIES ENVIRONMENT
                                                  "QT_QPA_PLATFORM=offscreen")
endif()
