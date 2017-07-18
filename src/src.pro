TEMPLATE = app
TARGET = slavgps

QT = core gui widgets printsupport

RESOURCES = icons.qrc cursors.qrc

SOURCES += main.cpp \
    layer_trw_dialogs.cpp \
    routing.cpp \
    routing_engine.cpp \
    routing_engine_web.cpp \
    vikwebtoolbounds.cpp \
    vikwebtoolcenter.cpp \
    vikwebtool.cpp \
    vikwebtool_datasource.cpp \
    vikwebtoolformat.cpp \
    bluemarble.cpp \
    external_tool.cpp \
    vikexttool_datasources.cpp \
    external_tools.cpp \
    bing.cpp \
    google.cpp \
    googlesearch.cpp \
    map_source_slippy.cpp \
    map_source_bing.cpp \
    map_source_wmsc.cpp \
    map_source_tms.cpp \
    map_source_terraserver.cpp \
    terraserver.cpp \
    geonamessearch.cpp \
    geojson.cpp \
    kmz.cpp \
    gpspoint.cpp \
    thumbnails.cpp \
    osm.cpp \
    osm-traces.cpp \
    datasource_bfilter.cpp \
    datasource_gc.cpp \
    datasource_geojson.cpp \
    datasource_geotag.cpp \
    datasource_gps.cpp \
    datasource_osm.cpp \
    datasource_osm_my_traces.cpp \
    datasource_routing.cpp \
    datasource_url.cpp \
    datasource_wikipedia.cpp \
    layer_trw_stats.cpp \
    layer_trw_export.cpp \
    layer_trw_geotag.cpp \
    track_statistics.cpp \
    jpg.cpp \
    mapnik_interface.cpp \
    gpsmapper.cpp \
    layer_georef.cpp \
    layer_gps.cpp \
    layer_mapnik.cpp \
    print.cpp \
    print-preview.cpp \
    layer_map.cpp \
    map_source.cpp \
    map_cache.cpp \
    map_utils.cpp \
    metatile.cpp \
    goto.cpp \
    goto_tool.cpp \
    goto_tool_xml.cpp \
    geonames.cpp \
    window.cpp \
    acquire.cpp \
    datasource_file.cpp \
    babel.cpp \
    babel_ui.cpp \
    window_layer_tools.cpp \
    viewport.cpp \
    viewport_utils.cpp \
    viewport_zoom.cpp \
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
    layer_trw_painter.cpp \
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
    widget_list_selection.cpp \
    widget_slider.cpp \
    date_time_dialog.cpp \
    dem.cpp \
    dem_cache.cpp \
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
    modules.cpp \
    variant.cpp


HEADERS += window.h \
    routing.h \
    routing_engine.h \
    routing_engine_web.h \
    vikwebtoolbounds.h \
    vikwebtoolcenter.h \
    vikwebtool.h \
    vikwebtool_datasource.h \
    vikwebtoolformat.h \
    bluemarble.h \
    external_tool.h \
    vikexttool_datasources.h \
    external_tools.h \
    bing.h \
    google.h \
    googlesearch.h \
    map_source_slippy.h \
    map_source_bing.h \
    map_source_wmsc.h \
    map_source_tms.h \
    map_source_terraserver.h \
    terraserver.h \
    geonamessearch.h \
    geojson.h \
    kmz.h \
    gpspoint.h \
    thumbnails.h \
    osm.h \
    osm-traces.h \
    datasource_gps.h \
    layer_trw_stats.h \
    layer_trw_export.h \
    layer_trw_geotag.h \
    track_statistics.h \
    jpg.h \
    mapnik_interface.h \
    gpsmapper.h \
    layer_georef.h \
    layer_gps.h \
    layer_mapnik.h \
    print.h \
    print-preview.h \
    layer_map.h \
    map_source.h \
    map_utils.h \
    metatile.h \
    map_cache.h \
    goto.h \
    goto_tool.h \
    goto_tool_xml.h \
    geonames.h \
    window_layer_tools.h \
    viewport.h \
    viewport_internal.h \
    viewport_utils.h \
    viewport_zoom.h \
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
    layer_trw_painter.h \
    layer_trw_tools.h \
    layer_trw_containers.h \
    layer_trw_dialogs.h \
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
    tree_view_internal.h \
    uibuilder_qt.h \
    widget_color_button.h \
    widget_file_list.h \
    widget_file_entry.h \
    widget_radio_group.h \
    widget_list_selection.h \
    widget_slider.h \
    date_time_dialog.h \
    dem.h \
    dem_cache.h \
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
    babel_ui.h \
    variant.h


# clipboard.cpp
# garminsymbols.cpp
# geotag_exif.cpp
# toolbar.cpp

# vikmaptype.cpp
# vikmapslayer_compat.cpp
# expedia.cpp
# vikenumtypes.cpp


# For glib library.
CONFIG += link_pkgconfig debug
PKGCONFIG += glib-2.0 zlib gio-2.0

DEFINES += SLAVGPS_QT HAVE_CONFIG_H



QMAKE_CXXFLAGS += -std=c++11 -Wno-unused -Wshadow -Wall -pedantic -g -O0
QMAKE_LFLAGS += -lm -lbz2 -lmagic -lcurl -lexpat -licuuc -lmapnik
