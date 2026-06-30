cmake_minimum_required(VERSION 3.20)

project(AlwaysLang VERSION 3.1)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

add_subdirectory(FluentUI)
add_subdirectory(src)
