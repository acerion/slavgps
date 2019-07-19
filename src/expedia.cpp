/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2005, Evan Battaglia <viking@greentorch.org>
 *
 * Some formulas or perhaps even code derived from GPSDrive
 * GPSDrive Copyright (C) 2001-2004 Fritz Ganter <ganter@ganter.at>

 * Lat/Lon plotting functions expedia_screen_pos_to_lat_lon() and
 * expedia_lat_lon_to_screen_pos() are from GPSDrive GPSDrive
 * Copyright (C) 2001-2004 Fritz Ganter <ganter@ganter.at>
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




static bool expedia_coord_to_tile_info(const Coord & src_coord, const VikingScale & viking_scale, TileInfo & tile_info);
static sg_ret expedia_tile_info_to_center_coord(const TileInfo & src, Coord & coord);
static DownloadStatus expedia_download_tile(const TileInfo & src, const QString & dest_file_path, DownloadHandle * dl_handle);
static void * expedia_handle_init();
static void expedia_handle_cleanup(void * handle);
static int viking_scale_to_expedia_alti(double viking_scale);

static double calcR(double lat);




static DownloadOptions expedia_options;
static double Radius[181];




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
	expedia_options.follow_location = 2;
	expedia_options.file_validator_fn = map_file_validator_fn;

	VikMapsLayer_MapType map_type;

	map_type.uniq_id = MapTypeID::Expedia;
	map_type.tilesize_x = 0;
	map_type.tilesize_y = 0;
	map_type.drawmode = GisViewportDrawMode::Expedia;
	map_type.coord_to_tile_info = expedia_coord_to_tile_info;
	map_type.tile_info_to_center_coord = expedia_tile_info_to_center_coord;
	map_type.download = expedia_download_tile;
	map_type.download_handle_init = expedia_handle_init;
	map_type.download_handle_cleanup = expedia_handle_cleanup;
