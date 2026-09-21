cmake_minimum_required(VERSION 3.16)

file(READ "${CMAKE_SOURCE_DIR}/VERSION" VER_RAW)
string(STRIP ${VER_RAW} VER)

project(hpa VERSION ${VER} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(deps REQUIRED IMPORTED_TARGET
    hyprutils
    hyprtoolkit
    hyprgraphics
    hyprlang
    pixman-1
    pango
    libdrm
    sdbus-c++>=2
)

add_executable(hyprpolkitagent
    src/main.cpp
    src/config/Config.cpp
    src/config/Config.hpp
    src/core/Agent.cpp
    src/core/Agent.hpp
    src/core/PolkitListener.cpp
    src/core/PolkitListener.hpp
    src/ui/Dialog.cpp
    src/ui/Dialog.hpp
)

target_link_libraries(hyprpolkitagent PRIVATE PkgConfig::deps)

include(GNUInstallDirs)
set(LIBEXECDIR ${CMAKE_INSTALL_FULL_LIBEXECDIR})
configure_file(assets/hyprpolkitagent-service.in hyprpolkitagent.service @ONLY)
configure_file(assets/hyprpolkitagent-dbus.in hyprpolkitagent-dbus.service @ONLY)

install(TARGETS hyprpolkitagent
        DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
install(FILES ${CMAKE_BINARY_DIR}/hyprpolkitagent.service
        DESTINATION "lib/systemd/user")
install(FILES ${CMAKE_BINARY_DIR}/hyprpolkitagent-dbus.service
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services
        RENAME org.hyprland.hyprpolkitagent.service)
