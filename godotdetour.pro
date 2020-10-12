# This file is a project file for QtCreator, as that is what I use to write C++
# You can ignore it if you don't want to use QtCreator as an IDE yourself for this project

TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += $$PWD/godot-cpp/godot-headers
INCLUDEPATH += $$PWD/godot-cpp/include
INCLUDEPATH += $$PWD/godot-cpp/include/core
INCLUDEPATH += $$PWD/godot-cpp/include/gen
INCLUDEPATH += $$PWD/recastnavigation/DebugUtils/Include
INCLUDEPATH += $$PWD/recastnavigation/Detour/Include
INCLUDEPATH += $$PWD/recastnavigation/DetourCrowd/Include
INCLUDEPATH += $$PWD/recastnavigation/DetourTileCache/Include
INCLUDEPATH += $$PWD/recastnavigation/Recast/Include

SOURCES += \
        src/detourcrowdagent.cpp \
        src/detournavigation.cpp \
        src/detournavigationmesh.cpp \
        src/detourobstacle.cpp \
        src/godotdetour.cpp \
        src/util/chunkytrimesh.cpp \
        src/util/detourinputgeometry.cpp \
        src/util/fastlz.c \
        src/util/godotdetourdebugdraw.cpp \
        src/util/godotgeometryparser.cpp \
        src/util/meshdataaccumulator.cpp \
        src/util/navigationmeshhelpers.cpp \
        src/util/recastcontext.cpp

HEADERS += \
    src/detourcrowdagent.h \
    src/detournavigation.h \
    src/detournavigationmesh.h \
    src/detourobstacle.h \
    src/godotdetour.h \
    src/util/chunkytrimesh.h \
    src/util/detourinputgeometry.h \
    src/util/fastlz.h \
    src/util/godotdetourdebugdraw.h \
    src/util/godotgeometryparser.h \
    src/util/meshdataaccumulator.h \
    src/util/navigationmeshhelpers.h \
    src/util/recastcontext.h

