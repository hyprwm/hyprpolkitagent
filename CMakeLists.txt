cmake_minimum_required(VERSION 3.16)

# Get version
file(READ "${CMAKE_SOURCE_DIR}/VERSION" VER_RAW)
string(STRIP ${VER_RAW} VER)

project(hpa VERSION ${VER} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Quick QuickControls2)
find_package(PkgConfig REQUIRED)
set(CMAKE_CXX_STANDARD 23)

pkg_check_modules(
  deps
  REQUIRED
  IMPORTED_TARGET
  hyprutils
  polkit-agent-1
  polkit-qt6-1)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(hyprpolkitagent
    src/main.cpp
    src/core/Agent.cpp
    src/core/Agent.hpp
    src/core/PolkitListener.hpp
    src/core/PolkitListener.cpp
    src/QMLIntegration.cpp
    src/QMLIntegration.hpp
)

qt_add_qml_module(hyprpolkitagent
    URI hpa
    VERSION 1.0
    QML_FILES
        qml/main.qml
    SOURCES
        src/QMLIntegration.cpp
        src/QMLIntegration.hpp
)

target_link_libraries(hyprpolkitagent
    PRIVATE Qt6::Widgets Qt6::Quick Qt6::Gui Qt6::QuickControls2 PkgConfig::deps
)

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
        RENAME hyprpolkitagent.service)
