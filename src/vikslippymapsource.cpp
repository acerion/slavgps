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
  * SECTION:vikslippymapsource
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

#include "vikslippymapsource.h"
#include "maputils.h"




using namespace SlavGPS;




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
	this->logo = NULL; //memcpy(this->logo, map.logo, sizeof (GdkPixbuf)); /* FIXME: implement this. */

	this->set_name(map.name);
	this->map_type = map.map_type;
	this->set_label(map.label);
	this->tilesize_x = map.tilesize_x;
	this->tilesize_y = map.tilesize_y;
	this->drawmode = map.drawmode;
	this->set_file_extension(map.file_extension);

	memcpy(&this->download_options, &map.download_options, sizeof (DownloadFileOptions));

	this->server_hostname = g_strdup(map.server_hostname);
	this->server_path_format = g_strdup(map.server_path_format);

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

	fprintf(stderr, "MapSourceSlippy &= hostname = %d, path_pattern = %s\n", server_hostname, server_path_format);

        return *this;
}




MapSourceSlippy::MapSourceSlippy(MapTypeID map_type_, char const * label_, char const * hostname_, char const * path_format_)
{
	map_type = map_type_;
	fprintf(stderr, "++++++++++ called VikSlippy constructor with id %u / %u\n", map_type_, map_type);
	label = g_strdup(label_);
	server_hostname = g_strdup(hostname_);
	server_path_format = g_strdup(path_format_);
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
	return download_options.check_file_server_time || download_options.use_etag;
}




bool MapSourceSlippy::coord_to_tile(const VikCoord * src, double xzoom, double yzoom, TileInfo * dest)
{
	bool result = map_utils_vikcoord_to_iTMS(src, xzoom, yzoom, dest);
	return result;
}




void MapSourceSlippy::tile_to_center_coord(TileInfo *src, VikCoord *dest)
{
	map_utils_iTMS_to_center_vikcoord(src, dest);
}




char * MapSourceSlippy::get_server_path(TileInfo * src)
{
	if (switch_xy) {
		/* 'ARC GIS' Tile Server layout ordering. */
		return g_strdup_printf(server_path_format, 17 - src->scale, src->y, src->x);
	} else {
		/* (Default) Standard OSM Tile Server layout ordering. */
		return g_strdup_printf(server_path_format, 17 - src->scale, src->x, src->y);
	}
}




DownloadResult_t MapSourceSlippy::download(TileInfo * src, const char * dest_fn, void * handle)
{
	DownloadResult_t result = a_http_download_get_url(get_server_hostname(), get_server_path(src), dest_fn, &download_options, handle);
	//fprintf(stderr, "MapSourceSlippy::download(%s, %s) -> %d\n", get_server_hostname(), get_server_path(src), result);
	return result;
}
