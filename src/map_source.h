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



#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QPixmap>
#include <QString>

#include "coord.h"
#include "mapcoord.h"
#include "bbox.h"
#include "download.h"
#include "viewport.h"

#ifdef HAVE_SQLITE3_H
#include "sqlite3.h"
#endif




namespace SlavGPS {




	class MapCacheObj;
	enum class MapCacheLayout;
	enum class ViewportDrawMode;
	class Viewport;




	enum class MapTypeID {
		Initial             =  -1, /* No ID set yet. */
		Default             =   0, /* Let the program select default map type id. */

		/* Old Terraserver ids - listed for compatibility. */
		TerraserverAerial   =   1,
		TerraserverTopo     =   2,
		TerraserverUrban    =   4,

		Expedia             =   5,

		MapnikRender        =   7,

		/* Mostly OSM related - except the Blue Marble value. */
		OSMMapnik           =  13,
		BlueMarble          =  15,
		OSMCycle            =  17,
		MapQuestOSM         =  19,
		OSMTransport        =  20,
		OSMOnDisk           =  21,
		OSMHumanitarian     =  22,
		MBTiles             =  23,
		OSMMetatiles        =  24,

		BingAerial          = 212

		/*
		  Unfortunately previous ID allocations have been a
		  little haphazard, but hopefully future IDs can be
		  follow this scheme:

		   - 0 to 31 are intended for hard coded internal defaults
		   - 32-127 are intended for XML configuration map supplied defaults: see data/maps.xml
		   - 128 and above are intended for end user configurations.
		*/
	};




	class MapSourceArgs {
	public:
		QString map_type_string; /* May be an empty string, and may be different than source's type string. */
		QString tile_file_full_path;

		QWidget * parent_window = NULL;

#ifdef HAVE_SQLITE3_H
		sqlite3 ** sqlite_handle = NULL;
#endif
	};




	class MapSource {
	public:
		MapSource();
		MapSource(MapSource & map);
		virtual ~MapSource();

		virtual MapSource & operator=(const MapSource & other);

		virtual void add_copyright(Viewport * viewport, const LatLonBBox & bbox, double zoom);
		QString get_license(void) const;
		QString get_license_url(void) const;
		const ViewportLogo & get_logo(void) const;

		const QString get_server_hostname(void) const;
		virtual const QString get_server_path(const TileInfo & src) const;

		static bool is_map_type_id_registered(MapTypeID map_type_id);
		QString get_map_type_string(void) const;
		MapTypeID get_map_type_id(void) const;
		QString get_label(void) const;
		uint16_t get_tilesize_x(void) const;
		uint16_t get_tilesize_y(void) const;
		ViewportDrawMode get_drawmode(void) const;

		virtual QPixmap get_tile_pixmap(const MapCacheObj & map_cache_obj, const TileInfo & tile_info, const MapSourceArgs & args) const;
		virtual QStringList get_tile_description(const MapCacheObj & map_cache_obj, const TileInfo & tile_info, const MapSourceArgs & args) const;
		virtual void close_map_source(MapSourceArgs & args) { return; }
		virtual void post_read(MapSourceArgs & args) { return; }
		QPixmap create_tile_pixmap_from_file(const QString & tile_file_full_path) const;

		/* TODO_LATER: do these need to be virtual methods? */
		virtual bool is_direct_file_access(void) const;
		virtual bool is_osm_meta_tiles(void) const;

		virtual bool supports_download_only_new(void) const;

		uint8_t get_zoom_min(void) const;
		uint8_t get_zoom_max(void) const;
		double get_lat_min(void) const;
		double get_lat_max(void) const;
		double get_lon_min(void) const;
		double get_lon_max(void) const;
		QString get_file_extension(void) const;

		virtual bool coord_to_tile(const Coord & src_coord, double xzoom, double yzoom, TileInfo & dest) const;
		virtual void tile_to_center_coord(const TileInfo & src, Coord & dest_coord) const;

		virtual DownloadResult download_tile(const TileInfo & src, const QString & dest_file_path, DownloadHandle * dl_handle) const;
		DownloadHandle * download_handle_init(void) const;
		void download_handle_cleanup(DownloadHandle * dl_handle) const;

		const DownloadOptions * get_download_options(void) const;

		void set_map_type_string(const QString & map_type_string);
		bool set_map_type_id(MapTypeID map_type_id);
		void set_label(const QString & label);
		void set_tilesize_x(uint16_t tilesize_x);
		void set_tilesize_y(uint16_t tilesize_y);
		void set_drawmode(ViewportDrawMode drawmode);
		void set_copyright(const QString & copyright);
		void set_license(const QString & license);
		void set_license_url(const QString & license_url);
		void set_file_extension(const QString & file_extension);


		bool includes_tile(const TileInfo & tile_info) const;


		QString copyright;     /* The copyright of the map source. */
		QString license;       /* The license of the map source. */
		QString license_url;   /* The URL of the license of the map source. */
		ViewportLogo logo;

		QString map_type_string;  /* The name of the map that may be used as the file cache directory. Non-translatable. */
		MapTypeID map_type_id;    /* Id of source of map (OSM MapQuest, OSM Transport, BlueMarble, etc.). */
		QString label;            /* The label of the map source. */

		uint16_t tilesize_x; /* The size of the tile (x). */
		uint16_t tilesize_y; /* The size of the tile (x). */

		ViewportDrawMode drawmode; /* The mode used to draw map. */
		QString file_extension;    /* The file extension of tile files on disk. */

		DownloadOptions dl_options;

		QString server_hostname;    /* The hostname of the map server. e.g. "tile.openstreetmap.org". */
		QString server_path_format; /* The template of the tiles' URL. e.g. "/%d/%d/%d.png" */

		// NB Probably best to keep the above fields in same order to be common across Slippy, TMS & WMS map definitions
		uint8_t zoom_min; /* Minimum Zoom level supported by the map provider.  TMS Zoom level: 0 = Whole World // http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames */
		uint8_t zoom_max; /* Maximum Zoom level supported by the map provider. / TMS Zoom level: Often 18 for zoomed in. */
		double lat_min; /* Minimum latitude in degrees supported by the map provider. Degrees. */
		double lat_max; /* Maximum latitude in degrees supported by the map provider. Degrees. */
		double lon_min; /* Minimum longitude in degrees supported by the map provider. Degrees. */
		double lon_max; /* Maximum longitude in degrees supported by the map provider. Degrees. */

		bool is_direct_file_access_flag;
		bool is_osm_meta_tiles_flag; /* http://wiki.openstreetmap.org/wiki/Meta_tiles as used by tirex or renderd. */

		/* Mainly for ARCGIS Tile Server URL Layout // http://help.arcgis.com/EN/arcgisserver/10.0/apis/rest/tile.html */
		bool switch_xy;
	};




} /* namespace SlavGPS */




#endif /* _SG_MAP_SOURCE_H_ */
