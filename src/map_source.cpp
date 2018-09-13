/*
 * viking
 * Copyright (C) 2009-2010, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *
 * viking is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * viking is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdlib>
#include <cstring>
#include <unistd.h>




#include <QDebug>




#include "window.h"
#include "viewport.h"
#include "coord.h"
#include "mapcoord.h"
#include "download.h"
#include "map_source.h"
#include "map_cache.h"
#include "layer_map.h"
#include "statusbar.h"




using namespace SlavGPS;




#define SG_MODULE "Map Source"




extern Tree * g_tree;




MapSource::MapSource()
{
	fprintf(stderr, "MapSource regular constructor called\n");

	this->map_type_string = QObject::tr("Unknown"); /* I usually write that it's non-translatable, but this is exception. TODO_LATER: maybe this is a sign of bad design. */
	this->map_type_id = MapTypeID::Initial;
	this->label = "<no-set>";

	tilesize_x = 256;
	tilesize_y = 256;

	drawmode = ViewportDrawMode::MERCATOR; /* ViewportDrawMode::UTM */
	this->file_extension = ".png";

	this->dl_options.check_file = a_check_map_file;

	is_direct_file_access_flag = false; /* Use direct file access to OSM like tile images - no need for a webservice. */
	is_osm_meta_tiles_flag = false; /* Read from OSM Meta Tiles - Should be 'use-direct-file-access' as well. */

	switch_xy = false; /* Switch the order of x,y components in the URL (such as used by ARCGIS Tile Server. */
}




MapSource::~MapSource()
{
	qDebug() << SG_PREFIX_I << "Destructor called";
}




MapSource & MapSource::operator=(const MapSource & other)
{
	qDebug() << SG_PREFIX_I << "Copy assignment called";

	if (&other == this) {
		/* Self-assignment. */
		return *this;
	}

	this->copyright   = other.copyright;
	this->license     = other.license;
	this->license_url = other.license_url;

	this->logo.logo_pixmap = other.logo.logo_pixmap;
	this->logo.logo_id     = other.logo.logo_id;

	this->map_type_string = other.map_type_string;
	this->map_type_id     = other.map_type_id;
	this->label           = other.label;

	this->tilesize_x = other.tilesize_x;
	this->tilesize_y = other.tilesize_y;

	this->drawmode   = other.drawmode;
	this->file_extension = other.file_extension;

	memcpy(&this->dl_options, &other.dl_options, sizeof (DownloadOptions));

	this->server_hostname    = other.server_hostname;
	this->server_path_format = other.server_path_format;

	this->zoom_min = other.zoom_min;
	this->zoom_max = other.zoom_max;

	this->lat_min = other.lat_min;
	this->lat_max = other.lat_max;
	this->lon_min = other.lon_min;
	this->lon_max = other.lon_max;

	this->is_direct_file_access_flag = other.is_direct_file_access_flag;
	this->is_osm_meta_tiles_flag = other.is_osm_meta_tiles_flag;

	this->switch_xy = other.switch_xy;

        return *this;
}




MapSource::MapSource(MapSource & map)
{
	qDebug() << SG_PREFIX_I << "Copy constructor called";

	this->copyright   = map.copyright;
	this->license     = map.license;
	this->license_url = map.license_url;

	this->logo.logo_pixmap = map.logo.logo_pixmap;
	this->logo.logo_id     = map.logo.logo_id;

	this->map_type_string = map.map_type_string;
	this->map_type_id     = map.map_type_id;
	this->label           = map.label;

	this->tilesize_x = map.tilesize_x;
	this->tilesize_y = map.tilesize_y;

	this->drawmode   = map.drawmode;
	this->file_extension = map.file_extension;

	memcpy(&this->dl_options, &map.dl_options, sizeof (DownloadOptions));

	this->server_hostname    = map.server_hostname;
	this->server_path_format = map.server_path_format;

	this->zoom_min = map.zoom_min;
	this->zoom_max = map.zoom_max;

	this->lat_min = map.lat_min;
	this->lat_max = map.lat_max;
	this->lon_min = map.lon_min;
	this->lon_max = map.lon_max;

	this->is_direct_file_access_flag = map.is_direct_file_access_flag;
	this->is_osm_meta_tiles_flag = map.is_osm_meta_tiles_flag;

	this->switch_xy = map.switch_xy;
}