#ifdef FIXME_RESTORE
	maps_layer_register_type(QObject::tr("Expedia Street Maps"), MapTypeID::Expedia, &map_type);
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
int viking_scale_to_expedia_alti(double viking_scale)
{
	for (unsigned int i = 0; i < expedia_altis_count; i++) {
		if (std::fabs(expedia_altis[i] - viking_scale) / viking_scale < MPP_MARGIN_OF_ERROR) {
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




sg_ret expedia_crop(const QString & file)
{
	QPixmap orig;
	if (!orig.load(file)) {
		qDebug() << SG_PREFIX_E << "Couldn't open EXPEDIA image file (right after successful download! Please report and delete image file!)";
		return sg_ret::err;
	}

	const int width = orig.width();
	const int height = orig.height();

	/* Let's hope that implicit sharing works: http://doc.qt.io/qt-5/implicit-sharing.html */
	QPixmap cropped = orig.copy(WIDTH_BUFFER, HEIGHT_BUFFER, width - 2 * WIDTH_BUFFER, height - 2 * HEIGHT_BUFFER);
	if (!cropped.save(file)) {
		qDebug() << SG_PREFIX_W << "Couldn't save EXPEDIA image file (right after successful download! Please report and delete image file!)";
		return sg_ret::err;
	} else {
		return sg_ret::ok;
	}
}




/* If degree_freeq = 60 -> nearest minute (in middle).
   Everything starts at -90,-180 -> 0,0. then increments by (1/degree_freq). */
static bool expedia_coord_to_tile_info(const Coord & src_coord, const VikingScale & viking_scale, TileInfo & tile_info)
{
	assert (src_coord.get_coord_mode() == CoordMode::LatLon);

	if (!viking_scale.x_y_is_equal()) {
		return false;
	}

	const int alti = viking_scale_to_expedia_alti(viking_scale.get_x());
	if (alti != -1) {
		tile_info.scale.set_scale_value(alti);
		tile_info.x = (int) (((src_coord.lat_lon.lon + 180) * expedia_altis_freq(alti))+0.5);
		tile_info.y = (int) (((src_coord.lat_lon.lat + 90) * expedia_altis_freq(alti))+0.5);
		/* + 0.5 to round off and not floor. */

		/* Just to space out tiles on the filesystem. */
		tile_info.z = 0;
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




static sg_ret expedia_tile_info_to_center_coord(const TileInfo & src, Coord & coord)
{
	coord.set_coord_mode(CoordMode::LatLon); /* This function decides what will be the coord mode of returned coordinate. */
	coord.lat_lon.lon = (((double) src.x) / expedia_altis_freq(src.scale.get_scale_value())) - 180;
	coord.lat_lon.lat = (((double) src.y) / expedia_altis_freq(src.scale.get_scale_value())) - 90;
	return sg_ret::ok;
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
	DownloadStatus res = dl_handle->perform_download(EXPEDIA_SITE, uri, dest_file_path, DownloadProtocol::HTTP);
	if (res == DownloadStatus::Success) {
		expedia_crop(dest_file_path);
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




/* Thanks GPSDrive. */
bool Expedia::screen_pos_to_lat_lon(LatLon & lat_lon, int x, int y, const LatLon & lat_lon_center, double pixelfact_x, double pixelfact_y, fpixel mapSizeX2, fpixel mapSizeY2)
{
	double Ra = Radius[90 + (int) lat_lon_center.lat];

	int px = (mapSizeX2 - x) * pixelfact_x;
	int py = (-mapSizeY2 + y) * pixelfact_y;

	double lat = lat_lon_center.lat - py / Ra;
	double lon = lat_lon_center.lon - px / (Ra * cos (DEG2RAD(lat)));

	double dif = lat * (1 - (cos (DEG2RAD(std::fabs (lon - lat_lon_center.lon)))));
	lat = lat - dif / 1.5;
	lon = lat_lon_center.lon - px / (Ra * cos (DEG2RAD(lat)));

	lat_lon = LatLon(lat, lon);

	return true;
}




/* Thanks GPSDrive. */
bool Expedia::lat_lon_to_screen_pos(fpixel * pos_x, fpixel * pos_y, const LatLon & lat_lon_center, const LatLon & lat_lon, double pixelfact_x, double pixelfact_y, int mapSizeX2, int mapSizeY2)
{
	int mapSizeX = 2 * mapSizeX2;
	int mapSizeY = 2 * mapSizeY2;

	assert (lat_lon_center.lat >= SG_LATITUDE_MIN && lat_lon_center.lat <= SG_LATITUDE_MAX);
	//    lat_lon_center.lon *= rad2deg; // FIXME, optimize equations
	//    lat_lon_center.lat *= rad2deg;
	double Ra = Radius[90 + (int) lat_lon_center.lat];
	*pos_x = Ra * cos (DEG2RAD(lat_lon_center.lat)) * (lat_lon_center.lon - lat_lon.lon);
	*pos_y = Ra * (lat_lon_center.lat - lat_lon.lat);
	double dif = Ra * RAD2DEG(1 - (cos ((DEG2RAD(lat_lon_center.lon - lat_lon.lon)))));
	*pos_y = *pos_y + dif / 1.85;
	*pos_x = *pos_x / pixelfact_x;
	*pos_y = *pos_y / pixelfact_y;
	*pos_x = mapSizeX2 - *pos_x;
	*pos_y += mapSizeY2;
	if ((*pos_x < 0)||(*pos_x >= mapSizeX)||(*pos_y < 0)||(*pos_y >= mapSizeY)) {
		return false;
	}
	return true;
}




void Expedia::init_radius(void)
{
	static bool done_before = false;
	if (!done_before) {
		for (int i = (int) SG_LATITUDE_MIN; i <= (int) SG_LATITUDE_MAX; i++) {
			Radius[i + 90] = calcR(DEG2RAD((double) i));
		}
		done_before = true;
	}
}




static double calcR(double lat)
{
	/*
	 * The radius of curvature of an ellipsoidal Earth in the plane of the
	 * meridian is given by
	 *
	 * R' = a * (1 - e^2) / (1 - e^2 * (sin(lat))^2)^(3/2)
	 *
	 *
	 * where a is the equatorial radius, b is the polar radius, and e is
	 * the eccentricity of the ellipsoid = sqrt(1 - b^2/a^2)
	 *
	 * a = 6378 km (3963 mi) Equatorial radius (surface to center distance)
	 * b = 6356.752 km (3950 mi) Polar radius (surface to center distance) e
	 * = 0.081082 Eccentricity
	 */
	double a = 6378.137;
	double e2 = 0.081082 * 0.081082;
	lat = DEG2RAD(lat);
	double sc = sin (lat);
	double x = a * (1.0 - e2);
	double z = 1.0 - e2 * sc * sc;
	double y = pow (z, 1.5);
	double r = x / y;
	r = r * 1000.0;
	return r;
}
