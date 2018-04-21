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




#include <QWidget>
#include <QHash>




#include "globals.h"
#include "measurements.h"




/* Group for global preferences */
#define PREFERENCES_NAMESPACE_GENERAL "viking.globals"

/* Another group of global preferences,
   but in a separate section to try to keep things organized */
/* AKA Export/External Prefs */
#define PREFERENCES_NAMESPACE_IO "viking.io"

/* Group for global preferences - but 'advanced'
   User changeable but only for those that need it */
#define PREFERENCES_NAMESPACE_ADVANCED "viking.advanced"

#define PREFERENCES_NAMESPACE_STARTUP "viking.startup"




namespace SlavGPS {




	class SGVariant;
	class ParameterSpecification;




	enum class StartupMethod {
		HomeLocation,
		LastLocation,
		SpecifiedFile,
		AutoLocation,
	};




	/* Format of file paths, mainly for saving of a viking file. */
	enum class FilePathFormat {
		Absolute,
		Relative,
	};




	/* Time display format. */
	enum class SGTimeReference {
		Locale, /* User's locale. */
		World,  /* Derive the local timezone at the object's position. */
		UTC,
	};




	/* KML export preferences. */
	enum class KMLExportUnits {
		Metric,
		Statute,
		Nautical,
	};




	enum class GPXExportTrackSort {
		Alpha,
		Time,
		Creation
	};




	enum class GPXExportWptSymName {
		Titlecase,
		Lowercase,
	};




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
		static void register_parameter(const ParameterSpecification & param_spec, const SGVariant & default_param_value);

		/* Set value of a single parameter. */
		static bool set_param_value(const QString & param_name, const SGVariant & param_value);
		/* Get value of a single parameter. */
		static SGVariant get_param_value(const QString & param_name);


		QHash<QString, ParameterSpecification>::iterator begin();
		QHash<QString, ParameterSpecification>::iterator end();

		static bool get_restore_window_state(void);


		static DistanceUnit get_unit_distance(void);
		static SpeedUnit    get_unit_speed(void);
		static HeightUnit   get_unit_height(void);
		static DegreeFormat get_degree_format(void);


		static bool get_use_large_waypoint_icons();
		static double get_default_lat();
		static double get_default_lon();
		static SGTimeReference get_time_ref_frame();
		static KMLExportUnits get_kml_export_units();
		static GPXExportTrackSort get_gpx_export_trk_sort();
		static GPXExportWptSymName get_gpx_export_wpt_sym_name();
#ifndef WINDOWS
		/* Windows automatically uses the system defined viewer.
		   ATM for other systems need to specify the program to use. */
		static QString get_image_viewer(void);
#endif
		static QString get_external_gpx_program_1(void);
		static QString get_external_gpx_program_2(void);
		static FilePathFormat get_file_path_format();
		static bool get_ask_for_create_track_name();
		static bool get_create_track_tooltip();
		static int get_recent_number_files();
		static bool get_add_default_map_layer();
		static StartupMethod get_startup_method();
		static QString get_startup_file(void);
		static bool get_check_version();

		bool loaded = false; /* Have the preferences been loaded from file? */
		QHash<param_id_t, QString> group_names; /* Group id -> group UI label. */
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_PREFERENCES_H_ */
