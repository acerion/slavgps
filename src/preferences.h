/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_PREFERENCES_H_
#define _SG_PREFERENCES_H_




#include <cstdint>
#include <map>

#include <QWidget>

#include "uibuilder.h"




namespace SlavGPS {




	class Preferences {
	public:
		static void init();
		static void uninit();
		static void register_default_values();

		void set_param_value(param_id_t id, SGVariant value);
		SGVariant get_param_value(param_id_t id);

		std::map<param_id_t, Parameter *>::iterator begin();
		std::map<param_id_t, Parameter *>::iterator end();

		static bool get_restore_window_state(void);


		static DistanceUnit get_unit_distance(void);
		static SpeedUnit    get_unit_speed(void);
		static HeightUnit   get_unit_height(void);
		static DegreeFormat get_degree_format(void);


		static bool get_use_large_waypoint_icons();
		static double get_default_lat();
		static double get_default_lon();
		static vik_time_ref_frame_t get_time_ref_frame();
		static vik_kml_export_units_t get_kml_export_units();
		static vik_gpx_export_trk_sort_t get_gpx_export_trk_sort();
		static vik_gpx_export_wpt_sym_name_t get_gpx_export_wpt_sym_name();
#ifndef WINDOWS
		/* Windows automatically uses the system defined viewer.
		   ATM for other systems need to specify the program to use. */
		static char const * get_image_viewer();
#endif
		static char const * get_external_gpx_program_1();
		static char const * get_external_gpx_program_2();
		static vik_file_ref_format_t get_file_ref_format();
		static bool get_ask_for_create_track_name();
		static bool get_create_track_tooltip();
		static int get_recent_number_files();
		static bool get_add_default_map_layer();
		static vik_startup_method_t get_startup_method();
		static char const * get_startup_file();
		static bool get_check_version();

	};




}




/* TODO IMPORTANT!!!! add REGISTER_GROUP !!! OR SOMETHING!!! CURRENTLY GROUPLESS!!! */




/* Pref should be persistent thruout the life of the preference. */


/* Must call FIRST. */
void a_preferences_register_group(const char * key, const char * name);

/* Nothing in pref is copied neither but pref itself is copied. (TODO: COPY EVERYTHING IN PREF WE NEED, IF ANYTHING),
   so pref key is not copied. default param data IS copied. */
/* Group field (integer) will be overwritten. */
void a_preferences_register(Parameter * parameter, SlavGPS::SGVariant default_value, const char * group_key);

void preferences_show_window(QWidget * parent = NULL);

SlavGPS::SGVariant * a_preferences_get(const char * key);

/* Allow preferences to be manipulated externally. */
void a_preferences_run_setparam(SlavGPS::SGVariant value, ParameterScale * parameters);

bool a_preferences_save_to_file();




#endif
