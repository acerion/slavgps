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
 */




#include <cstdlib>




#include "variant.h"
#include "osm.h"
#include "layer_map.h"
#include "layer_map_source_slippy.h"
#include "layer_map_source_mbtiles.h"
#include "layer_map_source_wmsc.h"
#include "map_cache.h"
#include "webtool_center.h"
#include "webtool_bounds.h"
#include "webtool_format.h"
#include "webtool_query.h"
#include "datasource_webtool.h"
#include "external_tools.h"
#include "external_tool_datasources.h"
#include "goto_tool_xml.h"
#include "goto.h"
#include "util.h"
#include "routing.h"
#include "routing_engine_web.h"
#include "osm_metatile.h"
#include "map_utils.h"




/*
  OSM definition is a TMS derivative, (Global Mercator profile with Flipped Y)
  http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
  http://wiki.openstreetmap.org/wiki/TMS
  http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification
*/




using namespace SlavGPS;




#define SG_MODULE "OSM"




static MapSource * map_source_maker_mapnik(void);
static MapSource * map_source_maker_cycle(void);
static MapSource * map_source_maker_transport(void);
static MapSource * map_source_maker_mapquest(void);
static MapSource * map_source_maker_humanitarian(void);
static MapSource * map_source_maker_osm_on_disk(void);
static MapSource * map_source_maker_mb_tiles(void);
static MapSource * map_source_maker_osm_meta_tiles(void);




