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

#include "mapcoord.h"
#include "map_ids.h"




namespace SlavGPS {




	typedef struct {
		double duration; // Mostly for Mapnik Rendering duration - negative values indicate not rendered(i.e. read from disk)
	} map_cache_extra_t;




	void map_cache_init(void);
	void map_cache_uninit(void);

	void map_cache_add(QPixmap * pixmap, map_cache_extra_t extra, TileInfo * tile_info, MapTypeID map_type, uint8_t alpha, double xshrinkfactor, double yshrinkfactor, char const * name);
	QPixmap * map_cache_get(TileInfo * tile_info, MapTypeID map_type, uint8_t alpha, double xshrinkfactor, double yshrinkfactor, char const * name);
	map_cache_extra_t map_cache_get_extra(TileInfo * tile_info, MapTypeID map_type, uint8_t alpha, double xshrinkfactor, double yshrinkfactor, char const * name);

	void map_cache_remove_all_shrinkfactors(TileInfo * tile_info, MapTypeID map_type, char const * name);
	void map_cache_flush();
	void map_cache_flush_type(MapTypeID map_type);

	size_t map_cache_get_size();
	int map_cache_get_count();




} /* namespace SlavGPS */




#endif /* #ifndef _SG_MAP_CACHE_H_ */
