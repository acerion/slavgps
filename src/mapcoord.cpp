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




#include <algorithm> /* std::min(), std::max() */




#include "mapcoord.h"
#include "map_utils.h"




using namespace SlavGPS;




TilesRange TileInfo::get_tiles_range(const TileInfo & ulm, const TileInfo & brm)
{
	TilesRange range;

	range.x_begin = std::min(ulm.x, brm.x);
	range.x_end   = std::max(ulm.x, brm.x);
	range.y_begin = std::min(ulm.y, brm.y);
	range.y_end   = std::max(ulm.y, brm.y);

	return range;
}




int TileScale::get_osm_scale(void) const
{
	return MAGIC_SEVENTEEN - this->value;
}




int TileScale::get_non_osm_scale(void) const
{
	return this->value;
}




bool TileScale::is_valid(void) const
{
	return this->valid;
}
