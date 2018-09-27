/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2005, Evan Battaglia <viking@greentorch.org>
 *
 * Some formulas or perhaps even code derived from GPSDrive
 * GPSDrive Copyright (C) 2001-2004 Fritz Ganter <ganter@ganter.at>
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




#include <cmath>
#include <cstdlib>
#include <cassert>




#include "coords.h"
#include "coord.h"
#include "mapcoord.h"
#include "download.h"
#include "layer_map.h"
#include "expedia.h"
#include "globals.h"
#include "vikmapslayer_compat.h"




using namespace SlavGPS;




#define SG_MODULE "Expedia"




static bool expedia_coord_to_tile(const Coord & src_coord, double xzoom, double yzoom, TileInfo & dest);
static void expedia_tile_to_center_coord(const TileInfo & src, Coord & dest_coord);
static DownloadStatus expedia_download_tile(const TileInfo & src, char const * dest_fn, DownloadHandle * dl_handle);
static void * expedia_handle_init();
static void expedia_handle_cleanup(void * handle);



#ifdef TODO_2_LATER
static DownloadOptions expedia_options = { false, false, NULL, 2, a_check_map_file, NULL };
#else
static DownloadOptions expedia_options;
#endif




#define EXPEDIA_SITE "expedia.com"
#define MPP_MARGIN_OF_ERROR 0.01
#define DEGREES_TO_RADS 0.0174532925
#define HEIGHT_OF_LAT_DEGREE (111318.84502/ALTI_TO_MPP)
#define HEIGHT_OF_LAT_MINUTE (1855.3140837/ALTI_TO_MPP)
#define WIDTH_BUFFER 0
#define HEIGHT_BUFFER 25
#define REAL_WIDTH_BUFFER 1
#define REAL_HEIGHT_BUFFER 26




/* First buffer is to cut off the expedia/microsoft logo. Annoying little buggers ;) */
/* Second is to allow for a 1-pixel overlap on each side. this is a good thing (tm). */

static const int expedia_altis[]              = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 };
/* square this number to find out how many per square degree. */
static const double expedia_altis_degree_freq[]  = { 120, 60, 30, 15, 8, 4, 2, 1, 1, 1 };
static const unsigned int expedia_altis_count = sizeof(expedia_altis) / sizeof(expedia_altis[0]);




void Expedia::init(void)
{
#ifdef VIK_CONFIG_EXPEDIA
#ifdef TODO_2_LATER
	VikMapsLayer_MapType map_type = { MapTypeID::Expedia, 0, 0, ViewportDrawMode::EXPEDIA, expedia_coord_to_tile, expedia_tile_to_center_coord, expedia_download_tile, expedia_handle_init, expedia_handle_cleanup };
	maps_layer_register_type(QObject::tr("Expedia Street Maps"), MapTypeID::Expedia, &map_type);
#else
	VikMapsLayer_MapType map_type;
#endif
#endif
}




double expedia_altis_freq(int alti)
{
	for (unsigned int i = 0; i < expedia_altis_count; i++) {
		if (expedia_altis[i] == alti) {
			return expedia_altis_degree_freq[i];
		}
	}

	qDebug() << QObject::tr("ERROR: Invalid expedia altitude");
	return 0;
}




/* Returns -1 if none of the above. */
int expedia_zoom_to_alti(double viking_zoom_level)
{
	for (unsigned int i = 0; i < expedia_altis_count; i++) {
		if (fabs(expedia_altis[i] - viking_zoom_level) / viking_zoom_level < MPP_MARGIN_OF_ERROR) {
			return expedia_altis[i];
		}
	}
	return -1;
}




/*
int expedia_pseudo_zone(int alti, int x, int y)
{
	return (int) (x / expedia_altis_freq(alti) * 180) + (int) (y / expedia_altis_freq(alti) * 90);
}
*/




