TEMPLATE = app
TARGET = slavgps

QT = core gui widgets printsupport xml

RESOURCES = icons.qrc cursors.qrc thumbnails.qrc misc.qrc waypoint.qrc test_data.qrc

SOURCES += main.cpp \
    globals.cpp \
    viewport_gis.cpp \
    layer_trw_track_profile_dialog.cpp \
    babel.cpp \
    mem_cache.cpp \
    tree_item_list.cpp \
    ruler.cpp \
    acquire.cpp \
    window.cpp \
    datasource.cpp \
    datasource_babel.cpp \
    datasource_bfilter.cpp \
    datasource_geocache.cpp \
    datasource_geojson.cpp \
    datasource_geotag.cpp \
    datasource_gps.cpp \
    datasource_osm.cpp \
    datasource_osm_my_traces.cpp \
    datasource_routing.cpp \
    datasource_url.cpp \
    datasource_wikipedia.cpp \
    datasource_file.cpp \
    datasource_webtool.cpp \
    garmin_symbols.cpp \
    version_check.cpp \
    geotag_exif.cpp \
    clipboard.cpp \
    preferences.cpp \
    layer_trw_dialogs.cpp \
    mapcoord.cpp \
    routing.cpp \
    routing_engine.cpp \
    routing_engine_web.cpp \
    webtool_bounds.cpp \
    webtool_center.cpp \
    webtool_query.cpp \
    webtool.cpp \
    webtool_format.cpp \
    bluemarble.cpp \
    external_tool.cpp \
    external_tool_datasources.cpp \
    external_tools.cpp \
    astro.cpp \
    bing.cpp \
    google.cpp \
    googlesearch.cpp \
    map_source_slippy.cpp \
    map_source_mbtiles.cpp \
    map_source_bing.cpp \
    map_source_wmsc.cpp \
    map_source_tms.cpp \
    map_source_terraserver.cpp \
    terraserver.cpp \
    geojson.cpp \
    kmz.cpp \
    gpspoint.cpp \
    thumbnails.cpp \
    bbox.cpp \
    osm.cpp \
    osm_traces.cpp \
    layer_trw_stats.cpp \
    layer_trw_export.cpp \
    layer_trw_geotag.cpp \
    layer_trw_track_statistics.cpp \
    jpg.cpp \
    gpsmapper.cpp \
    layer_georef.cpp \
    layer_gps.cpp \
    layer_mapnik.cpp \
    layer_mapnik_wrapper.cpp \
    print.cpp \
    layer_map.cpp \
    layer_map_download.cpp \
    map_source.cpp \
    map_cache.cpp \
    map_utils.cpp \
    osm_metatile.cpp \
    goto.cpp \
    goto_tool.cpp \
    goto_tool_xml.cpp \
    geonames.cpp \
    geonames_search.cpp \
    babel_dialog.cpp \
    generic_tools.cpp \
    viewport_to_image.cpp \
    viewport_decorations.cpp \
    viewport_zoom.cpp \
    viewport_pixmap.cpp \
    coord.cpp \
    coords.cpp \
    lat_lon.cpp \
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
    layer_trw_track_data.cpp \
    layer_trw_track_split.cpp \
    layer_trw_track_properties_dialog.cpp \
    layer_trw_point_properties.cpp \
    layer_trw_trackpoint_properties.cpp \
    layer_trw_waypoint.cpp \
    layer_trw_waypoint_properties.cpp \
    tree_item.cpp \
    tree_view.cpp \
    ui_builder.cpp \
    widget_color_button.cpp \
    widget_file_list.cpp \
    widget_file_entry.cpp \
    widget_radio_group.cpp \
    widget_list_selection.cpp \
    widget_slider.cpp \
    widget_timestamp.cpp \
    widget_utm_entry.cpp \
    widget_lat_lon_entry.cpp \
    widget_coord_display.cpp \
    widget_measurement_entry.cpp \
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
    dialog_about.cpp \
    application_state.cpp \
    statusbar.cpp \
    modules.cpp \
    variant.cpp \
    graph_intervals.cpp \
    expedia.cpp


