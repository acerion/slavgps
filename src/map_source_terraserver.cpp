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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cmath>
#include <cstdlib>

#include <QDebug>

#include "map_source_terraserver.h"
#include "coord.h"
#include "mapcoord.h"




using namespace SlavGPS;




#define TERRASERVER_SITE "msrmaps.com"
#define MARGIN_OF_ERROR 0.001




static int mpp_to_scale(double mpp, MapTypeID type)
{
	mpp *= 4;
	int t = (int) mpp;
	if (std::abs(mpp - t) > MARGIN_OF_ERROR) {
		return false;
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




bool MapSourceTerraserver::coord_to_tile(const Coord & src_coord, double xmpp, double ympp, TileInfo * dest) const
{
	if (src_coord.mode != CoordMode::UTM) {
		return false;
	}

	if (xmpp != ympp) {
		return false;
	}

	dest->scale = mpp_to_scale(xmpp, this->map_type_id);
	if (!dest->scale) {
		return false;
	}

	dest->x = (int)(((int)(src_coord.utm.easting))/(200*xmpp));
	dest->y = (int)(((int)(src_coord.utm.northing))/(200*xmpp));
	dest->z = src_coord.utm.zone;
	return true;
}




bool MapSourceTerraserver::is_direct_file_access(void) const
{
	return false;
}




bool MapSourceTerraserver::is_mbtiles(void) const
{
	return false;
}




void MapSourceTerraserver::tile_to_center_coord(TileInfo * src, Coord & dest_coord) const
{
	/* FIXME: slowdown here! */
	double mpp = scale_to_mpp(src->scale);
	dest_coord.mode = CoordMode::UTM;
	dest_coord.utm.zone = src->z;
	dest_coord.utm.easting = ((src->x * 200) + 100) * mpp;
	dest_coord.utm.northing = ((src->y * 200) + 100) * mpp;
}




const QString MapSourceTerraserver::get_server_path(TileInfo * src) const
{
	const QString uri = QString("/tile.ashx?T=%1&S=%2&X=%3&Y=%4&Z=%5").arg((int) this->map_type_id).arg(src->scale).arg(src->x).arg(src->y).arg(src->z);
	return uri;
}




const QString MapSourceTerraserver::get_server_hostname(void) const
{
	return QString(TERRASERVER_SITE);
}




MapSourceTerraserver::MapSourceTerraserver(MapTypeID new_type_id, const QString & new_label)
{
	switch (new_type_id) {
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
		qDebug() << "EE: Map Source Terraserver: unknown type" << (int) new_type_id;
	}

	this->label = new_label;
	this->map_type_id = new_type_id;

	this->tilesize_x = 200;
	this->tilesize_y = 200;
	this->drawmode = ViewportDrawMode::UTM;

	this->dl_options.check_file = a_check_map_file;
}
