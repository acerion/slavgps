# This is the Makefile for the Viking GPS viewer program.
# This file is licensed through the GPL version 2 or newer.

SUBDIRS = icons

bin_PROGRAMS = viking

noinst_LIBRARIES = libviking.a

authors.h: $(top_srcdir)/AUTHORS
	rm -f $@
	echo "/* Generated file. */" >> $@
	echo "const gchar *AUTHORS[] = {\\" >> $@
	sed -e "s/ \*\*.*//" -e 's/^/"/' -e 's/$$/",\\/' -e '$$s/\\$$//' $< >> $@
	echo "NULL};" >> $@

if HAVE_XSLTPROC
documenters.h: $(top_srcdir)/help/C/viking.xml
	$(XP) $(srcdir)/docbook2documenters.xsl $(top_srcdir)/help/C/viking.xml > $@
else
documenters.h:
	rm -f $@
	echo "/* Generated file. */" >> $@
	echo "const gchar *DOCUMENTERS[] = {\\" >> $@
	echo "NULL};" >> $@
endif

BUILT_SOURCES = authors.h documenters.h

vikenumtypes.h: vikenumtypes.h.template $(ENUM_H_FILES) $(GLIB_MKENUMS)
	(cd $(srcdir) && $(GLIB_MKENUMS) --template vikenumtypes.h.template $(ENUM_H_FILES)) > $@

vikenumtypes.c: vikenumtypes.c.template $(ENUM_H_FILES) $(GLIB_MKENUMS)
	(cd $(srcdir) && $(GLIB_MKENUMS) --template vikenumtypes.c.template $(ENUM_H_FILES)) > $@

EXTRA_DIST = \
	docbook2documenters.xsl \
	vikenumtypes.h.template \
	vikenumtypes.c.template

BUILT_SOURCES += vikenumtypes.h vikenumtypes.c

$(BUILT_SOURCES): $(srcdir)/Makefile.am

ENUM_H_FILES = \
	vikviewport.h

libviking_a_SOURCES = \
	bbox.h \
	map_ids.h \
	modules.h modules.c \
	curl_download.c curl_download.h \
	compression.c compression.h \
	menu.xml.h \
	degrees_converters.c degrees_converters.h \
	globals.c globals.h \
	viking.h mapcoord.h config.h \
	vik_compat.c vik_compat.h \
	viktrack.cpp viktrack.h \
	vikwaypoint.cpp vikwaypoint.h \
	clipboard.c clipboard.h \
	coords.cpp coords.h \
	gpsmapper.c gpsmapper.h \
	gpspoint.c gpspoint.h \
	geojson.c geojson.h \
	dir.c dir.h \
	file.c file.h \
	fileutils.c fileutils.h \
	authors.h \
	documenters.h \
	dialog.c dialog.h \
	util.c util.h \
	ui_util.c ui_util.h \
	download.c download.h \
	jpg.c jpg.h \
	vikenumtypes.c vikenumtypes.h \
	viktreeview.c viktreeview.h \
	viktrwlayer.cpp viktrwlayer.h \
	viklayer.c viklayer.h \
	viklayerspanel.c viklayerspanel.h \
	vikcoordlayer.cpp vikcoordlayer.h \
	vikstatus.c vikstatus.h \
	vikwindow.c vikwindow.h \
	vikviewport.c vikviewport.h \
	vikaggregatelayer.c vikaggregatelayer.h \
	vikgobjectbuilder.c vikgobjectbuilder.h \
	vikgpslayer.c vikgpslayer.h \
	vikgeoreflayer.c vikgeoreflayer.h \
	vikfileentry.c vikfileentry.h \
	vikgototool.c vikgototool.h \
	vikgotoxmltool.c vikgotoxmltool.h \
	vikgoto.c vikgoto.h \
	viktrwlayer_export.c viktrwlayer_export.h \
	viktrwlayer_tpwin.c viktrwlayer_tpwin.h \
	viktrwlayer_wpwin.c viktrwlayer_wpwin.h \
	viktrwlayer_propwin.c viktrwlayer_propwin.h \
	viktrwlayer_analysis.c viktrwlayer_analysis.h \
	viktrwlayer_tracklist.c viktrwlayer_tracklist.h \
	viktrwlayer_waypointlist.c viktrwlayer_waypointlist.h \
	vikrouting.c vikrouting.h \
	vikroutingengine.c vikroutingengine.h \
	vikroutingwebengine.c vikroutingwebengine.h \
	vikutils.c vikutils.h \
	toolbar.c toolbar.h toolbar.xml.h \
	thumbnails.c thumbnails.h \
	background.c background.h \
	vikradiogroup.c vikradiogroup.h \
	vikcoord.cpp vikcoord.h \
	mapcache.c mapcache.h \
	maputils.c maputils.h \
	vikmapsource.cpp vikmapsource.h \
	vikmapsourcedefault.cpp vikmapsourcedefault.h \
	vikmapslayer.c vikmapslayer.h \
	vikmapslayer_compat.c vikmapslayer_compat.h \
	vikmaptype.cpp vikmaptype.h \
	vikslippymapsource.c vikslippymapsource.h \
	vikwmscmapsource.c vikwmscmapsource.h \
	viktmsmapsource.c viktmsmapsource.h \
	metatile.c metatile.h \
	gpx.c gpx.h \
	garminsymbols.c garminsymbols.h \
	acquire.cpp acquire.h \
	babel.cpp babel.h \
	babel_ui.c babel_ui.h \
	datasource_file.cpp \
	datasource_geojson.cpp \
	datasource_gps.cpp datasource_gps.h \
	datasource_routing.cpp \
	datasource_gc.cpp \
	datasource_bfilter.cpp \
	datasource_wikipedia.cpp \
	datasource_url.cpp \
	datasources.h \
	googlesearch.c googlesearch.h \
	dem.c dem.h \
	vikdemlayer.h vikdemlayer.c \
	vikdatetime_edit_dialog.c vikdatetime_edit_dialog.h \
	vikfilelist.c vikfilelist.h \
	vikexttool.c vikexttool.h \
	vikexttools.c vikexttools.h \
	vikwebtool.c vikwebtool.h \
	vikwebtoolcenter.c vikwebtoolcenter.h \
	vikwebtoolbounds.c vikwebtoolbounds.h \
	vikwebtoolformat.c vikwebtoolformat.h \
	vikexttool_datasources.c vikexttool_datasources.h \
	vikwebtool_datasource.c vikwebtool_datasource.h \
	dems.c dems.h \
	srtm_continent.c \
	uibuilder.c uibuilder.h \
	print-preview.c print-preview.h \
	print.c print.h \
	kmz.c kmz.h \
	viklayer_defaults.c viklayer_defaults.h \
	settings.c settings.h \
	preferences.c preferences.h \
	misc/strtod.c misc/strtod.h \
	misc/kdtree.c misc/kdtree.h

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
	terraservermapsource.c terraservermapsource.h \
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

viking_SOURCES = main.c

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
