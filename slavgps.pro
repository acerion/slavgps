TEMPLATE = app
TARGET = slavgps

QT = core gui widgets

SOURCES += src/main_qt.cpp src/window_qt.cpp
HEADERS += src/window_qt.h

QMAKE_MAKEFILE = Makefile.qt
