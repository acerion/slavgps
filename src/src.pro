TEMPLATE = app
TARGET = slavgps

QT = core gui widgets

SOURCES += main_qt.cpp \
    window_qt.cpp \
    vikviewport.cpp \
    vikcoord.cpp \
    coords.cpp \
    degrees_converters.cpp \
    slav_qt.cpp \
    viklayer_defaults.cpp \
    viklayer.cpp \
    vikcoordlayer.cpp \
    vikaggregatelayer.cpp \
    viklayerspanel.cpp \
    viktreeview.cpp \
    uibuilder.cpp \
    uibuilder_qt.cpp \
    widget_color_button.cpp


HEADERS += window_qt.h \
    vikviewport.h \
    vikcoord.h \
    coords.h \
    globals.h \
    degrees_converters.h \
    slav_qt.h \
    viklayer_defaults.h \
    viklayer.h \
    vikcoordlayer.h \
    vikaggregatelayer.h \
    viklayerspanel.h \
    viktreeview.h \
    uibuilder.h \
    uibuilder_qt.h \
    widget_color_button.h


# For glib library.
CONFIG += link_pkgconfig debug
PKGCONFIG += glib-2.0

DEFINES += SLAVGPS_QT HAVE_CONFIG_H



QMAKE_CXXFLAGS += -std=c++11 -Wno-unused -g -O0
QMAKE_LDFLAGS += -lm
