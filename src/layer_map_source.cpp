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




#include <cassert>
#include <cstdlib>
#include <cstring>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif




#include <QDebug>




#include "window.h"
#include "viewport.h"
#include "coord.h"
#include "layer_map_tile.h"
#include "download.h"
#include "layer_map_source.h"
#include "map_cache.h"
#include "layer_map.h"
#include "statusbar.h"




using namespace SlavGPS;




#define SG_MODULE "Map Source"




MapSource::MapSource()
{
	fprintf(stderr, "MapSource regular constructor called\n");

	this->m_map_type_string = "< ?? >"; /* Non-translatable. */
	this->m_ui_label = "< ?? >";

	this->m_tilesize_x = 256;
	this->m_tilesize_y = 256;

	this->drawmode = GisViewportDrawMode::Mercator; /* ViewportDrawMode::UTM */
	this->file_extension = ".png";

	this->dl_options.file_validator_fn = map_file_validator_fn;

	this->is_direct_file_access_flag = false; /* Use direct file access to OSM like tile images? No, not for a webservice. */
	this->is_osm_meta_tiles_flag = false; /* Read from OSM Meta Tiles? Should be 'use-direct-file-access' as well. */

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

	this->m_map_type_string = other.m_map_type_string;
	this->m_map_type_id   = other.m_map_type_id;
	this->m_ui_label      = other.m_ui_label;

	this->m_tilesize_x = other.m_tilesize_x;
	this->m_tilesize_y = other.m_tilesize_y;

	this->drawmode   = other.drawmode;
	this->file_extension = other.file_extension;

	memcpy(&this->dl_options, &other.dl_options, sizeof (DownloadOptions));

	this->server_hostname    = other.server_hostname;
	this->server_path_format = other.server_path_format;

	this->m_tile_zoom_level_min = other.m_tile_zoom_level_min;
	this->m_tile_zoom_level_max = other.m_tile_zoom_level_max;

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

	this->m_map_type_string = map.m_map_type_string;
	this->m_map_type_id   = map.m_map_type_id;
	this->m_ui_label      = map.m_ui_label;

	this->m_tilesize_x = map.m_tilesize_x;
	this->m_tilesize_y = map.m_tilesize_y;

	this->drawmode   = map.drawmode;
	this->file_extension = map.file_extension;

	memcpy(&this->dl_options, &map.dl_options, sizeof (DownloadOptions));

	this->server_hostname    = map.server_hostname;
	this->server_path_format = map.server_path_format;

	this->m_tile_zoom_level_min = map.m_tile_zoom_level_min;
	this->m_tile_zoom_level_max = map.m_tile_zoom_level_max;

	this->lat_min = map.lat_min;
	this->lat_max = map.lat_max;
	this->lon_min = map.lon_min;
	this->lon_max = map.lon_max;

	this->is_direct_file_access_flag = map.is_direct_file_access_flag;
	this->is_osm_meta_tiles_flag = map.is_osm_meta_tiles_flag;

	this->switch_xy = map.switch_xy;
}




void MapSource::set_map_type_string(const QString & map_type_string)
{
	this->m_map_type_string = map_type_string;

	/* Sanitize the name here for file usage.
	   A simple check just to prevent names containing slashes. */
	this->m_map_type_string.replace('\\', 'x');
	this->m_map_type_string.replace('/', 'x');
}




bool MapSource::set_map_type_id(MapTypeID map_type_id)
{
	if (!MapSource::is_map_type_id_registered(map_type_id)) {
		qDebug() << SG_PREFIX_E << "Unknown map type" << (int) map_type_id;
		return false;
	}

	this->m_map_type_id = map_type_id;
	return true;
}




void MapSource::set_ui_label(const QString & ui_label)
{
	this->m_ui_label = ui_label;
}




void MapSource::set_tilesize_x(uint16_t tilesize_x)
{
	this->m_tilesize_x = tilesize_x;
}



void MapSource::set_tilesize_y(uint16_t tilesize_y)
{
	this->m_tilesize_y = tilesize_y;
}




void MapSource::set_drawmode(GisViewportDrawMode new_drawmode)
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
void MapSource::add_copyright(__attribute__((unused)) GisViewport * gisview, __attribute__((unused)) const LatLonBBox & bbox, __attribute__((unused)) const VikingScale & viking_scale)
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




const GisViewportLogo & MapSource::get_logo(void) const
{
	return this->logo;
}




const QString & MapSource::map_type_string(void) const
{
	return this->m_map_type_string;
}




MapTypeID MapSource::map_type_id(void) const
{
	qDebug() << SG_PREFIX_D << "Returning map type" << (int) this->m_map_type_id << "for source type" << this->m_ui_label;
	return this->m_map_type_id;
}




const QString & MapSource::ui_label(void) const
{
	return this->m_ui_label;
}




uint16_t MapSource::tilesize_x(void) const
{
	return this->m_tilesize_x;
}




uint16_t MapSource::tilesize_y(void) const
{
	return this->m_tilesize_y;
}




GisViewportDrawMode MapSource::get_drawmode(void) const
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




