/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2006-2012, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdlib>

#ifdef K

#include <glib.h>
#include <glib/gstdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#endif

#include "modules.h"

#include "geonames.h"
#include "layer_map.h"
#include "osm.h"
#include "osm-traces.h"
#ifdef K
#include "bing.h"
#include "google.h"
#include "terraserver.h"
#include "expedia.h"

#include "bluemarble.h"
#include "dir.h"
#include "datasources.h"
#include "external_tools.h"
#include "vikexttool_datasources.h"
#include "goto.h"
#include "routing.h"

/* Loadable types */
#include "map_source_slippy.h"
#include "map_source_tms.h"
#include "map_source_wmsc.h"
#include "vikwebtoolcenter.h"
#include "vikwebtoolbounds.h"
#include "goto_tool_xml.h"
#include "vikwebtool_datasource.h"
#include "routing_engine_web.h"

#include "vikgobjectbuilder.h"
#endif

#ifdef HAVE_LIBMAPNIK
#include "layer_mapnik.h"
#endif

#include "layer_gps.h"
#include "layer_trw.h"
#include "viewport.h"
#include "window.h"




using namespace SlavGPS;




#define VIKING_MAPS_FILE "maps.xml"
#define VIKING_EXTTOOLS_FILE "external_tools.xml"
#define VIKING_DATASOURCES_FILE "datasources.xml"
#define VIKING_GOTOTOOLS_FILE "goto_tools.xml"
#define VIKING_ROUTING_FILE "routing.xml"



#ifdef K

static void modules_register_map_source(VikGobjectBuilder * self, MapSource * mapsource)
{
	fprintf(stderr, "DEBUG: %s\n", __FUNCTION__);
	/* FIXME label should be hosted by object. */
	maps_layer_register_map_source(mapsource);
}




static void modules_register_exttools(VikGobjectBuilder * self, GObject * object)
{
	/* kamilFIXME: I think that this no longer works. */
	fprintf(stderr, "DEBUG: %s\n", __FUNCTION__);
	ExternalTool * ext_tool = (ExternalTool *) object;
	vik_ext_tools_register(ext_tool);
}




static void SlavGPS::modules_register_datasources(VikGobjectBuilder * self, GObject * object)
{
	/* kamilFIXME: I think that this no longer works. */
	fprintf(stderr, "DEBUG: %s\n", __FUNCTION__);
	ExternalTool * ext_tool = (ExternalTool *) object;
	vik_ext_tool_datasources_register(ext_tool);
}




static void modules_register_gototools(VikGobjectBuilder * self, GObject * object)
{
	fprintf(stderr, "DEBUG: %s\n", __FUNCTION__);
	GotoTool * goto_tool = (GotoTool *) object;
	vik_goto_register(goto_tool);
}




static void modules_register_routing_engine(VikGobjectBuilder * self, GObject * object)
{
	fprintf(stderr, "DEBUG: %s\n", __FUNCTION__);
	RoutingEngine *engine = VIK_ROUTING_ENGINE (object);
	vik_routing_register(engine);
}




static void modules_load_config_dir(const char * dir)
{
	fprintf(stderr, "DEBUG: Loading configurations from directory %s\n", dir);

	/* Maps sources. */
	char * maps = g_build_filename(dir, VIKING_MAPS_FILE, NULL);
	if (access(maps, R_OK) == 0) {
		VikGobjectBuilder *builder = vik_gobject_builder_new();
		QObject::connect(builder, SIGNAL("new-object"), NULL, SLOT (modules_register_map_source));
		vik_gobject_builder_parse(builder, maps);
		g_object_unref(builder);
	}
	free(maps);

	/* External tools. */
	char * tools = g_build_filename(dir, VIKING_EXTTOOLS_FILE, NULL);
	if (access(tools, R_OK) == 0) {
		VikGobjectBuilder *builder = vik_gobject_builder_new();
		QObject::connect(builder, SIGNAL("new-object"), NULL, SLOT (modules_register_exttools));
		vik_gobject_builder_parse(builder, tools);
		g_object_unref(builder);
	}
	free(tools);

	char * datasources = g_build_filename(dir, VIKING_DATASOURCES_FILE, NULL);
	if (access(datasources, R_OK) == 0) {
		VikGobjectBuilder *builder = vik_gobject_builder_new();
		QObject::connect(builder, SIGNAL("new-object"), NULL, SLOT (modules_register_datasources));
		vik_gobject_builder_parse(builder, datasources);
		g_object_unref(builder);
	}
	free(datasources);

	/* Go-to search engines. */
	char * go_to = g_build_filename(dir, VIKING_GOTOTOOLS_FILE, NULL);
	if (access(go_to, R_OK) == 0) {
		VikGobjectBuilder * builder = vik_gobject_builder_new();
		QObject::connect(builder, SIGNAL("new-object"), NULL, SLOT (modules_register_gototools));
		vik_gobject_builder_parse(builder, go_to);
		g_object_unref(builder);
	}
	free(go_to);

	/* Routing engines. */
	char * routing = g_build_filename(dir, VIKING_ROUTING_FILE, NULL);
	if (access(routing, R_OK) == 0) {
		VikGobjectBuilder *builder = vik_gobject_builder_new();
		QObject::connect(builder, SIGNAL("new-object"), NULL, SLOT (modules_register_routing_engine));
		vik_gobject_builder_parse(builder, routing);
		g_object_unref(builder);
	}
	free(routing);
}




