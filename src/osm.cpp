/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2007,2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (c) 2012-2014, Rob Norris <rw_norris@hotmail.com>
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

#include "osm.h"
#include "map_ids.h"
#include "layer_map.h"
#include "map_source_slippy.h"
#include "map_source_wmsc.h"
#include "vikwebtoolcenter.h"
#include "vikwebtoolbounds.h"
#include "vikwebtoolformat.h"
#include "vikwebtool_datasource.h"
#include "vikexttools.h"
#include "vikexttool_datasources.h"
#include "goto_tool_xml.h"
#include "goto.h"
#include "util.h"
#ifdef K
#include "routing.h"
#include "routing_engine_web.h"
#endif



using namespace SlavGPS;



/* initialisation */
void SlavGPS::osm_init(void)
{
	MapSource * mapnik_type = new MapSourceSlippy(MAP_ID_OSM_MAPNIK, "OpenStreetMap (Mapnik)", "tile.openstreetmap.org", "/%d/%d/%d.png");
	mapnik_type->set_name((char *) "OSM-Mapnik");
	mapnik_type->dl_options.check_file_server_time = false;
	mapnik_type->dl_options.use_etag = true;
	mapnik_type->zoom_min = 0;
	mapnik_type->zoom_max = 19;
	mapnik_type->set_copyright((char *) "© OpenStreetMap contributors");
	mapnik_type->set_license((char *) "CC-BY-SA");
	mapnik_type->set_license_url((char *) "http://www.openstreetmap.org/copyright");


	MapSource * cycle_type = new MapSourceSlippy(MAP_ID_OSM_CYCLE, "OpenStreetMap (Cycle)", "tile.opencyclemap.org","/cycle/%d/%d/%d.png");
	cycle_type->set_name((char *) "OSM-Cycle");
	cycle_type->dl_options.check_file_server_time = true;
	cycle_type->dl_options.use_etag = false;
	cycle_type->zoom_min = 0;
	cycle_type->zoom_max = 18;
	cycle_type->set_copyright((char *) "Tiles courtesy of Andy Allan © OpenStreetMap contributors");
	cycle_type->set_license((char *) "CC-BY-SA");
	cycle_type->set_license_url((char *) "http://www.openstreetmap.org/copyright");

	MapSource * transport_type = new MapSourceSlippy(MAP_ID_OSM_TRANSPORT, "OpenStreetMap (Transport)", "tile2.opencyclemap.org", "/transport/%d/%d/%d.png");
	transport_type->set_name((char *) "OSM-Transport");
	transport_type->dl_options.check_file_server_time = true;
	transport_type->dl_options.use_etag = false;
	transport_type->zoom_min = 0;
	transport_type->zoom_max = 18;
	transport_type->set_copyright((char *) "Tiles courtesy of Andy Allan © OpenStreetMap contributors");
	transport_type->set_license((char *) "CC-BY-SA");
	transport_type->set_license_url((char *) "http://www.openstreetmap.org/copyright");

	MapSource * mapquest_type = new MapSourceSlippy(MAP_ID_MAPQUEST_OSM, "OpenStreetMap (MapQuest)", "otile1.mqcdn.com", "/tiles/1.0.0/osm/%d/%d/%d.png");
	mapquest_type->set_name((char *) "OSM-MapQuest");
	mapquest_type->dl_options.check_file_server_time = true;
	mapquest_type->dl_options.use_etag = false;
	mapquest_type->zoom_min = 0;
	mapquest_type->zoom_max = 19;
	mapquest_type->set_copyright((char *) "Tiles Courtesy of MapQuest © OpenStreetMap contributors");
	mapquest_type->set_license((char *) "MapQuest Specific");
	mapquest_type->set_license_url((char *) "http://developer.mapquest.com/web/info/terms-of-use");


	MapSource * hot_type = new MapSourceSlippy(MAP_ID_OSM_HUMANITARIAN, "OpenStreetMap (Humanitarian)", "c.tile.openstreetmap.fr", "/hot/%d/%d/%d.png");
	hot_type->set_name((char *) "OSM-Humanitarian");
	hot_type->dl_options.check_file_server_time = true;
	hot_type->dl_options.use_etag = false;
	hot_type->zoom_min = 0;
	hot_type->zoom_max = 20; // Super detail!!
	hot_type->set_copyright((char *) "© OpenStreetMap contributors. Tiles courtesy of Humanitarian OpenStreetMap Team");
	hot_type->set_license((char *) "CC-BY-SA");
	hot_type->set_license_url((char *) "http://www.openstreetmap.org/copyright");

	// NB no cache needed for this type!!
	MapSource * direct_type = new MapSourceSlippy(MAP_ID_OSM_ON_DISK,_("On Disk OSM Tile Format"), NULL, NULL);
	// For using your own generated data assumed you know the license already!
	direct_type->set_copyright((char *) "© OpenStreetMap contributors"); // probably
	direct_type->is_direct_file_access_flag = true;

	// NB no cache needed for this type!!
	MapSource * mbtiles_type = new MapSourceSlippy(MAP_ID_MBTILES, _("MBTiles File"), NULL, NULL);
	// For using your own generated data assumed you know the license already!
	mbtiles_type->set_copyright((char *) "© OpenStreetMap contributors"); // probably
	mbtiles_type->is_direct_file_access_flag = true;
	mbtiles_type->is_mbtiles_flag = true;


	// NB no cache needed for this type!!
	MapSource * metatiles_type = new MapSourceSlippy(MAP_ID_OSM_METATILES, _("OSM Metatiles"), NULL, NULL);
	// For using your own generated data assumed you know the license already!
	metatiles_type->set_copyright((char *) "© OpenStreetMap contributors"); // probably
	metatiles_type->is_direct_file_access_flag = true;
	metatiles_type->is_osm_meta_tiles_flag = true;

	maps_layer_register_map_source(mapquest_type);
	maps_layer_register_map_source(mapnik_type);
	maps_layer_register_map_source(cycle_type);
	maps_layer_register_map_source(transport_type);
	maps_layer_register_map_source(hot_type);
	maps_layer_register_map_source(direct_type);
	maps_layer_register_map_source(mbtiles_type);
	maps_layer_register_map_source(metatiles_type);
#ifdef K
	// Webtools
	WebToolCenter * web_tool = new WebToolCenter(_("OSM (view)"), "http://www.openstreetmap.org/?lat=%s&lon=%s&zoom=%d");
	vik_ext_tools_register(web_tool);
	//g_object_unref ( webtool );

	web_tool = new WebToolCenter(_("OSM (edit)"), "http://www.openstreetmap.org/edit?lat=%s&lon=%s&zoom=%d");
	vik_ext_tools_register(web_tool);
	//g_object_unref ( webtool );

	// Note the use of positional parameters
	web_tool = new WebToolCenter(_("OSM (query)"), "http://www.openstreetmap.org/query?lat=%1$s&lon=%2$s#map=%3$d/%1$s/%2$s");
	vik_ext_tools_register(web_tool);
	//g_object_unref ( webtool );

	web_tool = new WebToolCenter(_("OSM (render)"), "http://www.informationfreeway.org/?lat=%s&lon=%s&zoom=%d&layers=B0000F000F");
	vik_ext_tools_register(web_tool);
	//g_object_unref ( webtool );

	// Example: http://127.0.0.1:8111/load_and_zoom?left=8.19&right=8.20&top=48.605&bottom=48.590&select=node413602999
	// JOSM or merkaartor must already be running with remote interface enabled
	WebToolBounds * web_tool_bounds = new WebToolBounds(_("Local port 8111 (eg JOSM)"), "http://localhost:8111/load_and_zoom?left=%s&right=%s&bottom=%s&top=%s");
	vik_ext_tools_register(web_tool_bounds);
	//g_object_unref ( webtoolbounds );

	WebToolFormat * web_tool_format = new WebToolFormat(_("Geofabrik Map Compare"), "http://tools.geofabrik.de/mc/#%s/%s/%s", "ZAO");
	vik_ext_tools_register(web_tool_format);
	//g_object_unref ( vwtf );

	// Datasource
	WebToolDatasource * web_tool_datasource = new WebToolDatasource(_("OpenStreetMap Notes"), "http://api.openstreetmap.org/api/0.6/notes.gpx?bbox=%s,%s,%s,%s&amp;closed=0", "LBRT", NULL, NULL, NULL);
	vik_ext_tool_datasources_register(web_tool_datasource);
	//g_object_unref ( vwtds );
#endif



	/* Goto */

	GotoToolXML * nominatim = new GotoToolXML("OSM Nominatim",
						  "http://nominatim.openstreetmap.org/search?q=%s&format=xml",
						  "/searchresults/place",
						  "lat",
						  "/searchresults/place",
						  "lon");
	vik_goto_register(nominatim);
	//g_object_unref ( nominatim );

	GotoToolXML * namefinder = new GotoToolXML("OSM Name finder",
						   "http://gazetteer.openstreetmap.org/namefinder/search.xml?find=%s&max=1",
						   "/searchresults/named",
						   "lat",
						   "/searchresults/named",
						   "lon");
	vik_goto_register(namefinder);
	//g_object_unref ( namefinder );



#ifdef K
	// Not really OSM but can't be bothered to create somewhere else to put it...
	web_tool = new WebToolCenter(_("Wikimedia Toolserver GeoHack"), "http://tools.wmflabs.org/geohack/geohack.php?params=%s;%s");
	vik_ext_tools_register(web_tool);
	//g_object_unref ( webtool );

	/* See API references: https://github.com/DennisOSRM/Project-OSRM/wiki/Server-api */
	RoutingEngine * osrm = (RoutingEngine *) g_object_new(VIK_ROUTING_WEB_ENGINE_TYPE,
								    "id", "osrm",
								    "label", "OSRM",
								    "format", "gpx",
								    "url-base", "http://router.project-osrm.org/viaroute?output=gpx",
								    "url-start-ll", "&loc=%s,%s",
								    "url-stop-ll", "&loc=%s,%s",
								    "url-via-ll", "&loc=%s,%s",
								    NULL);
	vik_routing_register(VIK_ROUTING_ENGINE (osrm));
	g_object_unref(osrm);
#endif
}
