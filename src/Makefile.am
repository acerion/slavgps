# This is the Makefile for the Viking GPS viewer program.
# This file is licensed through the GPL version 2 or newer.

SUBDIRS = icons

bin_PROGRAMS = viking

noinst_LIBRARIES = libviking.a

authors.h: $(top_srcdir)/AUTHORS
	rm -f $@
	echo "/* Generated file. */" >> $@
	echo "const char *AUTHORS[] = {\\" >> $@
	sed -e "s/ \*\*.*//" -e 's/^/"/' -e 's/$$/",\\/' -e '$$s/\\$$//' $< >> $@
	echo "NULL};" >> $@

if HAVE_XSLTPROC
documenters.h: $(top_srcdir)/help/C/viking.xml
	$(XP) $(srcdir)/docbook2documenters.xsl $(top_srcdir)/help/C/viking.xml > $@
else
documenters.h:
	rm -f $@
	echo "/* Generated file. */" >> $@
	echo "const char *DOCUMENTERS[] = {\\" >> $@
	echo "NULL};" >> $@
endif

BUILT_SOURCES = authors.h documenters.h

vikenumtypes.h: vikenumtypes.h.template $(ENUM_H_FILES) $(GLIB_MKENUMS)
	(cd $(srcdir) && $(GLIB_MKENUMS) --template vikenumtypes.h.template $(ENUM_H_FILES)) > $@

vikenumtypes.cpp: vikenumtypes.cpp.template $(ENUM_H_FILES) $(GLIB_MKENUMS)
	(cd $(srcdir) && $(GLIB_MKENUMS) --template vikenumtypes.cpp.template $(ENUM_H_FILES)) > $@

EXTRA_DIST = \
	docbook2documenters.xsl \
	vikenumtypes.h.template \
	vikenumtypes.cpp.template

BUILT_SOURCES += vikenumtypes.h vikenumtypes.cpp

$(BUILT_SOURCES): $(srcdir)/Makefile.am

ENUM_H_FILES = \
	vikviewport.h

libviking_a_SOURCES = \
	bbox.h \
	map_ids.h \
	modules.h modules.cpp \
	curl_download.cpp curl_download.h \
	compression.cpp compression.h \
	menu.xml.h \
	degrees_converters.cpp degrees_converters.h \
	globals.cpp globals.h \
	viking.h mapcoord.h config.h \
	vik_compat.cpp vik_compat.h \
	viktrack.cpp viktrack.h \
	track_statistics.cpp track_statistics.h \
	vikwaypoint.cpp vikwaypoint.h \
	clipboard.cpp clipboard.h \
	coords.cpp coords.h \
	gpsmapper.cpp gpsmapper.h \
	gpspoint.cpp gpspoint.h \
	geojson.cpp geojson.h \
	dir.cpp dir.h \
	file.cpp file.h \
	fileutils.cpp fileutils.h \
	authors.h \
	documenters.h \
	dialog.cpp dialog.h \
	util.cpp util.h \
	ui_util.cpp ui_util.h \
	download.cpp download.h \
	jpg.cpp jpg.h \
	vikenumtypes.cpp vikenumtypes.h \
	viktreeview.cpp viktreeview.h \
	viktrwlayer.cpp viktrwlayer.h \
	layer_trw.cpp layer_trw.h \
	layer_trw_draw.cpp layer_trw_draw.h \
	layer_trw_containers.cpp layer_trw_containers.h \
	viklayer.cpp viklayer.h \
	viklayerspanel.cpp viklayerspanel.h \
	vikcoordlayer.cpp vikcoordlayer.h \
	vikstatus.cpp vikstatus.h \
	window.cpp window.h \
	window_layer_tools.cpp window_layer_tools.h \
	vikviewport.cpp vikviewport.h \
	vikaggregatelayer.cpp vikaggregatelayer.h \
	vikgobjectbuilder.cpp vikgobjectbuilder.h \
	vikgpslayer.cpp vikgpslayer.h \
	vikgeoreflayer.cpp vikgeoreflayer.h \
	vikfileentry.cpp vikfileentry.h \
	vikgototool.cpp vikgototool.h \
	vikgotoxmltool.cpp vikgotoxmltool.h \
	vikgoto.cpp vikgoto.h \
	viktrwlayer_export.cpp viktrwlayer_export.h \
	viktrwlayer_tpwin.cpp viktrwlayer_tpwin.h \
	viktrwlayer_wpwin.cpp viktrwlayer_wpwin.h \
	viktrwlayer_propwin.cpp viktrwlayer_propwin.h \
	viktrwlayer_analysis.cpp viktrwlayer_analysis.h \
	viktrwlayer_tracklist.cpp viktrwlayer_tracklist.h \
	viktrwlayer_waypointlist.cpp viktrwlayer_waypointlist.h \
	viktrwlayer_ui.cpp \
	vikrouting.cpp vikrouting.h \
	vikroutingengine.cpp vikroutingengine.h \
	vikroutingwebengine.cpp vikroutingwebengine.h \
	vikutils.cpp vikutils.h \
	toolbar.cpp toolbar.h toolbar.xml.h \
	thumbnails.cpp thumbnails.h \
	background.cpp background.h \
	vikradiogroup.cpp vikradiogroup.h \
	vikcoord.cpp vikcoord.h \
	mapcache.cpp mapcache.h \
	maputils.cpp maputils.h \
	vikmapsource.cpp vikmapsource.h \
	vikmapslayer.cpp vikmapslayer.h \
	vikmapslayer_compat.cpp vikmapslayer_compat.h \
	vikmaptype.cpp vikmaptype.h \
	vikslippymapsource.cpp vikslippymapsource.h \
	vikwmscmapsource.cpp vikwmscmapsource.h \
	viktmsmapsource.cpp viktmsmapsource.h \
	metatile.cpp metatile.h \
	gpx.cpp gpx.h \
	garminsymbols.cpp garminsymbols.h \
	acquire.cpp acquire.h \
	babel.cpp babel.h \
	babel_ui.cpp babel_ui.h \
	datasource_file.cpp \
	datasource_geojson.cpp \
	datasource_gps.cpp datasource_gps.h \
	datasource_routing.cpp \
	datasource_gc.cpp \
	datasource_bfilter.cpp \
	datasource_wikipedia.cpp \
	datasource_url.cpp \
	datasources.h \
	googlesearch.cpp googlesearch.h \
	dem.cpp dem.h \
	vikdemlayer.h vikdemlayer.cpp \
	vikdatetime_edit_dialog.cpp vikdatetime_edit_dialog.h \
	vikfilelist.cpp vikfilelist.h \
	vikexttool.cpp vikexttool.h \
	vikexttools.cpp vikexttools.h \
	vikwebtool.cpp vikwebtool.h \
	vikwebtoolcenter.cpp vikwebtoolcenter.h \
	vikwebtoolbounds.cpp vikwebtoolbounds.h \
	vikwebtoolformat.cpp vikwebtoolformat.h \
	vikexttool_datasources.cpp vikexttool_datasources.h \
	vikwebtool_datasource.cpp vikwebtool_datasource.h \
	dems.cpp dems.h \
	srtm_continent.cpp \
	uibuilder.cpp uibuilder.h \
	print-preview.cpp print-preview.h \
	print.cpp print.h \
	kmz.cpp kmz.h \
	viklayer_defaults.cpp viklayer_defaults.h \
	settings.cpp settings.h \
	preferences.cpp preferences.h \
	misc/strtod.cpp misc/strtod.h \
	misc/kdtree.cpp misc/kdtree.h

