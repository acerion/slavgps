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




using namespace SlavGPS;




#define PREFIX ": Map Source Slippy:" << __FUNCTION__ << __LINE__ << ">"




MapSourceSlippy::MapSourceSlippy()
{
	fprintf(stderr, "MapSourceSlippy constructor called\n");
}




MapSourceSlippy & MapSourceSlippy::operator=(MapSourceSlippy map)
{
	fprintf(stderr, "MapSourceSlippy copy assignment called\n");

	this->set_copyright(map.copyright);
	this->set_license(map.license);
	this->set_license_url(map.license_url);
	this->logo = NULL; //memcpy(this->logo, map.logo, sizeof (QPixmap)); /* FIXME: implement this. */

	this->set_name(map.name);
	this->map_type = map.map_type;
	this->set_label(map.label);
	this->tilesize_x = map.tilesize_x;
	this->tilesize_y = map.tilesize_y;
	this->drawmode = map.drawmode;
	this->set_file_extension(map.file_extension);

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
	this->is_mbtiles_flag = map.is_mbtiles_flag;
	this->is_osm_meta_tiles_flag = map.is_osm_meta_tiles_flag;

	this->switch_xy = map.switch_xy;

        return *this;
}




MapSourceSlippy::MapSourceSlippy(MapTypeID new_map_type, const QString & new_label, const QString & new_server_hostname, const QString & new_server_path_format)
{
	qDebug() << "II" PREFIX << "called VikSlippy constructor with id" << (int) new_map_type;

	this->map_type = new_map_type;
	this->label = new_label;
	this->server_hostname = new_server_hostname;
	this->server_path_format = new_server_path_format;
}




MapSourceSlippy::~MapSourceSlippy()
{
	fprintf(stderr, "MapSourceSlippy destructor called\n");
}




bool MapSourceSlippy::is_direct_file_access()
{
	return is_direct_file_access_flag;
}




bool MapSourceSlippy::is_mbtiles()
{
	return is_mbtiles_flag;
}




bool MapSourceSlippy::is_osm_meta_tiles()
{
	return is_osm_meta_tiles_flag;
}




bool MapSourceSlippy::supports_download_only_new()
{
	return this->dl_options.check_file_server_time || this->dl_options.use_etag;
}




bool MapSourceSlippy::coord_to_tile(const Coord & src_coord, double xzoom, double yzoom, TileInfo * dest)
{
	bool result = map_utils_coord_to_iTMS(src_coord, xzoom, yzoom, dest);
	return result;
}




void MapSourceSlippy::tile_to_center_coord(TileInfo * src, Coord & dest_coord)
{
	dest_coord = map_utils_iTMS_to_center_coord(src);
}




const QString MapSourceSlippy::get_server_path(TileInfo * src) const
{
	QString uri;
	if (switch_xy) {
		/* 'ARC GIS' Tile Server layout ordering. */
		uri = QString(this->server_path_format).arg(17 - src->scale).arg(src->y).arg(src->x);
	} else {
		/* (Default) Standard OSM Tile Server layout ordering. */
		uri = QString(this->server_path_format).arg(17 - src->scale).arg(src->x).arg(src->y);
	}
	return uri;
}




DownloadResult MapSourceSlippy::download(TileInfo * src, const QString & dest_file_path, DownloadHandle * dl_handle)
{
	dl_handle->set_options(this->dl_options);
	DownloadResult result = dl_handle->get_url_http(get_server_hostname(), get_server_path(src), dest_file_path);
	qDebug() << "II: Map Source Slippy: download" << get_server_hostname() << get_server_path(src) << "->" << (int) result;
	return result;
}
