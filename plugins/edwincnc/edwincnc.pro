#-------------------------------------------------
#
# Project created by QtCreator 2011-03-22T19:33:11
#
#-------------------------------------------------

QT       += gui
TEMPLATE = lib
CONFIG += plugin
VERSION = 1.0.1
TARGET = $$qtLibraryTarget(edwincnc)

GENERATED_DIR = ../../generated/plugin/edwincnc
# Use common project definitions.
include(../../common.pri)

# For plugins
INCLUDEPATH    += ../../librecad/src/plugins

SOURCES += edwincnc.cpp generador.cpp

HEADERS += edwincnc.h cam.h

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
    QMAKE_CXXFLAGS += /utf-8
}
