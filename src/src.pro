TEMPLATE = app
TARGET = slavgps

QT = core gui widgets printsupport

RESOURCES = icons.qrc cursors.qrc

SOURCES += main.cpp \
    datasource.cpp \
    garminsymbols.cpp \
    version_check.cpp \
    geotag_exif.cpp \
    clipboard.cpp \
    preferences.cpp \
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
    layer_trw_track_statistics.cpp \
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
    babel_dialog.cpp \
    generic_tools.cpp \
    viewport.cpp \
    viewport_utils.cpp \
    viewport_zoom.cpp \
    coord.cpp \
    coords.cpp \
    degrees_converters.cpp \
    layer_defaults.cpp \
    layer.cpp \
    layer_tool.cpp \
    layer_coord.cpp \
    layer_aggregate.cpp \
    layer_dem.cpp \
    layer_trw.cpp \
    layer_trw_painter.cpp \
    layer_trw_tools.cpp \
    layer_trw_menu.cpp \
    layer_trw_tracks.cpp \
    layer_trw_waypoints.cpp \
    layers_panel.cpp \
    toolbox.cpp \
    layer_trw_track.cpp \
    layer_trw_track_properties_dialog.cpp \
    layer_trw_track_profile_dialog.cpp \
    layer_trw_track_list_dialog.cpp \
    layer_trw_trackpoint_properties.cpp \
    layer_trw_waypoint.cpp \
    layer_trw_waypoint_properties.cpp \
    layer_trw_waypoint_list.cpp \
    tree_item.cpp \
    tree_view.cpp \
    ui_builder.cpp \
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
    file_utils.cpp \
    util.cpp \
    vikutils.cpp \
    measurements.cpp \
    ui_util.cpp \
    misc/kdtree.c \
    misc/strtod.c \
    file.cpp \
    dir.cpp \
    gpx.cpp \
    background.cpp \
    download.cpp \
    curl_download.cpp \
    dialog.cpp \
    application_state.cpp \
    statusbar.cpp \
    modules.cpp \
    variant.cpp


HEADERS += window.h \
    datasource.h \
    garminsymbols.h \
    version_check.h \
    geotag_exif.h \
    clipboard.h \
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
    datasource_gc.h \
    datasource_gps.h \
    datasource_routing.h \
    datasource_osm.h \
    datasource_osm_my_traces.h \
    datasource_url.h \
    datasource_file.h \
    layer_trw_stats.h \
    layer_trw_geotag.h \
    layer_trw_track_statistics.h \
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
    generic_tools.h \
    viewport.h \
    viewport_internal.h \
    viewport_utils.h \
    viewport_zoom.h \
    coord.h \
    coords.h \
    globals.h \
    degrees_converters.h \
    layer_defaults.h \
    layer.h \
    layer_tool.h \
    layer_interface.h \
    layer_coord.h \
    layer_aggregate.h \
    layer_dem.h \
    layer_trw.h \
    layer_trw_painter.h \
    layer_trw_tools.h \
    layer_trw_menu.h \
    layer_trw_dialogs.h \
    layer_trw_tracks.h \
    layer_trw_waypoints.h \
    layers_panel.h \
    toolbox.h \
    layer_trw_track.h \
    layer_trw_track_internal.h \
    layer_trw_track_properties_dialog.h \
    layer_trw_track_profile_dialog.h \
    layer_trw_track_list_dialog.h \
    layer_trw_trackpoint_properties.h \
    layer_trw_waypoint.h \
    layer_trw_waypoint_properties.h \
    layer_trw_waypoint_list.h \
    tree_item.h \
    tree_view.h \
    tree_view_internal.h \
    ui_builder.h \
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
    file_utils.h \
    util.h \
    vikutils.h \
    measurements.h \
    ui_util.h \
    misc/kdtree.h \
    misc/strtod.h \
    file.h \
    dir.h \
    gpx.h \
    background.h \
    download.h \
    curl_download.h \
    dialog.h \
    preferences.h \
    application_state.h \
    statusbar.h \
    modules.h \
    acquire.h \
    babel.h \
    babel_dialog.h \
    variant.h




# toolbar.cpp

# vikmaptype.cpp
# vikmapslayer_compat.cpp
# expedia.cpp
# vikenumtypes.cpp


# For glib library.
CONFIG += link_pkgconfig debug
PKGCONFIG += glib-2.0 zlib gio-2.0

# Put *.o files in the same location as corresponding *.cpp files.
# Useful when moc_*.cpp files are put into separate dir.
# https://wiki.qt.io/Undocumented_QMake#Config_features
CONFIG += object_parallel_to_source

DEFINES += HAVE_CONFIG_H

# Put moc_*.cpp files in a subdirectory.
MOC_DIR = ./moc/


QMAKE_CXXFLAGS += -std=c++11 -Wno-unused -Wshadow -Wall -pedantic -g -O0
QMAKE_LFLAGS += -lm -lbz2 -lmagic -lcurl -lexpat -licuuc -lmapnik
