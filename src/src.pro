TEMPLATE = app
TARGET = slavgps

QT = core gui widgets

RESOURCES = icons.qrc

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
    vikdemlayer.cpp \
    viklayerspanel.cpp \
    viktreeview.cpp \
    uibuilder.cpp \
    uibuilder_qt.cpp \
    widget_color_button.cpp \
    widget_file_list.cpp \
    dem.cpp \
    dems.cpp \
    fileutils.cpp


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
    vikdemlayer.h \
    viklayerspanel.h \
    viktreeview.h \
    uibuilder.h \
    uibuilder_qt.h \
    widget_color_button.h \
    widget_file_list.h \
    dem.h \
    dems.h \
    fileutils.h


# For glib library.
CONFIG += link_pkgconfig debug
PKGCONFIG += glib-2.0

DEFINES += SLAVGPS_QT HAVE_CONFIG_H



QMAKE_CXXFLAGS += -std=c++11 -Wno-unused -g -O0
QMAKE_LDFLAGS += -lm
