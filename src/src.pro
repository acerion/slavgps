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
    layer_trw_menu.cpp \
    viktrwlayer_propwin.cpp \
    layers_panel.cpp \
    track.cpp \
    trackpoint_properties.cpp \
    waypoint.cpp \
    waypoint_properties.cpp \
    waypoint_list.cpp \
    tree_view.cpp \
    uibuilder.cpp \
    uibuilder_qt.cpp \
    widget_color_button.cpp \
    widget_file_list.cpp \
    widget_file_entry.cpp \
    widget_radio_group.cpp \
    date_time_dialog.cpp \
    dem.cpp \
    dems.cpp \
    srtm_continent.cpp \
    compression.cpp \
    fileutils.cpp \
    util.cpp \
    vikutils.cpp \
    ui_util.cpp \
    misc/kdtree.c \
    file.cpp \
    dir.cpp \
    gpx.cpp \
    background.cpp \
    download.cpp \
    curl_download.cpp \
    dialog.cpp \
    preferences.cpp \
    settings.cpp \
    statusbar.cpp \
    modules.cpp


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
    viktrwlayer_propwin.h \
    layers_panel.h \
    track.h \
    trackpoint_properties.h \
    waypoint.h \
    waypoint_properties.h \
    waypoint_list.h \
    tree_view.h \
    uibuilder.h \
    uibuilder_qt.h \
    widget_color_button.h \
    widget_file_list.h \
    widget_file_entry.h \
    widget_radio_group.h \
    date_time_dialog.h \
    dem.h \
    dems.h \
    compression.h \
    fileutils.h \
    util.h \
    vikutils.h \
    ui_util.h \
    misc/kdtree.h \
    file.h \
    dir.h \
    gpx.h \
    background.h \
    download.h \
    curl_download.h \
    dialog.h \
    preferences.h \
    settings.h \
    statusbar.h \
    modules.h


# For glib library.
CONFIG += link_pkgconfig debug
PKGCONFIG += glib-2.0 zlib gio-2.0

DEFINES += SLAVGPS_QT HAVE_CONFIG_H



QMAKE_CXXFLAGS += -std=c++11 -Wno-unused -g -O0
QMAKE_LFLAGS += -lm -lbz2 -lmagic -lcurl -lexpat
