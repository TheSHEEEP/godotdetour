# This file is a project file for QtCreator, as that is what I use to write C++
# You can ignore it if you don't want to use QtCreator as an IDE yourself for this project

TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += $$PWD/godot-cpp/include
INCLUDEPATH += $$PWD/godot-cpp/include/core
INCLUDEPATH += $$PWD/godot-cpp/include/gen
INCLUDEPATH += $$PWD/godot-cpp/godot-headers
INCLUDEPATH += $$PWD/recastnavigation/DebugUtils/Include
INCLUDEPATH += $$PWD/recastnavigation/Detour/Include
INCLUDEPATH += $$PWD/recastnavigation/DetourCrowd/Include
INCLUDEPATH += $$PWD/recastnavigation/DetourTileCache/Include
INCLUDEPATH += $$PWD/recastnavigation/Recast/Include

SOURCES += \
        src/godotdetour.cpp

HEADERS += \
    src/godotdetour.h

