/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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
 *
 */
#ifndef _SG_UTILS_H_
#define _SG_UTILS_H_



#include <cstdint>

#include <glib.h>

#include "viktrwlayer.h"
#include "vikwindow.h"
#include "map_ids.h"




namespace SlavGPS {




	char * get_speed_unit_string(SpeedUnit speed_unit);
	double convert_speed_mps_to(SpeedUnit speed_units, double speed);
	char * get_speed_string(char * buf, size_t size, SpeedUnit speed_unit, double speed);

	bool get_distance_unit_string(char * buf, size_t size, DistanceUnit distance_unit);
	char * get_distance_string(char * buf, size_t size, DistanceUnit distance_unit, double distance);
	double convert_distance_meters_to(DistanceUnit distance_unit, double distance);


	char * vu_trackpoint_formatted_message(char * format_code, Trackpoint * tp, Trackpoint * tp_prev, Track * trk, double climb);

	void vu_check_latest_version(GtkWindow * window);

	void vu_set_auto_features_on_first_run(void);

	char * vu_get_canonical_filename(Layer * layer, const char * filename);

	char * vu_get_time_string(time_t * time, const char * format, const VikCoord * vc, const char * gtz);

	char * vu_get_tz_at_location(const VikCoord * vc);

	void vu_setup_lat_lon_tz_lookup();
	void vu_finalize_lat_lon_tz_lookup();

	void vu_command_line(Window * window, double latitude, double longitude, int zoom_osm_level, MapTypeID cmdline_type_id);

	void vu_copy_label_menu(GtkWidget *widget, unsigned int button);

	void vu_zoom_to_show_latlons(VikCoordMode mode, Viewport * viewport, struct LatLon maxmin[2]);
	void vu_zoom_to_show_latlons_common(VikCoordMode mode, Viewport * viewport, struct LatLon maxmin[2], double zoom, bool save_position);




}




#endif
