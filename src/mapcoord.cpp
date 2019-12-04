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




TilesRange TileInfo::get_tiles_range(const TileInfo & tile_info_ul, const TileInfo & tile_info_br)
{
	TilesRange range;

	range.x_first = std::min(tile_info_ul.x, tile_info_br.x);
	range.x_last  = std::max(tile_info_ul.x, tile_info_br.x);
	range.y_first = std::min(tile_info_ul.y, tile_info_br.y);
	range.y_last  = std::max(tile_info_ul.y, tile_info_br.y);

	return range;
}




void TileInfo::scale_up(int scale_dec, int scale_factor)
{
	this->x = this->x * scale_factor;
	this->y = this->y * scale_factor;
	this->scale.set_scale_value(this->scale.get_scale_value() - scale_dec); /* TODO_LATER: should it be "- scale_dec" or "/ scale_dec"? */
}




void TileInfo::scale_down(int scale_inc, int scale_factor)
{
	this->x = this->x / scale_factor;
	this->y = this->y / scale_factor;
	this->scale.set_scale_value(this->scale.get_scale_value() + scale_inc); /* TODO_LATER: should it be "+ scale_inc" or "* scale_inc"? */
}




sg_ret TileInfo::get_itms_lat_lon_ul_br(LatLon & lat_lon_ul, LatLon & lat_lon_br) const
{
	/*
	  Bottom-right coordinate of a tile is simply +1/+1 in iTMS
	  coords, i.e. it is coordinate of u-l corner of a next tile
	  that is +one to the right and +one to the bottom.

	  TODO_LATER: what if we are at a bottom or on the right of an x/y grid of tiles?
	*/
	TileInfo next_tile_info = *this;
	next_tile_info.x++;
	next_tile_info.y++;

	/* Reconvert back - thus getting the coordinate at the given
	   tile's *ul corner*. */
	lat_lon_ul = MapUtils::iTMS_to_lat_lon(*this);
	lat_lon_br = MapUtils::iTMS_to_lat_lon(next_tile_info); /* ul of 'next' tile is br of 'this' tile. */

	return sg_ret::ok;
}




QDebug SlavGPS::operator<<(QDebug debug, const TileInfo & tile_info)
{
	debug << "x =" << tile_info.x << ", y =" << tile_info.y << ", zoom level =" << tile_info.get_tile_zoom_level();
	return debug;
}




int TileScale::get_tile_zoom_level(void) const
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




double TileScale::to_so_called_mpp(void) const
{
	double result;

	if (this->value >= 0) {
		result = VIK_GZ(this->value);
		qDebug() << "ZZZ" << this->value << result << "pos";
	} else {
		result = 1.0/VIK_GZ(-this->value);
		qDebug() << "ZZZ" << this->value << result << "neg";
	}

	return result;
}




void TileScale::set_scale_value(int new_value)
{
	this->value = new_value;
}




int TileScale::get_scale_value(void) const
{
	return this->value;
}




void TileScale::set_scale_valid(bool new_value)
{
	this->valid = new_value;
}




bool TileScale::get_scale_valid(void) const
{
	return this->valid;
}




int TilesRange::get_tiles_count(void) const
{
	return (this->x_last - this->x_first + 1) * (this->y_last - this->y_first + 1);
}
