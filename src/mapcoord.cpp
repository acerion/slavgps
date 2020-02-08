/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2016-2020, Kamil Ignacak <acerion@wp.pl>
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




#define SG_MODULE "Map Tile"




TilesRange TileInfo::get_tiles_range(const TileInfo & tile_info_ul, const TileInfo & tile_info_br)
{
	TilesRange range;

	range.horiz_first_idx = std::min(tile_info_ul.x, tile_info_br.x);
	range.horiz_last_idx  = std::max(tile_info_ul.x, tile_info_br.x);
	range.vert_first_idx  = std::min(tile_info_ul.y, tile_info_br.y);
	range.vert_last_idx   = std::max(tile_info_ul.y, tile_info_br.y);

	return range;
}




void TileInfo::zoom_in(int zoom_level_delta)
{
	const TileZoomLevel before = this->osm_tile_zoom_level();

	/* At different zoom level the tiles' indexes are different,
	   so... */
	const int x_y_scale_factor = 1 << zoom_level_delta;  /* 2^zoom_level_delta */
	this->x = this->x * x_y_scale_factor;
	this->y = this->y * x_y_scale_factor;

	this->scale.set_scale_value(this->scale.get_scale_value() - zoom_level_delta);

	const TileZoomLevel after = this->osm_tile_zoom_level();
	qDebug() << SG_PREFIX_D << "Zooming in by" << zoom_level_delta << "changed OSM zoom level from" << before.value() << "to" << after.value();
}




void TileInfo::zoom_out(int zoom_level_delta)
{
	const TileZoomLevel before = this->osm_tile_zoom_level();

	/* At different zoom level the tiles' indexes are different,
	   so... */
	const int x_y_scale_factor = 1 << zoom_level_delta;  /* 2^zoom_level_delta */
	this->x = this->x / x_y_scale_factor;
	this->y = this->y / x_y_scale_factor;

	this->scale.set_scale_value(this->scale.get_scale_value() + zoom_level_delta);

	const TileZoomLevel after = this->osm_tile_zoom_level();
	qDebug() << SG_PREFIX_D << "Zooming out by" << zoom_level_delta << "changed OSM zoom level from" << before.value() << "to" << after.value();
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
	debug << "x =" << tile_info.x << ", y =" << tile_info.y << ", OSM zoom level =" << tile_info.osm_tile_zoom_level().value();
	return debug;
}




TileZoomLevel TileScale::osm_tile_zoom_level(void) const
{
	TileZoomLevel result(0);
	const int recalculated = MAGIC_SEVENTEEN - this->value;

	if (recalculated < (int) TileZoomLevel::Level::Min) {
		qDebug() << SG_PREFIX_E << "Clipping OSM Zoom Level: too small" << this->value << recalculated;
		result = TileZoomLevel(TileZoomLevel::Level::Min);

	} else if (recalculated > (int) TileZoomLevel::Level::Max) {
		qDebug() << SG_PREFIX_E << "Clipping OSM Zoom Level: too large" << this->value << recalculated;
		result = TileZoomLevel(TileZoomLevel::Level::Max);

	} else {
		result = TileZoomLevel(recalculated);
	}

	return result;
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
	return (this->horiz_last_idx - this->horiz_first_idx + 1) * (this->vert_last_idx - this->vert_first_idx + 1);
}




TilesRange TilesRange::make_ordered(const TileInfo & ref_tile) const
{
	TilesRange result;

	result.horiz_delta = (ref_tile.x == this->horiz_first_idx) ? 1 : -1;
	result.vert_delta = (ref_tile.y == this->vert_first_idx) ? 1 : -1;

	result.horiz_first_idx = (result.horiz_delta == 1) ?  this->horiz_first_idx     :  this->horiz_last_idx;
	result.vert_first_idx  = (result.vert_delta == 1)  ?  this->vert_first_idx      :  this->vert_last_idx;
	result.horiz_last_idx  = (result.horiz_delta == 1) ? (this->horiz_last_idx + 1) : (this->horiz_first_idx - 1);
	result.vert_last_idx   = (result.vert_delta == 1)  ? (this->vert_last_idx + 1)  : (this->vert_first_idx - 1);

	return result;
}
