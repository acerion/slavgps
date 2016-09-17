TEMPLATE = subdirs
SUBDIRS = src


# For glib library.
CONFIG += link_pkgconfig debug
PKGCONFIG += glib-2.0

DEFINES += SLAVGPS_QT HAVE_CONFIG_H



QMAKE_CXXFLAGS += -std=c++11 -Wno-unused -g -O0
QMAKE_LFLAGS += -lm