static void modules_load_config(void)
{
	/* Look in the directories of data path. */
	char ** data_dirs = get_viking_data_path();
	/* Priority is standard one:
	   left element is more important than right one.
	   But our logic is to load all existing files and overwrite
	   overlapping config with last recent one.
	   So, we have to process directories in reverse order. */
	int nb_data_dirs = g_strv_length(data_dirs);
	for (; nb_data_dirs > 0 ; nb_data_dirs--) {
		modules_load_config_dir(data_dirs[nb_data_dirs-1]);
	}
	g_strfreev(data_dirs);

	/* Check if system config is set. */
	modules_load_config_dir(VIKING_SYSCONFDIR);

	const char * data_home = get_viking_data_home();
	if (data_home) {
		modules_load_config_dir(data_home);
	}

	/* Check user's home config. */
	modules_load_config_dir(get_viking_dir());
}




static void register_loadable_types(void)
{
	/* Force registering of loadable types. */
	volatile GType types[] = {
		/* Maps */
		//VIK_TYPE_SLIPPY_MAP_SOURCE,
		//VIK_TYPE_TMS_MAP_SOURCE,
		//VIK_TYPE_WMSC_MAP_SOURCE,

		/* Goto */
		//VIK_GOTO_XML_TOOL_TYPE,

		/* Tools */
		//VIK_WEBTOOL_CENTER_TYPE,
		//VIK_WEBTOOL_BOUNDS_TYPE,

		/* Datasource */
		//VIK_WEBTOOL_DATASOURCE_TYPE,

		/* Routing */
		//VIK_ROUTING_WEB_ENGINE_TYPE
	};

	/* Kill 'unused variable' + argument type warnings. */
	fprintf(stderr, "DEBUG: %d types loaded\n", (int)sizeof(types)/(int)sizeof(GType));
}


#endif


/**
 * First stage initialization.
 * Can not use a_get_preferences() yet...
 * See comment in main.c.
 */
void SlavGPS::modules_init()
{
	/* OSM done first so this will be the default service for searching/routing/etc... */
#ifdef VIK_CONFIG_OPENSTREETMAP
	osm_init();
	osm_traces_init();
#endif

#ifdef K
#ifdef VIK_CONFIG_BING
	bing_init();
#endif
#ifdef VIK_CONFIG_GOOGLE
	google_init();
#endif
#ifdef VIK_CONFIG_EXPEDIA
	expedia_init();
#endif
#ifdef VIK_CONFIG_TERRASERVER
	terraserver_init();
#endif
#ifdef VIK_CONFIG_BLUEMARBLE
	bluemarble_init();
#endif
#endif
#if 1 //#ifdef VIK_CONFIG_GEONAMES
	geonames_init();
#endif
#ifdef K
#ifdef VIK_CONFIG_GEOCACHES
	a_datasource_gc_init();
#endif

#ifdef HAVE_LIBMAPNIK
	vik_mapnik_layer_init();
#endif

	register_loadable_types();

	/* As modules are loaded, we can load configuration files. */
	modules_load_config();
#endif
}




/**
 * Secondary stage initialization.
 * Can now use a_get_preferences() and a_babel_available().
 */
void SlavGPS::modules_post_init()
{
#ifdef K
#ifdef VIK_CONFIG_GOOGLE
	google_post_init();
#endif
#ifdef HAVE_LIBMAPNIK
	vik_mapnik_layer_post_init();
#endif
#endif
	layer_init();
	layer_trw_init();
#ifdef K
	layer_gps_init();
	layer_mapnik_init();
#endif
	viewport_init();
#ifdef K
	window_init();
#endif
}




void SlavGPS::modules_uninit()
{
#ifdef VIK_CONFIG_OPENSTREETMAP
	osm_traces_uninit();
#endif

#ifdef K
#ifdef HAVE_LIBMAPNIK
	vik_mapnik_layer_uninit();
#endif
#endif
}
