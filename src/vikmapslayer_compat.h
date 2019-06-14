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

#ifndef _SG_MAPSLAYER_COMPAT_H
#define _SG_MAPSLAYER_COMPAT_H




#include <cstdint>




#include <QString>




#include "coord.h"
#include "mapcoord.h"




namespace SlavGPS {




	typedef struct {
		MapTypeID uniq_id;
		uint16_t tilesize_x;
		uint16_t tilesize_y;
		ViewportDrawMode drawmode;
		bool (* coord_to_tile_info) (const Coord & src_coord, const VikingScale & viking_scale, TileInfo & tile_info);
		sg_ret (* tile_info_to_center_coord) (const TileInfo & src, Coord & coord);
		DownloadStatus (* download) (const TileInfo & src, const QString & dest_file_path, DownloadHandle * dl_handle);
		void *(* download_handle_init) ();
		void (* download_handle_cleanup) (void * handle);
	} VikMapsLayer_MapType;




	void maps_layer_register_type(const QString & label, MapTypeID id, VikMapsLayer_MapType * map_type);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_MAPSLAYER_COMPAT_H */