if BING
libviking_a_SOURCES += \
	bingmapsource.cpp bingmapsource.h \
	bing.cpp bing.h
endif

if GOOGLE
libviking_a_SOURCES += \
	google.cpp google.h
endif

if TERRASERVER
libviking_a_SOURCES += \
	terraservermapsource.cpp terraservermapsource.h \
	terraserver.cpp terraserver.h
endif

if EXPEDIA
libviking_a_SOURCES += \
	expedia.cpp expedia.h
endif

if OPENSTREETMAP
libviking_a_SOURCES += \
	osm.cpp osm.h \
	osm-traces.cpp osm-traces.h \
	datasource_osm_my_traces.cpp \
	datasource_osm.cpp
endif

if BLUEMARBLE
libviking_a_SOURCES += \
	bluemarble.cpp bluemarble.h
endif

if GEONAMES
libviking_a_SOURCES += \
	geonames.cpp geonames.h \
	geonamessearch.cpp geonamessearch.h
endif

if GEOTAG
libviking_a_SOURCES += \
	datasource_geotag.cpp \
	geotag_exif.cpp geotag_exif.h \
	viktrwlayer_geotag.cpp viktrwlayer_geotag.h
endif

# libexif doesn't have write support of EXIF info - so reused this code from command line exif tool
if LIBEXIF
libviking_a_SOURCES += \
	libjpeg/jpeg-data.c libjpeg/jpeg-data.h \
	libjpeg/jpeg-marker.c libjpeg/jpeg-marker.h
endif

if MAPNIK
libviking_a_SOURCES += \
	vikmapniklayer.cpp vikmapniklayer.h \
	mapnik_interface.cpp mapnik_interface.h
endif

viking_SOURCES = main.cpp

LDADD           = libviking.a $(PACKAGE_LIBS) @EXPAT_LIBS@ @LIBCURL@ icons/libicons.a
if WINDOWS
LDADD += \
	../win32/installer/pixmaps/viking_icon.o
endif

AM_CFLAGS		= -Wall -g -D_GNU_SOURCE \
	$(PACKAGE_CFLAGS) \
	@EXPAT_CFLAGS@ @LIBCURL_CPPFLAGS@
AM_CPPFLAGS		= \
	$(PACKAGE_CFLAGS) \
	-DVIKING_DATADIR=\""$(pkgdatadir)"\" \
	-DVIKING_SYSCONFDIR=\""$(sysconfdir)/$(PACKAGE)"\"

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = viking.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
EXTRA_DIST += $(desktop_in_files)

CLEANFILES = $(BUILT_SOURCES) $(desktop_DATA)
