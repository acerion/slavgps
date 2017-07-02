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

#ifndef _SG_MAP_SOURCE_H_
#define _SG_MAP_SOURCE_H_




#include <QPixmap>
#include <QString>

#include "viewport_internal.h"
#include "coord.h"
#include "mapcoord.h"
#include "bbox.h"
#include "download.h"
#include "map_ids.h"




namespace SlavGPS {




	class MapSource {
	public:
		MapSource();
		MapSource(MapSource & map);
		~MapSource();

		virtual MapSource & operator=(MapSource map);

		virtual void get_copyright(LatLonBBox bbox, double zoom, void (* fct)(Viewport *, QString const &), void * data);
		const char * get_license();
		const char * get_license_url();
		const QPixmap * get_logo();

		const QString get_server_hostname(void) const;
		virtual const QString get_server_path(TileInfo * src) const;

		const char * get_name();
		MapTypeID get_map_type();
		const char * get_label();
		uint16_t get_tilesize_x();
		uint16_t get_tilesize_y();
		ViewportDrawMode get_drawmode();

		virtual bool is_direct_file_access();
		virtual bool is_mbtiles();
		virtual bool is_osm_meta_tiles();

		virtual bool supports_download_only_new();

		uint8_t get_zoom_min();
		uint8_t get_zoom_max();
		double get_lat_min();
		double get_lat_max();
		double get_lon_min();
		double get_lon_max();
		const char * get_file_extension();

		virtual bool coord_to_tile(const Coord * src, double xzoom, double yzoom, TileInfo * dest);
		virtual void tile_to_center_coord(TileInfo * src, Coord * dest);

		virtual DownloadResult download(TileInfo * src, const char * dest_fn, void * handle);
		void * download_handle_init();
		void download_handle_cleanup(void * handle);

		const DownloadOptions * get_download_options(void) const;

		void set_name(char * name);
		void set_map_type(MapTypeID map_type);
		void set_label(char * label);
		void set_tilesize_x(uint16_t tilesize_x);
		void set_tilesize_y(uint16_t tilesize_y);
		void set_drawmode(ViewportDrawMode drawmode);
		void set_copyright(char * copyright);
		void set_license(char * license);
		void set_license_url(char * license_url);
		void set_file_extension(char * file_extension);




		char * copyright = NULL; /* The copyright of the map source. */
		char * license = NULL;   /* The license of the map source. */
		char * license_url = NULL; /* The URL of the license of the map source. */
		QPixmap * logo = NULL;

		char * name = NULL; /* The name of the map that may be used as the file cache directory. */
		MapTypeID map_type; /* Id of source of map (OSM MapQuest, OSM Transport, BlueMarble, etc.). */
		char * label = NULL; /* The label of the map source. */

		uint16_t tilesize_x; /* The size of the tile (x). */
		uint16_t tilesize_y; /* The size of the tile (x). */

		ViewportDrawMode drawmode; /* The mode used to draw map. */
		char * file_extension = NULL; /* The file extension of tile files on disk. */

		DownloadOptions dl_options;

		QString server_hostname = "";    /* The hostname of the map server. e.g. "tile.openstreetmap.org". */
		char * server_path_format = NULL; /* The template of the tiles' URL. e.g. "/%d/%d/%d.png" */

		// NB Probably best to keep the above fields in same order to be common across Slippy, TMS & WMS map definitions
		uint8_t zoom_min; /* Minimum Zoom level supported by the map provider.  TMS Zoom level: 0 = Whole World // http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames */
		uint8_t zoom_max; /* Maximum Zoom level supported by the map provider. / TMS Zoom level: Often 18 for zoomed in. */
		double lat_min; /* Minimum latitude in degrees supported by the map provider. Degrees. */
		double lat_max; /* Maximum latitude in degrees supported by the map provider. Degrees. */
		double lon_min; /* Minimum longitude in degrees supported by the map provider. Degrees. */
		double lon_max; /* Maximum longitude in degrees supported by the map provider. Degrees. */

		bool is_direct_file_access_flag;
		bool is_mbtiles_flag;
		bool is_osm_meta_tiles_flag; /* http://wiki.openstreetmap.org/wiki/Meta_tiles as used by tirex or renderd. */

		/* Mainly for ARCGIS Tile Server URL Layout // http://help.arcgis.com/EN/arcgisserver/10.0/apis/rest/tile.html */
		bool switch_xy;
	};




} /* namespace SlavGPS */




#endif /* _SG_MAP_SOURCE_H_ */
