TEMPLATE = app
TARGET = slavgps

QT = core gui widgets

RESOURCES = icons.qrc cursors.qrc

SOURCES += main.cpp \
    window.cpp \
    acquire.cpp \
    datasource_file.cpp \
    babel.cpp \
    babel_ui.cpp \
    window_layer_tools.cpp \
    viewport.cpp \
    viewport_utils.cpp \
    coord.cpp \
    coords.cpp \
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
    layers_panel.cpp \
    layer_toolbox.cpp \
    track.cpp \
    track_properties_dialog.cpp \
    track_profile_dialog.cpp \
    track_list_dialog.cpp \
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
    viewport_utils.h \
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
    layer_toolbox.h \
    track.h \
    track_properties_dialog.h \
    track_profile_dialog.h \
    track_list_dialog.h \
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
    modules.h \
    acquire.h \
    datasource_file.h \
    babel.h \
    babel_ui.h



# bing.cpp
# bingmapsource.cpp
# bluemarble.cpp
# clipboard.cpp
# datasource_bfilter.cpp
# datasource_gc.cpp
# datasource_geojson.cpp
# datasource_geotag.cpp
# datasource_gps.cpp
# datasource_osm.cpp
# datasource_osm_my_traces.cpp
# datasource_routing.cpp
# datasource_url.cpp
# datasource_wikipedia.cpp
# expedia.cpp
# garminsymbols.cpp
# geojson.cpp
# geonames.cpp
# geonamessearch.cpp
# geotag_exif.cpp
# google.cpp
# googlesearch.cpp
# gpsmapper.cpp
# gpspoint.cpp
# jpg.cpp
# kmz.cpp
# mapcache.cpp
# mapnik_interface.cpp
# maputils.cpp
# metatile.cpp
# osm.cpp
# osm-traces.cpp
# print.cpp
# print-preview.cpp
# qrc_cursors.cpp
# qrc_icons.cpp
# terraserver.cpp
# terraservermapsource.cpp
# thumbnails.cpp
# toolbar.cpp
# track_statistics.cpp
# vik_compat.cpp
# vikenumtypes.cpp
# vikexttool.cpp
# vikexttool_datasources.cpp
# vikexttools.cpp
# vikfileentry.cpp
# vikfilelist.cpp
# vikgeoreflayer.cpp
# vikgobjectbuilder.cpp
# vikgoto.cpp
# vikgototool.cpp
# vikgotoxmltool.cpp
# vikgpslayer.cpp
# vikmapniklayer.cpp
# vikmapslayer_compat.cpp
# vikmapslayer.cpp
# vikmapsource.cpp
# vikmaptype.cpp
# vikradiogroup.cpp
# vikrouting.cpp
# vikroutingengine.cpp
# vikroutingwebengine.cpp
# vikslippymapsource.cpp
# viktmsmapsource.cpp
# viktrwlayer_analysis.cpp
# viktrwlayer_export.cpp
# viktrwlayer_geotag.cpp
# vikwebtoolbounds.cpp
# vikwebtoolcenter.cpp
# vikwebtool.cpp
# vikwebtool_datasource.cpp
# vikwebtoolformat.cpp
# vikwmscmapsource.cpp


# For glib library.
CONFIG += link_pkgconfig debug
PKGCONFIG += glib-2.0 zlib gio-2.0

DEFINES += SLAVGPS_QT HAVE_CONFIG_H



QMAKE_CXXFLAGS += -std=c++11 -Wno-unused -Wshadow -Wall -pedantic -g -O0
QMAKE_LFLAGS += -lm -lbz2 -lmagic -lcurl -lexpat
