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
#include <QHash>

#include "ui_builder.h"




namespace SlavGPS {




	class Preferences {
	public:
		Preferences() {};
		static void uninit();
		static void register_default_values();

		static void show_window(QWidget * parent = NULL);
		static bool save_to_file(void);

		/*
		  Must be called first, before calling Preferences::register_parameter().

		  \param group_key is key string in preferences file
		  \param group_name is a pretty name for presentation in UI
		*/
		static void register_group(const QString & group_key, const QString & group_name);

		/* Nothing in pref is copied neither but pref itself is copied. (TODO: COPY EVERYTHING IN PREF WE NEED, IF ANYTHING),
		   so pref key is not copied. default param data IS copied. */
		/* Group field (integer) will be overwritten. */
		/* \param param_spec should be persistent through the life of the preference. */
		static void register_parameter(ParameterSpecification * param_spec, const SGVariant & default_param_value);

		/* Set value of a single parameter - by internal id. */
		static bool set_param_value(param_id_t id, const SGVariant & value);
		/* Set value of a single parameter - by name. */
		static bool set_param_value(const char * name, const SGVariant & value);
		/* Get value of a single parameter - by internal id. */
		static SGVariant get_param_value(param_id_t id);
		/* Get value of a single parameter - by namespace/key pair. */
		static const SGVariant * get_param_value(const char * key);


		std::map<param_id_t, ParameterSpecification *>::iterator begin();
		std::map<param_id_t, ParameterSpecification *>::iterator end();

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
		static const QString get_image_viewer(void);
#endif
		static const QString get_external_gpx_program_1(void);
		static const QString get_external_gpx_program_2(void);
		static vik_file_ref_format_t get_file_ref_format();
		static bool get_ask_for_create_track_name();
		static bool get_create_track_tooltip();
		static int get_recent_number_files();
		static bool get_add_default_map_layer();
		static vik_startup_method_t get_startup_method();
		static const QString get_startup_file(void);
		static bool get_check_version();

		bool loaded = false; /* Have the preferences been loaded from file? */
		QHash<param_id_t, QString> group_names; /* Group id -> group UI label. */
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_PREFERENCES_H_ */