/* Module initialisation. */
void OSM::init(void)
{
	MapSources::register_map_source_maker(map_source_maker_mapnik, MapTypeID::OSMMapnik, "OpenStreetMap (Mapnik)");
	MapSources::register_map_source_maker(map_source_maker_cycle, MapTypeID::OSMCycle, "OpenStreetMap (Cycle)");
	MapSources::register_map_source_maker(map_source_maker_transport, MapTypeID::OSMTransport, "OpenStreetMap (Transport)");
	MapSources::register_map_source_maker(map_source_maker_mapquest, MapTypeID::MapQuestOSM, "OpenStreetMap (MapQuest)");
	MapSources::register_map_source_maker(map_source_maker_humanitarian, MapTypeID::OSMHumanitarian, "OpenStreetMap (Humanitarian)");
	MapSources::register_map_source_maker(map_source_maker_osm_on_disk, MapTypeID::OSMOnDisk, QObject::tr("On Disk OSM Tile Format"));
	MapSources::register_map_source_maker(map_source_maker_mb_tiles, MapTypeID::MBTiles, QObject::tr("MBTiles File"));
	MapSources::register_map_source_maker(map_source_maker_osm_meta_tiles, MapTypeID::OSMMetatiles, QObject::tr("OSM Metatiles"));


	/* Online Services (formerly Webtools). */
	ExternalTools::register_tool(new OnlineService_center(QObject::tr("OSM (view)"), "http://www.openstreetmap.org/?lat=%1&lon=%2&zoom=%3"));

	ExternalTools::register_tool(new OnlineService_center(QObject::tr("OSM (edit)"), "http://www.openstreetmap.org/edit?lat=%1&lon=%2&zoom=%3"));

#ifdef TODO_LATER /* Correctly handle %d arguments in the string. */
	/* Note the use of positional parameters. */
	ExternalTools::register_tool(new OnlineService_center(QObject::tr("OSM (query)"), "http://www.openstreetmap.org/query?lat=%1$s&lon=%2$s#map=%3$d/%1$s/%2$s"));
#endif

	ExternalTools::register_tool(new OnlineService_center(QObject::tr("OSM (render)"), "http://www.informationfreeway.org/?lat=%1&lon=%2&zoom=%3&layers=B0000F000F"));

	/* Example: http://127.0.0.1:8111/load_and_zoom?left=8.19&right=8.20&top=48.605&bottom=48.590&select=node413602999
	   JOSM or merkaartor must already be running with remote interface enabled. */
	ExternalTools::register_tool(new OnlineService_bbox(QObject::tr("Local port 8111 (eg JOSM)"), "http://localhost:8111/load_and_zoom?left=%1&right=%2&bottom=%3&top=%4"));

	ExternalTools::register_tool(new OnlineService_format(QObject::tr("Geofabrik Map Compare"), "http://tools.geofabrik.de/mc/#%1/%2/%3", "ZAO"));

	/* Not really OSM but can't be bothered to create somewhere else to put it... */
	ExternalTools::register_tool(new OnlineService_center(QObject::tr("Wikimedia Toolserver GeoHack"), "http://tools.wmflabs.org/geohack/geohack.php?params=%1;%2"));


	/* Datasource. */
	ExternalToolDataSource::register_tool(new OnlineService_query(QObject::tr("OpenStreetMap Notes"), "http://api.openstreetmap.org/api/0.6/notes.gpx?bbox=%1,%2,%3,%4&amp;closed=0", "LBRT", "", ""));


	/* Goto */
	GoTo::register_tool(new GotoToolXML(QObject::tr("OSM Nominatim"), "http://nominatim.openstreetmap.org/search?q=%1&format=xml", "searchresults/place", "lat", "searchresults/place", "lon"));

	GoTo::register_tool(new GotoToolXML(QObject::tr("OSM Name finder"), "http://gazetteer.openstreetmap.org/namefinder/search.xml?find=%1&max=1", "/searchresults/named", "lat", "/searchresults/named", "lon"));


	/*
	  See API references:
	   https://github.com/DennisOSRM/Project-OSRM/wiki/Server-api
	   https://github.com/Project-OSRM/osrm-backend/wiki/Server-api/d3df08ef7fc4dbe4d1960bc6df92f441e1343b82#server-api-4x
	*/
	RoutingEngineWeb * osrm = new RoutingEngineWeb("osrm", "OSRM v4", "gpx");
	/* TODO_LATER: review and improve these assignments and format specifiers. */
	osrm->url_base = "http://router.project-osrm.org/viaroute?output=gpx";
	osrm->url_start_ll_fmt = "&loc=%s,%s";
	osrm->url_stop_ll_fmt = "&loc=%s,%s";
	osrm->url_via_ll_fmt = "&loc=%s,%s";
	Routing::register_engine(osrm);

#if 1
	/*
	  Test routing engine used to verify that Routing code
	  correctly replaces one entry with another using the same
	  "id". This can be used to overwrite hardcoded engine with
	  newer definition coming from config file.

	  Notice that this engine is using the same "id" as the engine
	  defined above, but is using different name. It is loaded
	  later, so it should overwrite (replace) the previous engine.
	*/
	RoutingEngineWeb * osrm_newer = new RoutingEngineWeb("osrm", "OSRM v21", "gpx");
	osrm_newer->url_base = "http://router.project-osrm.org/viaroute?output=gpx";
	osrm_newer->url_start_ll_fmt = "&loc=%s,%s";
	osrm_newer->url_stop_ll_fmt = "&loc=%s,%s";
	osrm_newer->url_via_ll_fmt = "&loc=%s,%s";
	Routing::register_engine(osrm_newer);
#endif

#if 1
	/*
	  Test routing engine used to verify that Routing code
	  correctly handles more than one engine type.
	*/
	RoutingEngineWeb * kamil_engine = new RoutingEngineWeb("kre", "KRE v1", "gpx");
	kamil_engine->url_base = "http://router.project-osrm.org/viaroute?output=gpx";
	kamil_engine->url_start_ll_fmt = "&loc=%s,%s";
	kamil_engine->url_stop_ll_fmt = "&loc=%s,%s";
	kamil_engine->url_via_ll_fmt = "&loc=%s,%s";
	Routing::register_engine(kamil_engine);
#endif
}




/* No cache needed for this type. */
MapSourceOSMMetatiles::MapSourceOSMMetatiles() : MapSourceSlippy(MapTypeID::OSMMetatiles, QObject::tr("OSM Metatiles"), NULL, NULL)
{
	/* For using your own generated data assumed you know the license already! */
	this->set_copyright("© OpenStreetMap contributors"); // probably
	this->is_direct_file_access_flag = true;
	this->is_osm_meta_tiles_flag = true;
}