HEADERS += window.h \
    mem_cache.h \
    ruler.h \
    datasource.h \
    garmin_symbols.h \
    version_check.h \
    geotag_exif.h \
    clipboard.h \
    routing.h \
    routing_engine.h \
    routing_engine_web.h \
    webtool_bounds.h \
    webtool_center.h \
    webtool_query.h \
    webtool.h \
    webtool_format.h \
    bluemarble.h \
    external_tool.h \
    external_tool_datasources.h \
    external_tools.h \
    astro.h \
    bing.h \
    google.h \
    googlesearch.h \
    map_source_slippy.h \
    map_source_mbtiles.h \
    map_source_bing.h \
    map_source_wmsc.h \
    map_source_tms.h \
    map_source_terraserver.h \
    terraserver.h \
    geojson.h \
    kmz.h \
    gpspoint.h \
    thumbnails.h \
    osm.h \
    osm_traces.h \
    datasource_babel.h \
    datasource_geocache.h \
    datasource_gps.h \
    datasource_routing.h \
    datasource_osm.h \
    datasource_osm_my_traces.h \
    datasource_url.h \
    datasource_file.h \
    datasource_wikipedia.h \
    datasource_geotag.h \
    datasource_geojson.h \
    datasource_bfilter.h \
    datasource_webtool.h \
    layer_trw_definitions.h \
    layer_trw_stats.h \
    layer_trw_geotag.h \
    layer_trw_track_statistics.h \
    jpg.h \
    gpsmapper.h \
    layer_georef.h \
    layer_gps.h \
    layer_mapnik.h \
    layer_mapnik_wrapper.h \
    print.h \
    layer_map.h \
    layer_map_download.h \
    map_source.h \
    map_utils.h \
    osm_metatile.h \
    map_cache.h \
    goto.h \
    goto_tool.h \
    goto_tool_xml.h \
    geonames.h \
    geonames_search.h \
    generic_tools.h \
    viewport.h \
    viewport_gis.h \
    viewport_internal.h \
    viewport_to_image.h \
    viewport_decorations.h \
    viewport_zoom.h \
    viewport_pixmap.h \
    coord.h \
    coords.h \
    globals.h \
    lat_lon.h \
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
    layer_trw_track_data.h \
    layer_trw_track_internal.h \
    layer_trw_track_properties_dialog.h \
    layer_trw_track_profile_dialog.h \
    layer_trw_point_properties.h \
    layer_trw_trackpoint_properties.h \
    layer_trw_waypoint.h \
    layer_trw_waypoint_properties.h \
    tree_item.h \
    tree_view.h \
    tree_item_list.h \
    tree_view_internal.h \
    ui_builder.h \
    widget_color_button.h \
    widget_file_list.h \
    widget_file_entry.h \
    widget_radio_group.h \
    widget_list_selection.h \
    widget_slider.h \
    widget_timestamp.h \
    widget_utm_entry.h \
    widget_lat_lon_entry.h \
    widget_measurement_entry.h \
    widget_coord_display.h \
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
    variant.h \
    graph_intervals.h \
    expedia.h




# vikmaptype.cpp
# vikmapslayer_compat.cpp





# Pre-compile-time tests.
# http://doc.qt.io/qt-5/qmake-test-function-reference.html#qtcompiletest-test
load(configure)

qtCompileTest(libmagic) {
    DEFINES += HAVE_MAGIC_H
    QMAKE_LFLAGS += -lmagic
}

qtCompileTest(libmapnik) {
    DEFINES += HAVE_LIBMAPNIK
    QMAKE_LFLAGS += -lmapnik
}

qtCompileTest(libbz) {
    DEFINES += HAVE_BZLIB_H
    QMAKE_LFLAGS += -lbz2
}




# For some reason the X11 package name must be lower case
packagesExist(x11) {
    DEFINES += HAVE_X11_XLIB_H
    QMAKE_LFLAGS += -lX11
    message("libX11 found")
} else {
    message("libX11 not found")
}

