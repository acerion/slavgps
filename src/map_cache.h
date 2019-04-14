/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_MAP_CACHE_H_
#define _SG_MAP_CACHE_H_




#include <cstdint>




#include <QPixmap>
#include <QString>




#include "mapcoord.h"
#include "map_source.h"




namespace SlavGPS {




/* Number of digits after comma for render time presented in
   seconds. */
#define SG_RENDER_TIME_RESOLUTION   6

/* Rendering was not performed (at least not during the application's
   run). Pixmap was already existing on disc and original render time
   is not available. */
#define SG_RENDER_TIME_NO_RENDER    0




	class PixmapScale;




	enum class MapCacheLayout {
		Viking = 0, /* CacheDir/t<MapId>s<VikingZoom>z0/X/Y (Legacy default layout. Notice no file extension.) */
		OSM,        /* CacheDir/<OptionalMapName>/OSMZoomLevel/X/Y.ext (Default extension (ext) is "png".) */
		Num         /* Last enum. */
	};




	class MapCacheItemProperties {
	public:
		MapCacheItemProperties() {}
		MapCacheItemProperties(long duration) : rendering_duration_ns(duration) {}

		/*
		  How much time it took to render a pixmap.

		  The unit of this fields is the same as of 'struct
		  timespec::tv_nsec'.
		*/
		long rendering_duration_ns = SG_RENDER_TIME_NO_RENDER; /* [nanoseconds] */
	};




	class MapCache {
	public:
		static void init(void);
		static void uninit(void);

		static void add_tile_pixmap(const QPixmap & pixmap, const MapCacheItemProperties & properties, const TileInfo & tile_info, MapTypeID map_type, int alpha, const PixmapScale & pixmap_scale, const QString & file_name);
		static QPixmap get_tile_pixmap(const TileInfo & tile_info, MapTypeID map_type, int alpha, const PixmapScale & pixmap_scale, const QString & file_name);
		static MapCacheItemProperties get_properties(const TileInfo & tile_info, MapTypeID map_type, int alpha, const PixmapScale & pixmap_scale, const QString & file_name);


		/* Get size of map cache in memory (in bytes). */
		static size_t get_size_bytes(void);

		/* Get number (count) of items in the map cache. */
		static int get_items_count(void);

		static void remove_all_shrinkfactors(const TileInfo & tile_info, MapTypeID map_type, const QString & file_name);
		static void flush(void);
		static void flush_type(MapTypeID map_type);

		static const QString & get_dir();
		static const QString & get_default_maps_dir(void);
	private:
	};




	class MapCacheObj {
	public:
		MapCacheObj() {}
		MapCacheObj(MapCacheLayout cache_layout, const QString & cache_dir_full_path) : layout(cache_layout), dir_full_path(cache_dir_full_path) {}

		QString get_cache_file_full_path(const TileInfo & tile_info, MapTypeID map_type_id, const QString & map_type_string, const QString & file_extension) const;

		MapCacheLayout layout;
		QString dir_full_path;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_MAP_CACHE_H_ */
