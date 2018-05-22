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


#ifdef K_INCLUDES
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

#ifdef K_INCLUDES
#include "bing.h"
#include "google.h"
#include "terraserver.h"
#include "expedia.h"
#include "bluemarble.h"
#include "dir.h"
#include "datasources.h"
#include "external_tools.h"
#include "external_tool_datasources.h"
#include "goto.h"
#include "routing.h"

/* Loadable types */
#include "map_source_slippy.h"
#include "map_source_tms.h"
#include "map_source_wmsc.h"
#include "webtool_center.h"
#include "webtool_bounds.h"
#include "webtool_datasource.h"
#include "goto_tool_xml.h"
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



#ifdef K_FIXME_RESTORE

static void modules_register_map_source(VikGobjectBuilder * self, MapSource * map_source)
{
	fprintf(stderr, "DEBUG: %s\n", __FUNCTION__);
	/* FIXME label should be hosted by object. */
	MapSources::register_map_source(map_source);
}




static void modules_register_exttools(VikGobjectBuilder * self, void * object)
{
	/* kamilFIXME: I think that this no longer works. */
	fprintf(stderr, "DEBUG: %s\n", __FUNCTION__);
	ExternalTools::register_tool((ExternalTool *) object);
}




static void SlavGPS::modules_register_datasources(VikGobjectBuilder * self, void * object)
{
	/* kamilFIXME: I think that this no longer works. */
	fprintf(stderr, "DEBUG: %s\n", __FUNCTION__);
	ExternalToolDataSource::register_tool((ExternalTool *) object);
}




static void modules_register_gototools(VikGobjectBuilder * self, void * object)
{
	fprintf(stderr, "DEBUG: %s\n", __FUNCTION__);
	GotoTool * goto_tool = (GotoTool *) object;
	GoTo::register_tool(goto_tool);
}




static void modules_register_routing_engine(VikGobjectBuilder * self, void * object)
{
	fprintf(stderr, "DEBUG: %s\n", __FUNCTION__);
	RoutingEngine *engine = VIK_ROUTING_ENGINE (object);
	Routing::register_engine(engine);
}




static void modules_load_config_dir(const QString & dir)
{
	qDebug() << "DD: Modules: Loading configurations from directory" << dir;

	/* Maps sources. */
	const QString maps = dir + QDir::separator() + VIKING_MAPS_FILE;
	if (access(maps.toUtf8().constData(), R_OK) == 0) {
		VikGobjectBuilder *builder = vik_gobject_builder_new();
		QObject::connect(builder, SIGNAL("new-object"), NULL, SLOT (modules_register_map_source));
		vik_gobject_builder_parse(builder, maps);
		g_object_unref(builder);
	}

	/* External tools. */
	const QString tools = dir + QDir::separator() + VIKING_EXTTOOLS_FILE;
	if (access(tools.toUtf8().constData(), R_OK) == 0) {
		VikGobjectBuilder *builder = vik_gobject_builder_new();
		QObject::connect(builder, SIGNAL("new-object"), NULL, SLOT (modules_register_exttools));
		vik_gobject_builder_parse(builder, tools);
		g_object_unref(builder);
	}

	const QString datasources = dir + QDir::separator() + VIKING_DATASOURCES_FILE;
	if (access(datasources.toUtf8().constData(), R_OK) == 0) {
		VikGobjectBuilder *builder = vik_gobject_builder_new();
		QObject::connect(builder, SIGNAL("new-object"), NULL, SLOT (modules_register_datasources));
		vik_gobject_builder_parse(builder, datasources);
		g_object_unref(builder);
	}

	/* Go-to search engines. */
	const QString go_to = dir + QDir::separator() + VIKING_GOTOTOOLS_FILE;
	if (access(go_to.toUtf8().constData(), R_OK) == 0) {
		VikGobjectBuilder * builder = vik_gobject_builder_new();
		QObject::connect(builder, SIGNAL("new-object"), NULL, SLOT (modules_register_gototools));
		vik_gobject_builder_parse(builder, go_to);
		g_object_unref(builder);
	}

	/* Routing engines. */
	const QString routing = dir + QDir::separator() + VIKING_ROUTING_FILE;
	if (access(routing.toUtf8().constData(), R_OK) == 0) {
		VikGobjectBuilder *builder = vik_gobject_builder_new();
		QObject::connect(builder, SIGNAL("new-object"), NULL, SLOT (modules_register_routing_engine));
		vik_gobject_builder_parse(builder, routing);
		g_object_unref(builder);
	}
}




static void modules_load_config(void)
{
	/* Look in the directories of data path. */
	const QString data_dirs = SlavGPSLocations::get_data_dirs();

	/* Priority is standard one:
	   left element is more important than right one.
	   But our logic is to load all existing files and overwrite
	   overlapping config with last recent one.
	   So, we have to process directories in reverse order. */
	const int n_data_dirs = data_dirs.size();
	for (int i = n_data_dirs - 1; i >= 0; i--) {
		modules_load_config_dir(data_dirs.at(i));
	}

	/* Check if system config is set. */
	modules_load_config_dir(VIKING_SYSCONFDIR);

	QString data_home = SlavGPSLocations::get_data_home();
	if (!data_home.isEmpty()) {
		modules_load_config_dir(data_home);
	}

	/* Check user's home config. */
	modules_load_config_dir(get_viking_dir());
}



typedef int GType;
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

#ifdef K_FIXME_RESTORE
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

#ifdef VIK_CONFIG_GEOCACHES
	DataSourceGeoCache::init();
#endif

#ifdef K_FIXME_RESTORE
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
 * Can now use a_get_preferences() and Babel::is_available().
 */
void SlavGPS::modules_post_init()
{
#ifdef K_FIXME_RESTORE
#ifdef VIK_CONFIG_GOOGLE
	google_post_init();
#endif
#ifdef HAVE_LIBMAPNIK
	vik_mapnik_layer_post_init();
#endif
#endif
	layer_init();
	layer_trw_init();

	layer_gps_init();
#ifdef K_FIXME_RESTORE
	layer_mapnik_init();
#endif
	viewport_init();
}




void SlavGPS::modules_uninit()
{
#ifdef VIK_CONFIG_OPENSTREETMAP
	osm_traces_uninit();
#endif

#ifdef K_FIXME_RESTORE
#ifdef HAVE_LIBMAPNIK
	vik_mapnik_layer_uninit();
#endif
#endif
}
