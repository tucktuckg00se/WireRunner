cmake_minimum_required(VERSION 3.28)
project(WireRunner VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WIRERUNNER_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(WIRERUNNER_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test)
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WirePlumber REQUIRED IMPORTED_TARGET wireplumber-0.5)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
  if(WIRERUNNER_WARNINGS_AS_ERRORS)
    add_compile_options(-Werror)
  endif()
  if(WIRERUNNER_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
  endif()
endif()

include(CTest)
add_subdirectory(src)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

install(FILES packaging/io.github.tucktuckg00se.WireRunner.desktop
  DESTINATION share/applications)
install(FILES packaging/io.github.tucktuckg00se.WireRunner.metainfo.xml
  DESTINATION share/metainfo)
install(FILES packaging/io.github.tucktuckg00se.WireRunner.svg
  DESTINATION share/icons/hicolor/scalable/apps)