void expedia_snip(const QString & file)
{
	/* Load the pixbuf. */
	QPixmap old;
	if (!old.load(file)) {
		qDebug() << "WW: Expedia: Couldn't open EXPEDIA image file (right after successful download! Please report and delete image file!)";
		return;
	}

	int width = old.width();
	int height = old.height();

	QPixmap * cropped = NULL;
#ifdef TODO_2_LATER
	cropped = gdk_pixbuf_new_subpixbuf(old, WIDTH_BUFFER, HEIGHT_BUFFER,
					   width - 2 * WIDTH_BUFFER, height - 2 * HEIGHT_BUFFER);
#endif

	if (!cropped->save(file)) {
		qDebug() << "WW: Expedia: Couldn't save EXPEDIA image file (right after successful download! Please report and delete image file!)";
	}
	delete cropped;
}




/* If degree_freeq = 60 -> nearest minute (in middle).
   Everything starts at -90,-180 -> 0,0. then increments by (1/degree_freq). */
static bool expedia_coord_to_tile(const Coord & src_coord, const VikingZoomLevel & viking_zoom_level, TileInfo & dest)
{
	assert (src_coord.mode == CoordMode::LATLON);

	if (!viking_zoom_level.x_y_is_equal()) {
		return false;
	}

	int alti = expedia_zoom_to_alti(viking_zoom_level.get_x());
	if (alti != -1) {
		dest.scale.set_scale_value(alti);
		dest.x = (int) (((src_coord.ll.lon + 180) * expedia_altis_freq(alti))+0.5);
		dest.y = (int) (((src_coord.ll.lat + 90) * expedia_altis_freq(alti))+0.5);
		/* + 0.5 to round off and not floor. */

		/* Just to space out tiles on the filesystem. */
		dest.z = 0;
		return true;
	}
	return false;
}




LatLon expedia_xy_to_latlon_middle(int alti, int x, int y)
{
	LatLon lat_lon;

	lat_lon.lon = (((double) x) / expedia_altis_freq(alti)) - 180;
	lat_lon.lat = (((double) y) / expedia_altis_freq(alti)) - 90;

	return lat_lon;
}




static void expedia_tile_to_center_coord(const TileInfo & src, Coord & dest_coord)
{
	dest_coord.mode = CoordMode::LATLON;
	dest_coord.ll.lon = (((double) src.x) / expedia_altis_freq(src.scale.get_scale_value())) - 180;
	dest_coord.ll.lat = (((double) src.y) / expedia_altis_freq(src.scale.get_scale_value())) - 90;
}




static DownloadStatus expedia_download_tile(const TileInfo & src, const QString & dest_file_path, DownloadHandle * dl_handle)
{
	const LatLon lat_lon = expedia_xy_to_latlon_middle(src.scale.get_scale_value(), src.x, src.y);

	int height = HEIGHT_OF_LAT_DEGREE / expedia_altis_freq(src.scale.get_scale_value()) / (src.scale.get_scale_value());
	int width = height * cos(lat_lon.lat * DEGREES_TO_RADS);

	height += 2 * REAL_HEIGHT_BUFFER;
	width  += 2 * REAL_WIDTH_BUFFER;

	const QString uri = QString("/pub/agent.dll?qscr=mrdt&ID=3XNsF.&CenP=%1,%2&Lang=%3&Alti=%4&Size=%5,%6&Offs=0.000000,0.000000&BCheck&tpid=1")
		.arg(lat_lon.lat)
		.arg(lat_lon.lon)
		.arg((lat_lon.lon > -30) ? "EUR0809" : "USA0409")
		.arg(src.scale.get_scale_value()) /* This should be an integer value. TODO_2_LATER: verify whether we get correct member of scale object. */
		.arg(width)
		.arg(height);

	dl_handle->set_options(expedia_options);
	DownloadStatus res = dl_handle->get_url_http(EXPEDIA_SITE, uri, dest_file_path);
	if (res == DownloadStatus::Success) {
		expedia_snip(dest_file_path);
	}
	return res;
}




static void * expedia_handle_init()
{
	/* Not much going on here. */
	return 0;
}




static void expedia_handle_cleanup(void * handle)
{
	/* Even less here! */
}
