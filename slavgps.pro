TEMPLATE = app
TARGET = slavgps

QT = core gui widgets

SOURCES += src/main_qt.cpp \
    src/window_qt.cpp \
    src/vikviewport.cpp \
    src/vikcoord.cpp \
    src/coords.cpp \
    src/degrees_converters.cpp \
    src/slav_qt.cpp \
    src/viklayer_defaults.cpp \
    src/viklayer.cpp \
    src/vikcoordlayer.cpp


HEADERS += src/window_qt.h \
    src/vikviewport.h \
    src/vikcoord.h \
    src/coords.h \
    src/globals.h \
    src/degrees_converters.h \
    src/slav_qt.h \
    src/viklayer_defaults.h \
    src/viklayer.h \
    src/vikcoordlayer.h


# For glib library.
CONFIG += link_pkgconfig debug
PKGCONFIG += glib-2.0

DEFINES += SLAVGPS_QT HAVE_CONFIG_H



QMAKE_CXXFLAGS += -std=c++11 -Wno-unused -g -O0
QMAKE_LDFLAGS += -lm

QMAKE_MAKEFILE = Makefile.qt