void MapSource::set_map_type_string(const QString & new_map_type_string)
{
	this->map_type_string = new_map_type_string;

	/* Sanitize the name here for file usage.
	   A simple check just to prevent names containing slashes. */
	this->map_type_string.replace('\\', 'x');
	this->map_type_string.replace('/', 'x');
}




bool MapSource::set_map_type_id(MapTypeID new_map_type_id)
{
	if (!MapSource::is_map_type_id_registered(new_map_type_id)) {
		qDebug() << SG_PREFIX_E << "Unknown map type" << (int) new_map_type_id;
		return false;
	}

	this->map_type_id = new_map_type_id;
	return true;
}




void MapSource::set_label(const QString & new_label)
{
	this->label = new_label;
}




void MapSource::set_tilesize_x(uint16_t tilesize_x_)
{
	tilesize_x = tilesize_x_;
}



void MapSource::set_tilesize_y(uint16_t tilesize_y_)
{
	tilesize_y = tilesize_y_;
}




void MapSource::set_drawmode(ViewportDrawMode new_drawmode)
{
	this->drawmode = new_drawmode;
}




void MapSource::set_copyright(const QString & new_copyright)
{
	this->copyright = new_copyright;
}




void MapSource::set_license(const QString & new_license)
{
	this->license = new_license;
}




void MapSource::set_license_url(const QString & new_license_url)
{
	this->license_url = new_license_url;
}




void MapSource::set_file_extension(const QString & new_file_extension)
{
	this->file_extension = new_file_extension;
}




/**
   Add copyright strings from map strings to viewport for the
   corresponding bounding box and zoom level.

   @bbox: bounding box of interest
   @zoom: the zoom level of interest
*/
void MapSource::add_copyright(Viewport * viewport, const LatLonBBox &  bbox, double zoom)
{
	return;
}




QString MapSource::get_license(void) const
{
	return this->license;
}




QString MapSource::get_license_url(void) const
{
	return this->license_url;
}




const ViewportLogo & MapSource::get_logo(void) const
{
	return this->logo;
}




QString MapSource::get_map_type_string(void) const
{
	return this->map_type_string;
}




MapTypeID MapSource::get_map_type_id(void) const
{
	qDebug() << SG_PREFIX_D << "Returning map type" << (int) this->map_type_id << "for map" << this->label;
	return this->map_type_id;
}




QString MapSource::get_label(void) const
{
	return this->label;
}




uint16_t MapSource::get_tilesize_x(void) const
{
	return tilesize_x;
}




uint16_t MapSource::get_tilesize_y(void) const
{
	return tilesize_y;
}




ViewportDrawMode MapSource::get_drawmode(void) const
{
	return this->drawmode;
}




/**
 * @self: the MapSource of interest.
 *
 * Return true when we can bypass all this download malarky.
 * Treat the files as a pre generated data set in OSM tile server layout: tiledir/%d/%d/%d.png
 */
bool MapSource::is_direct_file_access(void) const
{
	return this->is_direct_file_access_flag;
}




/**
 * @self: the MapSource of interest.
 *
 * Treat the files as a pre generated data set directly by tirex or renderd
 * tiledir/Z/[xxxxyyyy]/[xxxxyyyy]/[xxxxyyyy]/[xxxxyyyy]/[xxxxyyyy].meta
 */
bool MapSource::is_osm_meta_tiles(void) const
{
	return is_osm_meta_tiles_flag;
}




bool MapSource::supports_download_only_new(void) const
{
	return false;
}




/**
 * @self: the MapSource of interest.
 *
 * Returns the file extension of files held on disk.
 * Typically .png but may be .jpg or whatever the user defines.
 *
 */
QString MapSource::get_file_extension(void) const
{
	return this->file_extension;
}




bool MapSource::coord_to_tile(const Coord & scr_coord, const VikingZoomLevel & viking_zoom_level, TileInfo & dest) const
{
	fprintf(stderr, "MapSource coord_to_tile() returns false\n");
	return false;
}