bool MapSource::is_osm_meta_tiles(void) const
{
	return this->is_osm_meta_tiles_flag;
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




bool MapSource::coord_to_tile_info(__attribute__((unused)) const Coord & scr_coord, __attribute__((unused)) const VikingScale & viking_scale, __attribute__((unused)) TileInfo & tile_info) const
{
	qDebug() << SG_PREFIX_E << "Called method from base class";
	return false;
}




sg_ret MapSource::tile_info_to_center_coord(__attribute__((unused)) const TileInfo & src, __attribute__((unused)) Coord & coord) const
{
	qDebug() << SG_PREFIX_E << "Called method from base class";
	return sg_ret::err;
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
	return handle->perform_download(get_server_hostname(), get_server_path(src), dest_file_path, DownloadProtocol::HTTP);
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




const QString MapSource::get_server_path(__attribute__((unused)) const TileInfo & src) const
{
	return "";
}




const DownloadOptions * MapSource::get_download_options(void) const
{
	return &this->dl_options;
}




QPixmap MapSource::load_tile_pixmap_from_file(const QString & tile_file_full_path) const
{
	QPixmap result;

	if (0 != access(tile_file_full_path.toUtf8().constData(), F_OK | R_OK)) {
		qDebug() << SG_PREFIX_E << "Can't access file" << tile_file_full_path;
		return result;
	}

	if (!result.load(tile_file_full_path)) {
		Window * window = ThisApp::get_main_window();
		if (window) {
			window->statusbar_update(StatusBarField::Info, QObject::tr("Couldn't open file with tile pixmap"));
		}
	}
	return result;
}



/* Default implementation of the method in base class is for web accessing map sources. */
QStringList MapSource::get_tile_description(const MapCacheObj & map_cache_obj, const TileInfo & tile_info) const
{
	QStringList items;

	const QString tile_file_full_path = map_cache_obj.get_cache_file_full_path(tile_info,
										   this->map_type_id(),
										   this->map_type_string(),
										   this->get_file_extension());
	const QString source = QObject::tr("Source: http://%1%2").arg(this->get_server_hostname()).arg(this->get_server_path(tile_info));

	items.push_back(source);

	tile_info_add_file_info_strings(items, tile_file_full_path);

	return items;
}




/* Default implementation of the method in base class is for web accessing map sources. */
QPixmap MapSource::create_tile_pixmap(const MapCacheObj & map_cache_obj, const TileInfo & tile_info) const
{
	const QString tile_file_full_path = map_cache_obj.get_cache_file_full_path(tile_info,
										   this->map_type_id(),
										   this->map_type_string(),
										   this->get_file_extension());

	QPixmap pixmap = this->load_tile_pixmap_from_file(tile_file_full_path);
	qDebug() << SG_PREFIX_I << "Creating pixmap from file:" << (pixmap.isNull() ? "failure" : "success");

	return pixmap;
}



bool MapSource::includes_tile(const TileInfo & tile_info) const
{
	Coord center_coord;
	if (sg_ret::ok != this->tile_info_to_center_coord(tile_info, center_coord)) {
		qDebug() << SG_PREFIX_E << "Failed to convert tile into to coordinate";
		return false;
	}

	const Coord coord_tl(LatLon(this->lat_max, this->lon_min), CoordMode::LatLon);
	const Coord coord_br(LatLon(this->lat_min, this->lon_max), CoordMode::LatLon);

	return center_coord.is_inside(coord_tl, coord_br);
}




TileZoomLevel::TileZoomLevel(int value)
{
	if (value >= (int) TileZoomLevel::Level::Min && value <= (int) TileZoomLevel::Level::Max) {
		this->m_value = value;
	} else {
		qDebug() << SG_PREFIX_E << "Invalid value passed to constructor:" << value;
		this->m_value = (int) TileZoomLevel::Level::Default;
	}
}




QString TileZoomLevel::to_string(void) const
{
	return QString(this->m_value);
}




bool TileZoomLevel::unit_tests(void)
{
	{
		TileZoomLevel smaller(0);
		TileZoomLevel larger(1);

		assert(smaller < larger);
		assert(smaller <= larger);
		assert(!(smaller > larger));
		assert(!(smaller >= larger));
	}

	{
		TileZoomLevel larger(5);
		TileZoomLevel smaller(4);

		assert(larger > smaller);
		assert(larger >= smaller);
		assert(!(larger < smaller));
		assert(!(larger <= smaller));
	}

	{
		TileZoomLevel equal1(4);
		TileZoomLevel equal2(4);

		assert(!(equal1 > equal2));
		assert(equal1 >= equal2);
		assert(!(equal1 < equal2));
		assert(equal1 <= equal2);
	}

	return true;
}




bool TileZoomLevel::operator<(const TileZoomLevel & rhs) const { return this->m_value < rhs.m_value; }
bool TileZoomLevel::operator>(const TileZoomLevel & rhs) const { return rhs < *this; }
bool TileZoomLevel::operator<=(const TileZoomLevel & rhs) const { return !(*this > rhs); }
bool TileZoomLevel::operator>=(const TileZoomLevel & rhs) const { return !(*this < rhs); }




void MapSource::set_supported_tile_zoom_level_range(const TileZoomLevel & tile_zoom_level_min, const TileZoomLevel & tile_zoom_level_max)
{
	this->m_tile_zoom_level_min = tile_zoom_level_min;
	this->m_tile_zoom_level_max = tile_zoom_level_max;
}




bool MapSource::is_supported_tile_zoom_level(const TileZoomLevel & tile_zoom_level) const
{
	return tile_zoom_level >= this->m_tile_zoom_level_min && tile_zoom_level <= this->m_tile_zoom_level_max;
}
