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

#include "globals.h"
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
	if (ABS(mpp - t) > MARGIN_OF_ERROR) {
		return false;
	}

	switch (t) {
	case 1: return (type == MAP_ID_TERRASERVER_URBAN) ? 8 : 0;
	case 2: return (type == MAP_ID_TERRASERVER_URBAN) ? 9 : 0;
	case 4: return (type != MAP_ID_TERRASERVER_TOPO) ? 10 : 0;
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




bool MapSourceTerraserver::coord_to_tile(const Coord * src, double xmpp, double ympp, TileInfo * dest)
{
	if (src->mode != CoordMode::UTM) {
		return false;
	}

	if (xmpp != ympp) {
		return false;
	}

	dest->scale = mpp_to_scale(xmpp, this->map_type);
	if (!dest->scale) {
		return false;
	}

	dest->x = (int)(((int)(src->utm.easting))/(200*xmpp));
	dest->y = (int)(((int)(src->utm.northing))/(200*xmpp));
	dest->z = src->utm.zone;
	return true;
}




bool MapSourceTerraserver::is_direct_file_access(void)
{
	return false;
}




bool MapSourceTerraserver::is_mbtiles(void)
{
	return false;
}




void MapSourceTerraserver::tile_to_center_coord(TileInfo * src, Coord * dest)
{
	/* FIXME: slowdown here! */
	double mpp = scale_to_mpp (src->scale);
	dest->mode = CoordMode::UTM;
	dest->utm.zone = src->z;
	dest->utm.easting = ((src->x * 200) + 100) * mpp;
	dest->utm.northing = ((src->y * 200) + 100) * mpp;
}




const QString MapSourceTerraserver::get_server_path(TileInfo * src) const
{
	char * uri = g_strdup_printf("/tile.ashx?T=%d&S=%d&X=%d&Y=%d&Z=%d", (int) this->map_type, src->scale, src->x, src->y, src->z); /* kamilFIXME: memory leak. */
	return QString(uri);
}




const QString MapSourceTerraserver::get_server_hostname(void) const
{
	return QString(TERRASERVER_SITE);
}




MapSourceTerraserver::MapSourceTerraserver(MapTypeID type_, const char * label_)
{
	switch (type_) {
	case MAP_ID_TERRASERVER_AERIAL:
		this->copyright = strdup("© DigitalGlobe");
		break;
	case MAP_ID_TERRASERVER_TOPO:
		this->copyright = strdup("© LandVoyage");
		break;
	case MAP_ID_TERRASERVER_URBAN:
		this->copyright = strdup("© DigitalGlobe");
		break;
	default:
		qDebug() << "EE: Map Source Terraserver: unknown type" << (int) type_;
	}

	this->label = strdup(label_);
	this->map_type = type_;

	this->tilesize_x = 200;
	this->tilesize_y = 200;
	this->drawmode = ViewportDrawMode::UTM;

	this->dl_options.check_file = a_check_map_file;
}
