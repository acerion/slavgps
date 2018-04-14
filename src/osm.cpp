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

#include "variant.h"
#include "osm.h"
#include "map_ids.h"
#include "layer_map.h"
#include "map_source_slippy.h"
#include "map_source_wmsc.h"
#include "webtool_center.h"
#include "webtool_bounds.h"
#include "webtool_format.h"
#include "webtool_datasource.h"
#include "external_tools.h"
#include "external_tool_datasources.h"
#include "goto_tool_xml.h"
#include "goto.h"
#include "util.h"
#ifdef K_INCLUDES
#include "routing.h"
#include "routing_engine_web.h"
#endif



using namespace SlavGPS;



/* initialisation */
void SlavGPS::osm_init(void)
{
	MapSource * mapnik_type = new MapSourceSlippy(MapTypeID::OSM_MAPNIK, "OpenStreetMap (Mapnik)", "tile.openstreetmap.org", "/%1/%2/%3.png");
	mapnik_type->set_map_type_string("OSM-Mapnik"); /* Non-translatable. */
	mapnik_type->dl_options.check_file_server_time = false;
	mapnik_type->dl_options.use_etag = true;
	mapnik_type->zoom_min = 0;
	mapnik_type->zoom_max = 19;
	mapnik_type->set_copyright("© OpenStreetMap contributors");
	mapnik_type->set_license("CC-BY-SA");
	mapnik_type->set_license_url("http://www.openstreetmap.org/copyright");

	MapSource * cycle_type = new MapSourceSlippy(MapTypeID::OSM_CYCLE, "OpenStreetMap (Cycle)", "tile.opencyclemap.org","/cycle/%1/%2/%3.png");
	cycle_type->set_map_type_string("OSM-Cycle"); /* Non-translatable. */
	cycle_type->dl_options.check_file_server_time = true;
	cycle_type->dl_options.use_etag = false;
	cycle_type->zoom_min = 0;
	cycle_type->zoom_max = 18;
	cycle_type->set_copyright("Tiles courtesy of Andy Allan © OpenStreetMap contributors");
	cycle_type->set_license("CC-BY-SA");
	cycle_type->set_license_url("http://www.openstreetmap.org/copyright");

	MapSource * transport_type = new MapSourceSlippy(MapTypeID::OSM_TRANSPORT, "OpenStreetMap (Transport)", "tile2.opencyclemap.org", "/transport/%1/%2/%3.png");
	transport_type->set_map_type_string("OSM-Transport"); /* Non-translatable. */
	transport_type->dl_options.check_file_server_time = true;
	transport_type->dl_options.use_etag = false;
	transport_type->zoom_min = 0;
	transport_type->zoom_max = 18;
	transport_type->set_copyright("Tiles courtesy of Andy Allan © OpenStreetMap contributors");
	transport_type->set_license("CC-BY-SA");
	transport_type->set_license_url("http://www.openstreetmap.org/copyright");

	MapSource * mapquest_type = new MapSourceSlippy(MapTypeID::MAPQUEST_OSM, "OpenStreetMap (MapQuest)", "otile1.mqcdn.com", "/tiles/1.0.0/osm/%1/%2/%3.png");
	mapquest_type->set_map_type_string("OSM-MapQuest"); /* Non-translatable. */
	mapquest_type->dl_options.check_file_server_time = true;
	mapquest_type->dl_options.use_etag = false;
	mapquest_type->zoom_min = 0;
	mapquest_type->zoom_max = 19;
	mapquest_type->set_copyright("Tiles Courtesy of MapQuest © OpenStreetMap contributors");
	mapquest_type->set_license("MapQuest Specific");
	mapquest_type->set_license_url("http://developer.mapquest.com/web/info/terms-of-use");

	MapSource * hot_type = new MapSourceSlippy(MapTypeID::OSM_HUMANITARIAN, "OpenStreetMap (Humanitarian)", "c.tile.openstreetmap.fr", "/hot/%1/%2/%3.png");
	hot_type->set_map_type_string("OSM-Humanitarian"); /* Non-translatable. */
	hot_type->dl_options.check_file_server_time = true;
	hot_type->dl_options.use_etag = false;
	hot_type->zoom_min = 0;
	hot_type->zoom_max = 20; // Super detail!!
	hot_type->set_copyright("© OpenStreetMap contributors. Tiles courtesy of Humanitarian OpenStreetMap Team");
	hot_type->set_license("CC-BY-SA");
	hot_type->set_license_url("http://www.openstreetmap.org/copyright");

	/* No cache needed for this type. */
	MapSource * direct_type = new MapSourceSlippy(MapTypeID::OSM_ON_DISK, QObject::tr("On Disk OSM Tile Format"), NULL, NULL);
	/* For using your own generated data assumed you know the license already! */
	direct_type->set_copyright("© OpenStreetMap contributors"); // probably
	direct_type->is_direct_file_access_flag = true;

	/* No cache needed for this type. */
	MapSource * mbtiles_type = new MapSourceSlippy(MapTypeID::MBTILES, QObject::tr("MBTiles File"), NULL, NULL);
	/* For using your own generated data assumed you know the license already! */
	mbtiles_type->set_copyright("© OpenStreetMap contributors"); // probably
	mbtiles_type->is_direct_file_access_flag = true;
	mbtiles_type->is_mbtiles_flag = true;


	/* No cache needed for this type. */
	MapSource * metatiles_type = new MapSourceSlippy(MapTypeID::OSM_METATILES, QObject::tr("OSM Metatiles"), NULL, NULL);
	/* For using your own generated data assumed you know the license already! */
	metatiles_type->set_copyright("© OpenStreetMap contributors"); // probably
	metatiles_type->is_direct_file_access_flag = true;
	metatiles_type->is_osm_meta_tiles_flag = true;

	MapSources::register_map_source(mapquest_type);
	MapSources::register_map_source(mapnik_type);
	MapSources::register_map_source(cycle_type);
	MapSources::register_map_source(transport_type);
	MapSources::register_map_source(hot_type);
	MapSources::register_map_source(direct_type);
	MapSources::register_map_source(mbtiles_type);
	MapSources::register_map_source(metatiles_type);

	/* Webtools. */
	ExternalTools::register_tool(new WebToolCenter(QObject::tr("OSM (view)"), "http://www.openstreetmap.org/?lat=%1&lon=%2&zoom=%3"));

	ExternalTools::register_tool(new WebToolCenter(QObject::tr("OSM (edit)"), "http://www.openstreetmap.org/edit?lat=%1&lon=%2&zoom=%3"));

#ifdef K_TODO /* Correctly handle %d arguments in the string. */
	/* Note the use of positional parameters. */
	ExternalTools::register_tool(new WebToolCenter(QObject::tr("OSM (query)"), "http://www.openstreetmap.org/query?lat=%1$s&lon=%2$s#map=%3$d/%1$s/%2$s"));
#endif

	ExternalTools::register_tool(new WebToolCenter(QObject::tr("OSM (render)"), "http://www.informationfreeway.org/?lat=%1&lon=%2&zoom=%3&layers=B0000F000F"));

	/* Example: http://127.0.0.1:8111/load_and_zoom?left=8.19&right=8.20&top=48.605&bottom=48.590&select=node413602999
	   JOSM or merkaartor must already be running with remote interface enabled. */
	ExternalTools::register_tool(new WebToolBounds(QObject::tr("Local port 8111 (eg JOSM)"), "http://localhost:8111/load_and_zoom?left=%1&right=%2&bottom=%3&top=%4"));

	ExternalTools::register_tool(new WebToolFormat(QObject::tr("Geofabrik Map Compare"), "http://tools.geofabrik.de/mc/#%1/%2/%3", "ZAO"));



	/* Datasource. */
	ExternalToolDataSource::register_tool(new WebToolDatasource(QObject::tr("OpenStreetMap Notes"), "http://api.openstreetmap.org/api/0.6/notes.gpx?bbox=%1,%2,%3,%4&amp;closed=0", "LBRT", NULL, NULL, NULL));




	/* Goto */

	GotoToolXML * nominatim = new GotoToolXML(QObject::tr("OSM Nominatim"),
						  "http://nominatim.openstreetmap.org/search?q=%1&format=xml",
						  "searchresults/place",
						  "lat",
						  "searchresults/place",
						  "lon");
	GoTo::register_tool(nominatim);
	//g_object_unref ( nominatim );

	GotoToolXML * namefinder = new GotoToolXML(QObject::tr("OSM Name finder"),
						   "http://gazetteer.openstreetmap.org/namefinder/search.xml?find=%1&max=1",
						   "/searchresults/named",
						   "lat",
						   "/searchresults/named",
						   "lon");
	GoTo::register_tool(namefinder);
	//g_object_unref ( namefinder );


	/* Not really OSM but can't be bothered to create somewhere else to put it... */
	ExternalTools::register_tool(new WebToolCenter(QObject::tr("Wikimedia Toolserver GeoHack"), "http://tools.wmflabs.org/geohack/geohack.php?params=%1;%2"));

#ifdef K_TODO
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
	routing_register(VIK_ROUTING_ENGINE (osrm));
	g_object_unref(osrm);
#endif
}
