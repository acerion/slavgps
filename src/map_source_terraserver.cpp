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




#include <cmath>
#include <cstdlib>




#include <QDebug>




#include "map_source_terraserver.h"
#include "coord.h"
#include "mapcoord.h"
#include "viewport_internal.h"
#include "viewport_zoom.h"
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "Map Source Terraserver"
#define TERRASERVER_SITE "msrmaps.com"
#define MARGIN_OF_ERROR 0.001




static int mpp_to_scale(double mpp, MapTypeID type)
{
	mpp *= 4;
	int t = (int) mpp;
	if (std::abs(mpp - t) > MARGIN_OF_ERROR) {
		return 0;
	}

	switch (t) {
	case 1: return (type == MapTypeID::TerraserverUrban) ? 8 : 0;
	case 2: return (type == MapTypeID::TerraserverUrban) ? 9 : 0;
	case 4: return (type != MapTypeID::TerraserverTopo) ? 10 : 0;
	case 8: return 11;
	case 16: return 12;
	case 32: return 13;
	case 64: return 14;
	case 128: return 15;
	case 256: return 16;
	case 512: return 17;
	case 1024: return 18;
	case 2048: return 19;
	default: return 0;
	}
}




static double scale_to_mpp(int scale)
{
	return pow(2, scale - 10);
}




bool MapSourceTerraserver::coord_to_tile_info(const Coord & src_coord, const VikingScale & viking_scale, TileInfo & tile_info) const
{
	if (src_coord.get_coord_mode() != CoordMode::UTM) {
		return false;
	}

	if (!viking_scale.x_y_is_equal()) {
		return false;
	}

	const double xmpp = viking_scale.get_x();
	const double ympp = viking_scale.get_y();

	tile_info.scale.set_scale_value(mpp_to_scale(xmpp, this->m_map_type_id));
	if (0 == tile_info.scale.get_scale_value()) {
		return false;
	}

	tile_info.x = (int)(((int)(src_coord.utm.get_easting()))/(200 * xmpp));
	tile_info.y = (int)(((int)(src_coord.utm.get_northing()))/(200 * ympp));
	tile_info.z = src_coord.utm.get_zone();
	return true;
}




sg_ret MapSourceTerraserver::tile_info_to_center_coord(const TileInfo & src, Coord & coord) const
{
	/* TODO_LATER: slowdown here! */
	const double mpp = scale_to_mpp(src.scale.get_scale_value());
	coord.set_coord_mode(CoordMode::UTM); /* This function decides what will be the coord mode of returned coordinate. */
	coord.utm.set_zone(src.z);
	coord.utm.set_easting(((src.x * 200) + 100) * mpp);
	coord.utm.set_northing(((src.y * 200) + 100) * mpp);

	return sg_ret::ok;
}




const QString MapSourceTerraserver::get_server_path(const TileInfo & src) const
{
	const QString uri = QString("/tile.ashx?T=%1&S=%2&X=%3&Y=%4&Z=%5").arg((int) this->m_map_type_id).arg(src.scale.get_scale_value()).arg(src.x).arg(src.y).arg(src.z);
	return uri;
}




const QString MapSourceTerraserver::get_server_hostname(void) const
{
	return QString(TERRASERVER_SITE);
}




MapSourceTerraserver::MapSourceTerraserver(MapTypeID map_type_id, const QString & new_label)
{
	switch (map_type_id) {
	case MapTypeID::TerraserverAerial:
		this->copyright = "© DigitalGlobe";
		break;
	case MapTypeID::TerraserverTopo:
		this->copyright = "© LandVoyage";
		break;
	case MapTypeID::TerraserverUrban:
		this->copyright = "© DigitalGlobe";
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unknown map type" << (int) map_type_id;
	}

	this->label = new_label;
	this->m_map_type_id = map_type_id;

	this->tilesize_x = 200;
	this->tilesize_y = 200;
	this->drawmode = GisViewportDrawMode::UTM;

	this->dl_options.file_validator_fn = map_file_validator_fn;

	this->is_direct_file_access_flag = false;

	this->coord_mode = CoordMode::UTM;
}
