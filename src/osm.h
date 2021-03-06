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

#ifndef _SG_OSM_H_
#define _SG_OSM_H_




#include "layer_map_source_slippy.h"




namespace SlavGPS {




	class OSM {
	public:
		static void init(void);
	};




	class MapSourceOSMMetatiles : public MapSourceSlippy {
	public:
		MapSourceOSMMetatiles();
		~MapSourceOSMMetatiles() {}
		QPixmap create_tile_pixmap(const MapCachePath & cache_path, const TileInfo & tile_info) const override;
		QStringList get_tile_description(const MapCachePath & cache_path, const TileInfo & tile_info) const override;

	private:
		QString full_path;
	};




	class MapSourceOSMOnDisk : public MapSourceSlippy {
	public:
		MapSourceOSMOnDisk();
		~MapSourceOSMOnDisk() {}
		QPixmap create_tile_pixmap(const MapCachePath & cache_path, const TileInfo & tile_info) const override;
		QStringList get_tile_description(const MapCachePath & cache_path, const TileInfo & tile_info) const override;
	};





} /* namespace SlavGPS */




#endif /* #ifndef _SG_OSM_H_ */
