cmake_minimum_required(VERSION 3.16)

project(cpu-server VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Добавляем PrintSupport в компоненты Qt
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network PrintSupport)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network PrintSupport)

# Добавляем исходники QCustomPlot из подкаталога
set(QCUSTOMPLOT_SOURCES
    qcustomplot/qcustomplot.cpp
)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    ${QCUSTOMPLOT_SOURCES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(cpu-server
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        axistag.h axistag.cpp
    )
else()
    if(ANDROID)
        add_library(cpu-server SHARED ${PROJECT_SOURCES})
    else()
        add_executable(cpu-server ${PROJECT_SOURCES})
    endif()
endif()

# Добавляем подкаталог в пути поиска заголовков
target_include_directories(cpu-server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/qcustomplot)

# Добавляем PrintSupport в линковку
target_link_libraries(cpu-server PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::PrintSupport
)

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.cpu-server)
endif()
set_target_properties(cpu-server PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS cpu-server
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(cpu-server)
endif()
