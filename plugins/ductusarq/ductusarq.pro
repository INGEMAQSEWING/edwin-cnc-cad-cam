#-------------------------------------------------
#
# DuctusArq 2D - herramientas de arquitectura (plugin)
#
#-------------------------------------------------

QT       += gui
TEMPLATE = lib
# arq.h usa C++17 (structured bindings): con Qt 5 + MSVC common.pri no lo pide.
CONFIG += plugin c++17
VERSION = 1.0.0
TARGET = $$qtLibraryTarget(ductusarq)

GENERATED_DIR = ../../generated/plugin/ductusarq
# Use common project definitions.
include(../../common.pri)

# For plugins
INCLUDEPATH    += ../../librecad/src/plugins

SOURCES += ductusarq.cpp herramientas.cpp espacios.cpp mobiliario.cpp render.cpp rotulo.cpp

HEADERS += ductusarq.h arq.h comun.h muebles.h

# Installation Directory
win32 {
        DESTDIR = ../../windows/resources/plugins
}
unix {
    macx {
        DESTDIR = ../../LibreCAD.app/Contents/Resources/plugins
    }
    else {
        DESTDIR = ../../unix/resources/plugins
    }
}


# Textos en UTF-8 (tildes y eñes)
msvc {
    QMAKE_CXXFLAGS += /utf-8 /std:c++17
}