QPixmap MapSourceOSMMetatiles::create_tile_pixmap(const MapCacheObj & map_cache_obj, const TileInfo & tile_info) const
{
	QString err_msg;
	QPixmap pixmap;

	Metatile metatile(map_cache_obj.dir_full_path, tile_info);

	if (0 != metatile.read_metatile(err_msg)) {
		qDebug() << SG_PREFIX_E << "Failed to read metatile file:" << err_msg;
		return pixmap;
	}

	if (metatile.is_compressed) {
		/* TODO_MAYBE: Not handled yet - I don't think this is used often - so implement later if necessary. */
		qDebug() << SG_PREFIX_E << "Handling of compressed metatile not implemented";
		return pixmap;
	}

	/* Convert bytes stored in Metatile::buffer into a pixmap. */
	if (!pixmap.loadFromData(metatile.buffer, metatile.read_bytes)) {
		qDebug() << SG_PREFIX_E << "Failed to load pixmap from metatile";
		return pixmap;
	} else {
		qDebug() << SG_PREFIX_I << "Creating pixmap from metatile:" << (pixmap.isNull() ? "failure" : "success");
	}

	return pixmap;
}




QStringList MapSourceOSMMetatiles::get_tile_description(const MapCacheObj & map_cache_obj, const TileInfo & tile_info) const
{
	QStringList items;
	Metatile metatile(map_cache_obj.dir_full_path, tile_info);

	items.push_back(metatile.file_full_path);
	items.push_back(this->full_path); /* TODO_LATER: this->full_path is not set anywhere. */

	tile_info_add_file_info_strings(items, this->full_path);

	return items;
}




/* No cache needed for this type. */
MapSourceOSMOnDisk::MapSourceOSMOnDisk() : MapSourceSlippy(MapTypeID::OSMOnDisk, QObject::tr("On Disk OSM Tile Format"), NULL, NULL)
{
	/* TODO_LATER: This license string is invalid for
	   user-generated and user-owned tiles. */
	this->set_copyright("© OpenStreetMap contributors");
	this->is_direct_file_access_flag = true;

}




QPixmap MapSourceOSMOnDisk::create_tile_pixmap(const MapCacheObj & map_cache_obj, const TileInfo & tile_info) const
{
	if (MapCacheLayout::OSM != map_cache_obj.layout) {
		qDebug() << SG_PREFIX_W << "Layout mismatch:" << (int) MapCacheLayout::OSM << (int) map_cache_obj.layout;
	}

	const MapCacheObj new_map_cache_obj(MapCacheLayout::OSM, map_cache_obj.dir_full_path); /* TODO_LATER: why do we need to create the copy with explicit layout? */
	const QString tile_file_full_path = new_map_cache_obj.get_cache_file_full_path(tile_info,
										       this->m_map_type_id,
										       "", /* In other map sources it would be this->get_map_type_string(), but not for this map source. */
										       this->get_file_extension());
	QPixmap pixmap = this->load_tile_pixmap_from_file(tile_file_full_path);

	qDebug() << SG_PREFIX_I << "Creating pixmap from file:" << (pixmap.isNull() ? "failure" : "success");

	return pixmap;
}




QStringList MapSourceOSMOnDisk::get_tile_description(const MapCacheObj & map_cache_obj, const TileInfo & tile_info) const
{
	QStringList items;

	const MapCacheObj new_map_cache_obj(MapCacheLayout::OSM, map_cache_obj.dir_full_path); /* TODO_LATER: why do we need to create the copy with explicit layout? */

	const QString tile_file_full_path = new_map_cache_obj.get_cache_file_full_path(tile_info, /* ulm */
										       this->m_map_type_id,
										       "", /* In other map sources it would be this->get_map_type_string(), but not for this map source. */
										       this->get_file_extension());
	const QString source = QObject::tr("Source: file://%1").arg(tile_file_full_path);
	items.push_back(source);

	tile_info_add_file_info_strings(items, tile_file_full_path);

	return items;
}