packagesExist(sqlite3) {
    DEFINES += HAVE_SQLITE3_H
    QMAKE_LFLAGS += -lsqlite3
    message("libsqlite3 found")
} else {
    message("libsqlite3 not found")
}

packagesExist(libcurl) {
    DEFINES += HAVE_LIBCURL
    QMAKE_LFLAGS += -lcurl
    message("libcurl found")
} else {
    error("libcurl not found")
}

packagesExist(libgps) {
    DEFINES += HAVE_LIBGPS
    QMAKE_LFLAGS += -lgps
    message("libgps found")
} else {
    message("libgps not found")
}

packagesExist(expat) {
    DEFINES += HAVE_EXPAT_H
    QMAKE_LFLAGS += -lexpat
    message("libexpat found")
} else {
    message("libexpat not found")
}

packagesExist(zlib) {
    DEFINES += HAVE_LIBZ
    QMAKE_LFLAGS += -lz
    message("libz found")
} else {
    message("libz not found")
}

packagesExist(icu-uc) {
    DEFINES += HAVE_LIBICUUC
    QMAKE_LFLAGS += -licuuc
    message("libicu-uc found")
} else {
    error("libicu-uc library not found")
}






# For glib library.
CONFIG += link_pkgconfig debug
PKGCONFIG += glib-2.0 zlib gio-2.0

# Put *.o files in the same location as corresponding *.cpp files.
# Useful when moc_*.cpp files are put into separate dir.
# https://wiki.qt.io/Undocumented_QMake#Config_features
CONFIG += object_parallel_to_source




DEFINES += HAVE_CONFIG_H
DEFINES += VIK_CONFIG_GEOCACHES
DEFINES += VIK_CONFIG_BING
DEFINES += VIK_CONFIG_GOOGLE
DEFINES += VIK_CONFIG_EXPEDIA
DEFINES += VIK_CONFIG_TERRASERVER
DEFINES += VIK_CONFIG_GEONAMES
DEFINES += VIK_CONFIG_GEOTAG
# DEFINES += VIK_CONFIG_GEOCACHES
# DEFINES += VIK_CONFIG_DEM24K
DEFINES += VIK_CONFIG_REALTIME_GPS_TRACKING

DEFINES += "PROJECT=\"\\\"SlavGPS\\\"\""
DEFINES += "PACKAGE=\"\\\"slavgps\\\"\""
DEFINES += "PACKAGE_NAME=\"\\\"slavgps\\\"\""
DEFINES += "PACKAGE_STRING=\"\\\"slavgps 1.0.0\\\"\""
DEFINES += "PACKAGE_VERSION=\"\\\"1.0.0\\\"\""
DEFINES += "PACKAGE_URL=\"\\\"https://to_be_done\\\"\""
DEFINES += "CURRENT_YEAR=\"\\\"2018\\\"\""

# DEFINES += "HAVE_LIBGEXIV2=0"
# DEFINES += "HAVE_LIBEXIF=0"

# Define to 1 if you have the <zip.h> header file.
# DEFINES += HAVE_ZIP_H=0

# TODO: add tests that set these flags
DEFINES += HAVE_UTIME_H
DEFINES += "HAVE_SYS_TYPES_H=1"
DEFINES += "HAVE_SYS_STAT_H=1"
DEFINES += "HAVE_UNISTD_H=1"

# Size of the map cache
DEFINES += "VIK_CONFIG_MAPCACHE_SIZE=128"

# Age of tiles before checking it (in seconds)
DEFINES += "VIK_CONFIG_DEFAULT_TILE_AGE=604800"

# Define to 1 if lstat dereferences a symlink specified with a trailing slash
DEFINES += "LSTAT_FOLLOWS_SLASHED_SYMLINK=1"




# Put moc_*.cpp files in a subdirectory.
MOC_DIR = ./moc/
UI_DIR = ./
OBJECTS_DIR = ./




QMAKE_CXXFLAGS += -std=c++11 -Wno-unused -Wshadow -Wall -Wextra -pedantic -g -O0
QMAKE_LFLAGS += -lm

# TODO: this lib needs to be added only if lib is present on build machine. Write test for the flag.
QMAKE_LFLAGS += -lexiv2
