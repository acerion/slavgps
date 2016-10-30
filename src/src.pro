TEMPLATE = app
TARGET = slavgps

QT = core gui widgets

RESOURCES = icons.qrc cursors.qrc

SOURCES += main.cpp \
    window.cpp \
    window_layer_tools.cpp \
    viewport.cpp \
    coord.cpp \
    coords.cpp \
    globals.cpp \
    degrees_converters.cpp \
    slav_qt.cpp \
    layer_defaults.cpp \
    layer.cpp \
    layer_coord.cpp \
    layer_aggregate.cpp \
    layer_dem.cpp \
    layer_trw.cpp \
    layer_trw_draw.cpp \
    layer_trw_tools.cpp \
    layer_trw_containers.cpp \
    layers_panel.cpp \
    track.cpp \
    viktrwlayer_tpwin.cpp \
    waypoint.cpp \
    waypoint_properties.cpp \
    tree_view.cpp \
    uibuilder.cpp \
    uibuilder_qt.cpp \
    widget_color_button.cpp \
    widget_file_list.cpp \
    widget_file_entry.cpp \
    widget_radio_group.cpp \
    dem.cpp \
    dems.cpp \
    srtm_continent.cpp \
    compression.cpp \
    fileutils.cpp \
    util.cpp \
    file.cpp \
    dir.cpp \
    background.cpp \
    download.cpp \
    curl_download.cpp \
    dialog.cpp \
    preferences.cpp \
    settings.cpp \
    statusbar.cpp


HEADERS += window.h \
    window_layer_tools.h \
    viewport.h \
    coord.h \
    coords.h \
    globals.h \
    degrees_converters.h \
    slav_qt.h \
    layer_defaults.h \
    layer.h \
    layer_coord.h \
    layer_aggregate.h \
    layer_dem.h \
    layer_trw.h \
    layer_trw_draw.h \
    layer_trw_tools.h \
    layer_trw_containers.h \
    layers_panel.h \
    track.h \
    viktrwlayer_tpwin.h \
    waypoint.h \
    waypoint_properties.h \
    layer_trw.h \
    tree_view.h \
    uibuilder.h \
    uibuilder_qt.h \
    widget_color_button.h \
    widget_file_list.h \
    widget_file_entry.h \
    widget_radio_group.h \
    dem.h \
    dems.h \
    compression.h \
    fileutils.h \
    util.h \
    file.h \
    dir.h \
    background.h \
    download.h \
    curl_download.h \
    dialog.h \
    preferences.h \
    settings.h \
    statusbar.h


# For glib library.
CONFIG += link_pkgconfig debug
PKGCONFIG += glib-2.0 zlib gio-2.0

DEFINES += SLAVGPS_QT HAVE_CONFIG_H



QMAKE_CXXFLAGS += -std=c++11 -Wno-unused -g -O0
QMAKE_LFLAGS += -lm -lbz2 -lmagic -lcurl
