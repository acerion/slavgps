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
 */

#ifndef _SG_UTILS_H_
#define _SG_UTILS_H_



#include <cstdint>

#include <QString>
#include <QTemporaryFile>
#include <QAbstractButton>

#include "map_ids.h"
#include "preferences.h"
#include "coord.h"




#define TEST_BOOLEAN(str) (! ((str)[0] == '\0' || (str)[0] == '0' || (str)[0] == 'n' || (str)[0] == 'N' || (str)[0] == 'f' || (str)[0] == 'F'))




namespace SlavGPS {




	class Window;
	class Viewport;
	class Layer;
	class LayerTRW;
	class Track;
	class Trackpoint;




	typedef int GtkWidget; /* TODO: remove sooner or later. */




	QString get_speed_unit_string(SpeedUnit speed_unit);
	double convert_speed_mps_to(double speed, SpeedUnit speed_units);
	QString get_speed_string(double speed, SpeedUnit speed_unit);

	bool get_distance_unit_string(char * buf, size_t size, DistanceUnit distance_unit);
	QString get_distance_unit_string(DistanceUnit distance_unit);
	QString get_distance_string(double distance, DistanceUnit distance_unit);
	double convert_distance_meters_to(double distance, DistanceUnit distance_unit);

	QString vu_trackpoint_formatted_message(const char * format_code, Trackpoint * tp, Trackpoint * tp_prev, Track * trk, double climb);

	char * vu_get_canonical_filename(Layer * layer, const char * filename, const char * reference_file_full_path);



	char * vu_get_tz_at_location(const Coord * coord);

	void vu_setup_lat_lon_tz_lookup();
	void vu_finalize_lat_lon_tz_lookup();


	void vu_zoom_to_show_latlons(CoordMode mode, Viewport * viewport, struct LatLon maxmin[2]);
	void vu_zoom_to_show_latlons_common(CoordMode mode, Viewport * viewport, struct LatLon maxmin[2], double zoom, bool save_position);


	/* Allow comparing versions. */
	int viking_version_to_number(char const * version);


	QString file_base_name(const QString & full_path);



	class SGUtils {
	public:
		static bool is_very_first_run(void);
		static void set_auto_features_on_first_run(void);
		static void command_line(Window * window, double latitude, double longitude, int zoom_osm_level, MapTypeID cmdline_type_id);
		static bool create_temporary_file(QTemporaryFile & file, const QString & name_pattern);
		static void copy_label_menu(QAbstractButton * button);
		static QString get_time_string(time_t time, const char * format, const Coord * coord, const char * gtz);
	};




	/* Comparison class for std::maps with QString as a key. */
	struct qstring_compare {
		bool operator() (const QString & string1, const QString & string2) const { return string1 < string2; }
	};




}




#endif