void MapSource::tile_to_center_coord(const TileInfo & src, Coord & dest_coord) const
{
	fprintf(stderr, "MapSource::tile_to_center_coord\n");
	return;
}




/**
 * @self:    The MapSource of interest.
 * @src:     The map location to download
 * @dest_file_path: The filename to save the result in
 * @handle:  Potential reusable Curl Handle (may be NULL)
 *
 * Returns: How successful the download was as per the type #DownloadStatus
 */
DownloadStatus MapSource::download_tile(const TileInfo & src, const QString & dest_file_path, DownloadHandle * handle) const
{
	qDebug() << "II: Map Source: download to" << dest_file_path;
	handle->set_options(this->dl_options);
	return handle->get_url_http(get_server_hostname(), get_server_path(src), dest_file_path);
}




DownloadHandle * MapSource::download_handle_init(void) const
{
	return new DownloadHandle();
}




void MapSource::download_handle_cleanup(DownloadHandle * dl_handle) const
{
	delete dl_handle;
}




const QString MapSource::get_server_hostname(void) const
{
	return this->server_hostname;
}




const QString MapSource::get_server_path(const TileInfo & src) const
{
	return "";
}




const DownloadOptions * MapSource::get_download_options(void) const
{
	return &this->dl_options;
}




QPixmap MapSource::create_tile_pixmap_from_file(const QString & tile_file_full_path) const
{
	QPixmap result;

	if (0 != access(tile_file_full_path.toUtf8().constData(), F_OK | R_OK)) {
		qDebug() << SG_PREFIX_E << "Can't access file" << tile_file_full_path;
		return result;
	}

	if (!result.load(tile_file_full_path)) {
		Window * window = g_tree->tree_get_main_window();
		if (window) {
			window->statusbar_update(StatusBarField::INFO, QString("Couldn't open image file"));
		}
	}
	return result;
}



/* Default implementation of the method in base class is for web accessing map sources. */
QStringList MapSource::get_tile_description(const MapCacheObj & map_cache_obj, const TileInfo & tile_info, const MapSourceArgs & args) const
{
	QStringList items;

	const QString tile_file_full_path = map_cache_obj.get_cache_file_full_path(tile_info,
										   this->map_type_id,
										   this->get_map_type_string(),
										   this->get_file_extension());
	const QString source = QObject::tr("Source: http://%1%2").arg(this->get_server_hostname()).arg(this->get_server_path(tile_info));

	items.push_back(source);

	tile_info_add_file_info_strings(items, tile_file_full_path);

	return items;
}




/* Default implementation of the method in base class is for web accessing map sources. */
QPixmap MapSource::get_tile_pixmap(const MapCacheObj & map_cache_obj, const TileInfo & tile_info, const MapSourceArgs & args) const
{
	const QString tile_file_full_path = map_cache_obj.get_cache_file_full_path(tile_info,
										   this->map_type_id,
										   this->get_map_type_string(),
										   this->get_file_extension());

	QPixmap pixmap = this->create_tile_pixmap_from_file(tile_file_full_path);
	qDebug() << SG_PREFIX_I << "Creating pixmap from file:" << (pixmap.isNull() ? "failure" : "success");

	return pixmap;
}




bool MapSource::includes_tile(const TileInfo & tile_info) const
{
	Coord center_coord;
	this->tile_to_center_coord(tile_info, center_coord);

	const Coord coord_tl(LatLon(this->lat_max, this->lon_min), CoordMode::LATLON);
	const Coord coord_br(LatLon(this->lat_min, this->lon_max), CoordMode::LATLON);

	return center_coord.is_inside(&coord_tl, &coord_br);
}




QString MapSourceZoomLevel::to_string(void) const
{
	return QString(this->value);
}




void MapSource::set_supported_zoom_level_range(int zoom_level_min, int zoom_level_max)
{
	this->zoom_min = MapSourceZoomLevel(zoom_level_min);
	this->zoom_max = MapSourceZoomLevel(zoom_level_max);
}




bool MapSource::is_supported_zoom_level(const MapSourceZoomLevel & zoom_level) const
{
	return zoom_level.value >= this->zoom_min.value && zoom_level.value <= this->zoom_max.value;
}
