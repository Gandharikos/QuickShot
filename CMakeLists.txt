cmake_minimum_required(VERSION 3.24)

project(
  quickshot
  VERSION 2.0.0
  DESCRIPTION "A modern Qt 6 image ROI editor"
  LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)

option(QUICKSHOT_ENABLE_CLANG_TIDY "Run clang-tidy while compiling" OFF)
option(QUICKSHOT_ENABLE_DEPLOYMENT
       "Bundle runtime dependencies and enable platform packages" OFF)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Svg Widgets)
qt_standard_project_setup()

if(QUICKSHOT_ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)
  set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE}")
endif()

add_library(
  quickshot_ui STATIC
  src/batch_save_dialog.cpp
  src/canvas_view.cpp
  src/shape_commands.cpp
  src/image_document.cpp
  src/image_scene.cpp
  src/main_window.cpp
  src/roi_exporter.cpp
  src/shape_item.cpp
  src/shapes/bezier_curve.cpp
  src/shapes/box_shape.cpp
  src/shapes/circle.cpp
  src/shapes/ellipse.cpp
  src/shapes/multi_point_shape.cpp
  src/shapes/polygon.cpp
  src/shapes/rectangle.cpp
  src/shapes/shape.cpp
  include/quickshot/batch_save_dialog.hpp
  include/quickshot/canvas_view.hpp
  include/quickshot/shape_commands.hpp
  include/quickshot/image_document.hpp
  include/quickshot/image_scene.hpp
  include/quickshot/main_window.hpp
  include/quickshot/roi_exporter.hpp
  include/quickshot/shape_item.hpp
  include/quickshot/shapes/bezier_curve.hpp
  include/quickshot/shapes/box_shape.hpp
  include/quickshot/shapes/circle.hpp
  include/quickshot/shapes/ellipse.hpp
  include/quickshot/shapes/multi_point_shape.hpp
  include/quickshot/shapes/polygon.hpp
  include/quickshot/shapes/rectangle.hpp
  include/quickshot/shapes/shape.hpp
  include/quickshot/shapes/shape_handle.hpp)
target_include_directories(quickshot_ui PUBLIC include)
target_link_libraries(quickshot_ui PUBLIC Qt6::Svg Qt6::Widgets)
target_compile_features(quickshot_ui PUBLIC cxx_std_20)

qt_add_resources(
  quickshot_ui
  "quickshot_icons"
  PREFIX
  "/quickshot/icons"
  BASE
  "assets/icons"
  FILES
  assets/icons/bezier-curve.svg
  assets/icons/circle.svg
  assets/icons/ellipse.svg
  assets/icons/polygon.svg
  assets/icons/rectangle.svg
  assets/icons/rotate-cursor.svg
  assets/icons/rotate-left.svg
  assets/icons/rotate-right.svg)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(quickshot_ui PRIVATE -Wall -Wextra -Wpedantic
                                              -Wconversion -Wsign-conversion)
endif()

qt_add_executable(quickshot src/main.cpp)
target_link_libraries(quickshot PRIVATE quickshot_ui)
target_compile_definitions(quickshot
                           PRIVATE QUICKSHOT_VERSION="${PROJECT_VERSION}")

if(APPLE)
  set_target_properties(
    quickshot
    PROPERTIES MACOSX_BUNDLE TRUE
               MACOSX_BUNDLE_BUNDLE_NAME "QuickShot"
               MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.gandharikos.quickshot"
               MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
               MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
               OUTPUT_NAME "QuickShot")
elseif(WIN32)
  set_target_properties(quickshot PROPERTIES OUTPUT_NAME "QuickShot"
                                             WIN32_EXECUTABLE TRUE)
endif()

install(
  TARGETS quickshot
  BUNDLE DESTINATION .
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(QUICKSHOT_ENABLE_DEPLOYMENT)
  qt_generate_deploy_app_script(
    TARGET quickshot OUTPUT_SCRIPT quickshot_deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR)
  install(SCRIPT ${quickshot_deploy_script})
endif()

if(BUILD_TESTING)
  find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

  qt_add_executable(graphics_scene_tests tests/graphics_scene_test.cpp)
  target_link_libraries(graphics_scene_tests PRIVATE quickshot_ui Qt6::Test)

  qt_add_executable(quickshot_tests tests/main_window_test.cpp)
  target_link_libraries(quickshot_tests PRIVATE quickshot_ui Qt6::Test)

  qt_add_executable(shape_tests tests/shape_test.cpp)
  target_link_libraries(shape_tests PRIVATE quickshot_ui Qt6::Test)

  add_test(NAME graphics_scene_tests COMMAND graphics_scene_tests)
  # Persist the QTest report because Windows runners can lose its console
  # output.
  add_test(NAME quickshot_tests COMMAND quickshot_tests -o
                                        quickshot_tests.txt,txt)
  add_test(NAME shape_tests COMMAND shape_tests)
  set_tests_properties(graphics_scene_tests quickshot_tests shape_tests
                       PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(QUICKSHOT_ENABLE_DEPLOYMENT)
  set(CPACK_PACKAGE_NAME "QuickShot")
  set(CPACK_PACKAGE_VENDOR "Gandharikos")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
      "A modern C++20 and Qt 6 image ROI editor")
  set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "QuickShot")
  set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
  set(CPACK_VERBATIM_VARIABLES YES)

  if(WIN32)
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_PACKAGE_FILE_NAME "QuickShot-${PROJECT_VERSION}-windows-x86_64")
  elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_PACKAGE_FILE_NAME "QuickShot-${PROJECT_VERSION}-macos-universal")
  else()
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_FILE_NAME "QuickShot-${PROJECT_VERSION}-linux-x86_64")
  endif()

  include(CPack)
endif()
