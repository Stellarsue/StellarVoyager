cmake_minimum_required(VERSION 3.16)

project(StellarVoyager VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if (APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 17)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
elseif (WIN32)
    enable_language(RC)
endif()

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Network OpenGL OpenGLWidgets)

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
    statusdashboard.cpp
    statusdashboard.h
    starmapwidget.cpp
    starmapwidget.h
    themeicons.cpp
    themeicons.h
    systeminfo.cpp
    systeminfo.h
)

if (APPLE)
    list(APPEND MONITOR_SOURCES macmenubarstatus.h macmenubarstatus.mm)
    list(APPEND MONITOR_SOURCES macfan.h macfan.cpp)
endif()

option(STELLARSUE_BUNDLE_TIMES "Bundle the intact core fonts installer (not for for-profit distribution)" ON)
if (STELLARSUE_BUNDLE_TIMES)
    list(APPEND MONITOR_SOURCES fonts.qrc)
endif()

add_executable(system-monitor main.cpp ${MONITOR_SOURCES})
target_compile_definitions(system-monitor PRIVATE STELLARVOYAGER_VERSION="${PROJECT_VERSION}")
if (WIN32)
    target_sources(system-monitor PRIVATE icons/stellarvoyager.rc)
    target_include_directories(system-monitor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/icons)
endif()

target_link_libraries(system-monitor PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)
if (APPLE)
    target_link_libraries(system-monitor PRIVATE "-framework AppKit" "-framework IOKit")
elseif (WIN32)
    target_link_libraries(system-monitor PRIVATE Iphlpapi Advapi32)
endif()

option(STELLARSUE_BUILD_SMOKE_TESTS "Build the interactive X11/OpenGL smoke test" OFF)
if (STELLARSUE_BUILD_SMOKE_TESTS)
    add_executable(stellarsue-smoke tests/visual_smoke.cpp ${MONITOR_SOURCES})
    target_include_directories(stellarsue-smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(stellarsue-smoke PRIVATE
        Qt6::Widgets Qt6::Network Qt6::OpenGL Qt6::OpenGLWidgets)
    if (APPLE)
        target_link_libraries(stellarsue-smoke PRIVATE "-framework AppKit" "-framework IOKit")
    elseif (WIN32)
        target_link_libraries(stellarsue-smoke PRIVATE Iphlpapi Advapi32)
    endif()
    if (STELLARSUE_BUNDLE_TIMES)
        add_executable(stellarsue-font-test tests/font_install.cpp fontbundle.cpp fonts.qrc)
        target_include_directories(stellarsue-font-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_compile_definitions(stellarsue-font-test PRIVATE STELLARSUE_TESTING)
        target_link_libraries(stellarsue-font-test PRIVATE Qt6::Widgets)
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
    install(FILES icons/linux-system-monitor.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/512x512/apps
    )
    install(FILES assets/nasa-shuttle/SOURCE.md
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/stellarsue/nasa-shuttle
    )
    install(FILES automation/news_publisher.py automation/requirements.txt automation/README.md
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/stellarsue/automation
    )
    install(PROGRAMS automation/setup.sh automation/manual_publish.sh
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/stellarsue/automation
    )
    if (STELLARSUE_BUNDLE_TIMES)
        install(FILES assets/corefonts/EULA.html assets/corefonts/SOURCE.md
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/stellarsue/corefonts)
    endif()
    install(FILES README.md packaging/THIRD_PARTY.md
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/stellarsue)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "stellarvoyager")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_CONTACT "xiexu <1357142137@qq.com>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Interactive 3D Linux system monitor")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Stellarsue/StellarVoyager")
    set(CPACK_PACKAGE_FILE_NAME "StellarVoyager-${PROJECT_VERSION}-linux-amd64")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "qt6-qpa-plugins, fontconfig, cabextract, fonts-liberation2, fonts-noto-cjk")
    include(CPack)
endif()