MapSource * map_source_maker_mapnik(void)
{
	MapSource * map_source = new MapSourceSlippy(MapTypeID::OSMMapnik, QObject::tr("OpenStreetMap (Mapnik)"), "tile.openstreetmap.org", "/%1/%2/%3.png");
	map_source->set_map_type_string("OSM-Mapnik"); /* Non-translatable. */
	map_source->dl_options.check_file_server_time = false;
	map_source->dl_options.use_etag = true;
	map_source->set_supported_tile_zoom_level_range(0, 19);
	map_source->set_copyright("© OpenStreetMap contributors");
	map_source->set_license("CC-BY-SA");
	map_source->set_license_url("http://www.openstreetmap.org/copyright");
	return map_source;
}




MapSource * map_source_maker_cycle(void)
{
	MapSource * map_source = new MapSourceSlippy(MapTypeID::OSMCycle, QObject::tr("OpenStreetMap (Cycle)"), "tile.opencyclemap.org","/cycle/%1/%2/%3.png");
	map_source->set_map_type_string("OSM-Cycle"); /* Non-translatable. */
	map_source->dl_options.check_file_server_time = true;
	map_source->dl_options.use_etag = false;
	map_source->set_supported_tile_zoom_level_range(0, 18);
	map_source->set_copyright("Tiles courtesy of Andy Allan © OpenStreetMap contributors");
	map_source->set_license("CC-BY-SA");
	map_source->set_license_url("http://www.openstreetmap.org/copyright");
	return map_source;
}




MapSource * map_source_maker_transport(void)
{
	MapSource * map_source = new MapSourceSlippy(MapTypeID::OSMTransport, QObject::tr("OpenStreetMap (Transport)"), "tile2.opencyclemap.org", "/transport/%1/%2/%3.png");
	map_source->set_map_type_string("OSM-Transport"); /* Non-translatable. */
	map_source->dl_options.check_file_server_time = true;
	map_source->dl_options.use_etag = false;
	map_source->set_supported_tile_zoom_level_range(0, 18);
	map_source->set_copyright("Tiles courtesy of Andy Allan © OpenStreetMap contributors");
	map_source->set_license("CC-BY-SA");
	map_source->set_license_url("http://www.openstreetmap.org/copyright");
	return map_source;
}




MapSource * map_source_maker_mapquest(void)
{
	MapSource * map_source = new MapSourceSlippy(MapTypeID::MapQuestOSM, QObject::tr("OpenStreetMap (MapQuest)"), "otile1.mqcdn.com", "/tiles/1.0.0/osm/%1/%2/%3.png");
	map_source->set_map_type_string("OSM-MapQuest"); /* Non-translatable. */
	map_source->dl_options.check_file_server_time = true;
	map_source->dl_options.use_etag = false;
	map_source->set_supported_tile_zoom_level_range(0, 19);
	map_source->set_copyright("Tiles Courtesy of MapQuest © OpenStreetMap contributors");
	map_source->set_license("MapQuest Specific");
	map_source->set_license_url("http://developer.mapquest.com/web/info/terms-of-use");
	return map_source;
}




MapSource * map_source_maker_humanitarian(void)
{
	MapSource * map_source = new MapSourceSlippy(MapTypeID::OSMHumanitarian, QObject::tr("OpenStreetMap (Humanitarian)"), "c.tile.openstreetmap.fr", "/hot/%1/%2/%3.png");
	map_source->set_map_type_string("OSM-Humanitarian"); /* Non-translatable. */
	map_source->dl_options.check_file_server_time = true;
	map_source->dl_options.use_etag = false;
	map_source->set_supported_tile_zoom_level_range(0, 20); /* Super detail! */
	map_source->set_copyright("© OpenStreetMap contributors. Tiles courtesy of Humanitarian OpenStreetMap Team");
	map_source->set_license("CC-BY-SA");
	map_source->set_license_url("http://www.openstreetmap.org/copyright");
	return map_source;
}




MapSource * map_source_maker_osm_on_disk(void)
{
	return new MapSourceOSMOnDisk();
}




MapSource * map_source_maker_mb_tiles(void)
{
	return new MapSourceMBTiles();
}




MapSource * map_source_maker_osm_meta_tiles(void)
{
	return new MapSourceOSMMetatiles();
}
