cmake_minimum_required(VERSION 3.16)

project(LinuxSystemMonitor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)

set(MONITOR_SOURCES
    blackholewidget.cpp
    blackholewidget.h
    spacecraftmesh.cpp
    spacecraftmesh.h
    flightpath.cpp
    flightpath.h
    nebulaenvironment.cpp
    nebulaenvironment.h
    themefonts.h
    fontbundle.cpp
    fontbundle.h
    telemetrytooltip.cpp
    telemetrytooltip.h
    stellarsue.qrc
    monitorwidget.cpp
    monitorwidget.h
    starmapwidget.cpp
    starmapwidget.h
    themeicons.cpp
    themeicons.h
    systeminfo.cpp
    systeminfo.h
)

option(STELLARSUE_BUNDLE_TIMES "Bundle the intact core fonts installer (not for for-profit distribution)" ON)
if (STELLARSUE_BUNDLE_TIMES)
    list(APPEND MONITOR_SOURCES fonts.qrc)
endif()

add_executable(system-monitor main.cpp ${MONITOR_SOURCES})

target_link_libraries(system-monitor PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::OpenGL
    Qt${QT_VERSION_MAJOR}::OpenGLWidgets
)

option(STELLARSUE_BUILD_SMOKE_TESTS "Build the interactive X11/OpenGL smoke test" OFF)
if (STELLARSUE_BUILD_SMOKE_TESTS)
    add_executable(stellarsue-smoke tests/visual_smoke.cpp ${MONITOR_SOURCES})
    target_include_directories(stellarsue-smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(stellarsue-smoke PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::OpenGL
        Qt${QT_VERSION_MAJOR}::OpenGLWidgets)
    if (STELLARSUE_BUNDLE_TIMES)
        add_executable(stellarsue-font-test tests/font_install.cpp fontbundle.cpp fonts.qrc)
        target_include_directories(stellarsue-font-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_link_libraries(stellarsue-font-test PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
    endif()
endif()

if (UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS system-monitor
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(FILES linux-system-monitor.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
    )
    install(FILES icons/linux-system-monitor.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
    )
    install(FILES assets/nasa-shuttle/SOURCE.md
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/stellarsue/nasa-shuttle
    )
    if (STELLARSUE_BUNDLE_TIMES)
        install(FILES assets/corefonts/EULA.html assets/corefonts/SOURCE.md
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/stellarsue/corefonts)
    endif()
endif()
