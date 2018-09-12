/*
 * viking
 * Copyright (C) 2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

 /**
  * SECTION:map_source_slippy
  * @short_description: the class for SlippyMap oriented map sources
  *
  * The #VikSlippyMapSource class handles slippy map oriented map sources.
  * The related service is tile oriented, à la Google.
  *
  * The tiles are in 'google spherical mercator', which is
  * basically a mercator projection that assumes a spherical earth.
  * http://docs.openlayers.org/library/spherical_mercator.html
  *
  * Such service is also a type of TMS (Tile Map Service) as defined in
  * OSGeo's wiki.
  * http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification
  * But take care that the Y axis is inverted, ie the origin is at top-left
  * corner.
  * Following this specification, the protocol handled by this class
  * follows the global-mercator profile.
  *
  * You can also find many interesting information on the OSM's wiki.
  * http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
  * http://wiki.openstreetmap.org/wiki/Setting_up_TMS
  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif




#include <glib.h>
#include <cstdlib>
#include <cstring>




#include <QDebug>




#include "map_source_slippy.h"
#include "map_utils.h"
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "Map Source Slippy"
#define PREFIX ": Map Source Slippy:" << __FUNCTION__ << __LINE__ << ">"




MapSourceSlippy::MapSourceSlippy()
{
	fprintf(stderr, "MapSourceSlippy constructor called\n");
}




/* TODO_LATER: how many of these operations should be performed by parent class' operator? */
MapSourceSlippy & MapSourceSlippy::operator=(const MapSourceSlippy & other)
{
	qDebug() << SG_PREFIX_I << "Copy assignment called";
	if (&other == this) {
		/* Self-assignment. */
		return *this;
	}

	this->set_copyright(other.copyright);
	this->set_license(other.license);
	this->set_license_url(other.license_url);

	this->logo.logo_pixmap = other.logo.logo_pixmap;
	this->logo.logo_id     = other.logo.logo_id;

	this->set_map_type_string(other.map_type_string); /* Non-translatable. */
	this->map_type_id = other.map_type_id;
	this->set_label(other.label);
	this->tilesize_x = other.tilesize_x;
	this->tilesize_y = other.tilesize_y;
	this->drawmode = other.drawmode;
	this->set_file_extension(other.file_extension);

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




MapSourceSlippy::MapSourceSlippy(MapTypeID new_map_type_id, const QString & new_label, const QString & new_server_hostname, const QString & new_server_path_format)
{
	qDebug() << "II" PREFIX << "called VikSlippy constructor with id" << (int) new_map_type_id;

	this->map_type_id = new_map_type_id;
	this->label = new_label;
	this->server_hostname = new_server_hostname;
	this->server_path_format = new_server_path_format;
}




MapSourceSlippy::~MapSourceSlippy()
{
	fprintf(stderr, "MapSourceSlippy destructor called\n");
}




bool MapSourceSlippy::is_direct_file_access(void) const
{
	return this->is_direct_file_access_flag;
}




bool MapSourceSlippy::is_osm_meta_tiles(void) const
{
	return is_osm_meta_tiles_flag;
}




bool MapSourceSlippy::supports_download_only_new(void) const
{
	return this->dl_options.check_file_server_time || this->dl_options.use_etag;
}




bool MapSourceSlippy::coord_to_tile(const Coord & src_coord, double xzoom, double yzoom, TileInfo & dest) const
{
	bool result = map_utils_coord_to_iTMS(src_coord, xzoom, yzoom, dest);
	return result;
}




void MapSourceSlippy::tile_to_center_coord(const TileInfo & src, Coord & dest_coord) const
{
	dest_coord = map_utils_iTMS_to_center_coord(src);
}




const QString MapSourceSlippy::get_server_path(const TileInfo & src) const
{
	QString uri;
	if (switch_xy) {
		/* 'ARC GIS' Tile Server layout ordering. */
		uri = QString(this->server_path_format).arg(MAGIC_SEVENTEEN - src.scale).arg(src.y).arg(src.x);
	} else {
		/* (Default) Standard OSM Tile Server layout ordering. */
		uri = QString(this->server_path_format).arg(MAGIC_SEVENTEEN - src.scale).arg(src.x).arg(src.y);
	}
	return uri;
}




DownloadStatus MapSourceSlippy::download_tile(const TileInfo & src, const QString & dest_file_path, DownloadHandle * dl_handle) const
{
	dl_handle->set_options(this->dl_options);
	DownloadStatus result = dl_handle->get_url_http(get_server_hostname(), get_server_path(src), dest_file_path);
	qDebug() << "II: Map Source Slippy: download" << get_server_hostname() << get_server_path(src) << "->" << result;
	return result;
}
