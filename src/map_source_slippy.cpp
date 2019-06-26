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




#include <cstdlib>
#include <cstring>




#include <QDebug>




#include "map_source_slippy.h"
#include "map_utils.h"
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "Map Source Slippy"




MapSourceSlippy::MapSourceSlippy()
{
	fprintf(stderr, "MapSourceSlippy constructor called\n");
}




MapSourceSlippy::MapSourceSlippy(MapTypeID new_map_type_id, const QString & new_label, const QString & new_server_hostname, const QString & new_server_path_format)
{
	qDebug() << SG_PREFIX_I << "Called VikSlippy constructor with id" << (int) new_map_type_id;

	this->map_type_id = new_map_type_id;
	this->label = new_label;
	this->server_hostname = new_server_hostname;
	this->server_path_format = new_server_path_format;
}




MapSourceSlippy::~MapSourceSlippy()
{
	fprintf(stderr, "MapSourceSlippy destructor called\n");
}




bool MapSourceSlippy::supports_download_only_new(void) const
{
	return this->dl_options.check_file_server_time || this->dl_options.use_etag;
}




bool MapSourceSlippy::coord_to_tile_info(const Coord & src_coord, const VikingScale & viking_scale, TileInfo & tile_info) const
{
	if (src_coord.get_coord_mode() != CoordMode::LatLon) {
		qDebug() << SG_PREFIX_E << "Invalid coord mode of argument";
		return false;
	}

	return sg_ret::ok == MapUtils::lat_lon_to_iTMS(src_coord.lat_lon, viking_scale, tile_info);
}




sg_ret MapSourceSlippy::tile_info_to_center_coord(const TileInfo & src, Coord & coord) const
{
	coord.set_coord_mode(CoordMode::LatLon); /* This function decides what will be the coord mode of returned coordinate. */
	coord.lat_lon = MapUtils::iTMS_to_center_lat_lon(src);
	return sg_ret::ok;
}




const QString MapSourceSlippy::get_server_path(const TileInfo & src) const
{
	QString uri;
	const int tile_zoom_level = src.scale.get_tile_zoom_level();
	if (this->switch_xy) {
		/* 'ARC GIS' Tile Server layout ordering. */
		uri = QString(this->server_path_format).arg(tile_zoom_level).arg(src.y).arg(src.x);
	} else {
		/* (Default) Standard OSM Tile Server layout ordering. */
		uri = QString(this->server_path_format).arg(tile_zoom_level).arg(src.x).arg(src.y);
	}
	return uri;
}




DownloadStatus MapSourceSlippy::download_tile(const TileInfo & src, const QString & dest_file_path, DownloadHandle * dl_handle) const
{
	dl_handle->set_options(this->dl_options);
	DownloadStatus result = dl_handle->perform_download(get_server_hostname(), get_server_path(src), dest_file_path, DownloadProtocol::HTTP);
	qDebug() << SG_PREFIX_I << "Download" << get_server_hostname() << get_server_path(src) << "->" << result;
	return result;
}
