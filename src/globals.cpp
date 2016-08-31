/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2008, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2010-2013, Rob Norris <rw_norris@hotmail.com>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <glib/gi18n.h>

#include "globals.h"
#include "preferences.h"
#include "math.h"
#include "dir.h"




using namespace SlavGPS;




bool vik_debug = false;
bool vik_verbose = false;
bool vik_version = false;

/**
 * @version:  The string of the Viking version.
 *            This should be in the form of N.N.N.N, where the 3rd + 4th numbers are optional
 *            Often you'll want to pass in VIKING_VERSION
 *
 * Returns: a single number useful for comparison.
 */
int viking_version_to_number(char *version)
{
	/* Basic method, probably can be improved. */
	int version_number = 0;
	char** parts = g_strsplit(version, ".", 0);
	int part_num = 0;
	char *part = parts[part_num];
	/* Allow upto 4 parts to the version number. */
	while (part && part_num < 4) {
		/* Allow each part to have upto 100. */
		version_number = version_number + (atol(part) * pow(100, 3-part_num));
		part_num++;
		part = parts[part_num];
	}
	g_strfreev(parts);
	return version_number;
}




static char * params_degree_formats[] = { (char *) "DDD", (char *) "DMM", (char *) "DMS", (char *) N_("Raw"), NULL};
static char * params_units_distance[] = { (char *) N_("Kilometres"), (char *) N_("Miles"), (char *) N_("Nautical Miles"), NULL};
static char * params_units_speed[] = { (char *) "km/h", (char *) "mph", (char *) "m/s", (char *) "knots", NULL};
static char * params_units_height[] = { (char *) "Metres", (char *) "Feet", NULL};
static LayerParamScale params_scales_lat[] = { {-90.0, 90.0, 0.05, 2} };
static LayerParamScale params_scales_long[] = { {-180.0, 180.0, 0.05, 2} };
static char * params_time_ref_frame[] = { (char *) N_("Locale"), (char *) N_("World"), (char *) N_("UTC"), NULL};

static LayerParam general_prefs[] = {
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "degree_format",            LayerParamType::UINT,    VIK_LAYER_GROUP_NONE, N_("Degree format:"),            LayerWidgetType::COMBOBOX,    params_degree_formats, NULL, NULL, NULL, NULL, NULL },
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "units_distance",           LayerParamType::UINT,    VIK_LAYER_GROUP_NONE, N_("Distance units:"),           LayerWidgetType::COMBOBOX,    params_units_distance, NULL, NULL, NULL, NULL, NULL },
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "units_speed",              LayerParamType::UINT,    VIK_LAYER_GROUP_NONE, N_("Speed units:"),              LayerWidgetType::COMBOBOX,    params_units_speed,    NULL, NULL, NULL, NULL, NULL },
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "units_height",             LayerParamType::UINT,    VIK_LAYER_GROUP_NONE, N_("Height units:"),             LayerWidgetType::COMBOBOX,    params_units_height,   NULL, NULL, NULL, NULL, NULL },
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "use_large_waypoint_icons", LayerParamType::BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Use large waypoint icons:"), LayerWidgetType::CHECKBUTTON, NULL,                  NULL, NULL, NULL, NULL, NULL },
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "default_latitude",         LayerParamType::DOUBLE,  VIK_LAYER_GROUP_NONE, N_("Default latitude:"),         LayerWidgetType::SPINBUTTON,  params_scales_lat,     NULL, NULL, NULL, NULL, NULL },
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "default_longitude",        LayerParamType::DOUBLE,  VIK_LAYER_GROUP_NONE, N_("Default longitude:"),        LayerWidgetType::SPINBUTTON,  params_scales_long,    NULL, NULL, NULL, NULL, NULL },
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_NAMESPACE "time_reference_frame",     LayerParamType::UINT,    VIK_LAYER_GROUP_NONE, N_("Time Display:"),             LayerWidgetType::COMBOBOX,    params_time_ref_frame, NULL, N_("Display times according to the reference frame. Locale is the user's system setting. World is relative to the location of the object."), NULL, NULL, NULL },
};

/* External/Export Options */

static char * params_kml_export_units[] = {(char *) "Metric", (char *) "Statute", (char *) "Nautical", NULL};
static char * params_gpx_export_trk_sort[] = {(char *) N_("Alphabetical"), (char *) N_("Time"), (char *) N_("Creation"), NULL };
static char * params_gpx_export_wpt_symbols[] = {(char *) N_("Title Case"), (char *) N_("Lowercase"), NULL};

static LayerParam io_prefs[] = {
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "kml_export_units",         LayerParamType::UINT, VIK_LAYER_GROUP_NONE, N_("KML File Export Units:"), LayerWidgetType::COMBOBOX, params_kml_export_units,       NULL, NULL, NULL, NULL, NULL },
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "gpx_export_track_sort",    LayerParamType::UINT, VIK_LAYER_GROUP_NONE, N_("GPX Track Order:"),       LayerWidgetType::COMBOBOX, params_gpx_export_trk_sort,    NULL, NULL, NULL, NULL, NULL },
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "gpx_export_wpt_sym_names", LayerParamType::UINT, VIK_LAYER_GROUP_NONE, N_("GPX Waypoint Symbols:"),  LayerWidgetType::COMBOBOX, params_gpx_export_wpt_symbols, NULL, N_("Save GPX Waypoint Symbol names in the specified case. May be useful for compatibility with various devices"), NULL, NULL, NULL },
};

#ifndef WINDOWS
static LayerParam io_prefs_non_windows[] = {
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "image_viewer", LayerParamType::STRING, VIK_LAYER_GROUP_NONE, N_("Image Viewer:"), LayerWidgetType::FILEENTRY, NULL, NULL, NULL, NULL, NULL, NULL },
};
#endif

static LayerParam io_prefs_external_gpx[] = {
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "external_gpx_1", LayerParamType::STRING, VIK_LAYER_GROUP_NONE, N_("External GPX Program 1:"), LayerWidgetType::FILEENTRY, NULL, NULL, NULL, NULL, NULL, NULL },
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "external_gpx_2", LayerParamType::STRING, VIK_LAYER_GROUP_NONE, N_("External GPX Program 2:"), LayerWidgetType::FILEENTRY, NULL, NULL, NULL, NULL, NULL, NULL },
};

static char * params_vik_fileref[] = {(char *) N_("Absolute"), (char *) N_("Relative"), NULL};
static LayerParamScale params_recent_files[] = { {-1, 25, 1, 0} };

static LayerParam prefs_advanced[] = {
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_ADVANCED_NAMESPACE "save_file_reference_mode",  LayerParamType::UINT,    VIK_LAYER_GROUP_NONE, N_("Save File Reference Mode:"),           LayerWidgetType::COMBOBOX,    params_vik_fileref,  NULL, N_("When saving a Viking .vik file, this determines how the directory paths of filenames are written."), NULL, NULL, NULL },
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_ADVANCED_NAMESPACE "ask_for_create_track_name", LayerParamType::BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Ask for Name before Track Creation:"), LayerWidgetType::CHECKBUTTON, NULL,                NULL, NULL, NULL, NULL, NULL },
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_ADVANCED_NAMESPACE "create_track_tooltip",      LayerParamType::BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Show Tooltip during Track Creation:"), LayerWidgetType::CHECKBUTTON, NULL,                NULL, NULL, NULL, NULL, NULL },
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_ADVANCED_NAMESPACE "number_recent_files",       LayerParamType::INT,     VIK_LAYER_GROUP_NONE, N_("The number of recent files:"),         LayerWidgetType::SPINBUTTON,  params_recent_files, NULL, N_("Only applies to new windows or on application restart. -1 means all available files."), NULL, NULL, NULL },
};

static char * params_startup_methods[] = {(char *) N_("Home Location"), (char *) N_("Last Location"), (char *) N_("Specified File"), (char *) N_("Auto Location"), NULL};

static LayerParam startup_prefs[] = {
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_STARTUP_NAMESPACE "restore_window_state",  LayerParamType::BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Restore Window Setup:"),    LayerWidgetType::CHECKBUTTON, NULL,                   NULL, N_("Restore window size and layout"), NULL, NULL, NULL},
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_STARTUP_NAMESPACE "add_default_map_layer", LayerParamType::BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Add a Default Map Layer:"), LayerWidgetType::CHECKBUTTON, NULL,                   NULL, N_("The default map layer added is defined by the Layer Defaults. Use the menu Edit->Layer Defaults->Map... to change the map type and other values."), NULL, NULL, NULL},
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_STARTUP_NAMESPACE "startup_method",        LayerParamType::UINT,    VIK_LAYER_GROUP_NONE, N_("Startup Method:"),          LayerWidgetType::COMBOBOX,    params_startup_methods, NULL, NULL, NULL, NULL, NULL },
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_STARTUP_NAMESPACE "startup_file",          LayerParamType::STRING,  VIK_LAYER_GROUP_NONE, N_("Startup File:"),            LayerWidgetType::FILEENTRY,   NULL,                   NULL, N_("The default file to load on startup. Only applies when the startup method is set to 'Specified File'"), NULL, NULL, NULL },
	{ LayerType::NUM_TYPES, VIKING_PREFERENCES_STARTUP_NAMESPACE "check_version",         LayerParamType::BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Check For New Version:"),   LayerWidgetType::CHECKBUTTON, NULL,                   NULL, N_("Periodically check to see if a new version of Viking is available"), NULL, NULL, NULL },
};
/* End of Options static stuff. */




/**
 * Detect when Viking is run for the very first time.
 * Call this very early in the startup sequence to ensure subsequent correct results.
 * The return value is cached, since later on the test will no longer be true.
 */
bool a_vik_very_first_run()
{
	static bool vik_very_first_run_known = false;
	static bool vik_very_first_run = false;

	/* Use cached result if available. */
	if (vik_very_first_run_known) {
		return vik_very_first_run;
	}

	char * dir = get_viking_dir_no_create();
	/* NB: will need extra logic if default dir gets changed e.g. from ~/.viking to ~/.config/viking. */
	if (dir) {
		/* If directory exists - Viking has been run before. */
		vik_very_first_run = ! g_file_test(dir, G_FILE_TEST_EXISTS);
		free(dir);
	} else {
		vik_very_first_run = true;
	}
	vik_very_first_run_known = true;

	return vik_very_first_run;
}




void a_vik_preferences_init()
{
	fprintf(stderr, "DEBUG: VIKING VERSION as number: %d\n", viking_version_to_number((char *) VIKING_VERSION));

	/* Defaults for the options are setup here. */
	a_preferences_register_group(VIKING_PREFERENCES_GROUP_KEY, _("General"));

	LayerParamData tmp;
	tmp.u = VIK_DEGREE_FORMAT_DMS;
	a_preferences_register(&general_prefs[0], tmp, VIKING_PREFERENCES_GROUP_KEY);

	tmp.u = (uint32_t) DistanceUnit::KILOMETRES;
	a_preferences_register(&general_prefs[1], tmp, VIKING_PREFERENCES_GROUP_KEY);

	tmp.u = (uint32_t) SpeedUnit::KILOMETRES_PER_HOUR;
	a_preferences_register(&general_prefs[2], tmp, VIKING_PREFERENCES_GROUP_KEY);

	tmp.u = (uint32_t) HeightUnit::METRES;
	a_preferences_register(&general_prefs[3], tmp, VIKING_PREFERENCES_GROUP_KEY);

	tmp.b = true;
	a_preferences_register(&general_prefs[4], tmp, VIKING_PREFERENCES_GROUP_KEY);

	/* Maintain the default location to New York. */
	tmp.d = 40.714490;
	a_preferences_register(&general_prefs[5], tmp, VIKING_PREFERENCES_GROUP_KEY);
	tmp.d = -74.007130;
	a_preferences_register(&general_prefs[6], tmp, VIKING_PREFERENCES_GROUP_KEY);

	tmp.u = VIK_TIME_REF_LOCALE;
	a_preferences_register(&general_prefs[7], tmp, VIKING_PREFERENCES_GROUP_KEY);

	/* New Tab. */
	a_preferences_register_group(VIKING_PREFERENCES_STARTUP_GROUP_KEY, _("Startup"));

	tmp.b = false;
	a_preferences_register(&startup_prefs[0], tmp, VIKING_PREFERENCES_STARTUP_GROUP_KEY);

	tmp.b = false;
	a_preferences_register(&startup_prefs[1], tmp, VIKING_PREFERENCES_STARTUP_GROUP_KEY);

	tmp.u = VIK_STARTUP_METHOD_HOME_LOCATION;
	a_preferences_register(&startup_prefs[2], tmp, VIKING_PREFERENCES_STARTUP_GROUP_KEY);

	tmp.s = "";
	a_preferences_register(&startup_prefs[3], tmp, VIKING_PREFERENCES_STARTUP_GROUP_KEY);

	tmp.b = false;
	a_preferences_register(&startup_prefs[4], tmp, VIKING_PREFERENCES_STARTUP_GROUP_KEY);

	/* New Tab. */
	a_preferences_register_group(VIKING_PREFERENCES_IO_GROUP_KEY, _("Export/External"));

	tmp.u = VIK_KML_EXPORT_UNITS_METRIC;
	a_preferences_register(&io_prefs[0], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);

	tmp.u = VIK_GPX_EXPORT_TRK_SORT_TIME;
	a_preferences_register(&io_prefs[1], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);

	tmp.b = VIK_GPX_EXPORT_WPT_SYM_NAME_TITLECASE;
	a_preferences_register(&io_prefs[2], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);

#ifndef WINDOWS
	tmp.s = "xdg-open";
	a_preferences_register(&io_prefs_non_windows[0], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);
#endif

	/* JOSM for OSM editing around a GPX track. */
	tmp.s = "josm";
	a_preferences_register(&io_prefs_external_gpx[0], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);
	/* Add a second external program - another OSM editor by default. */
	tmp.s = "merkaartor";
	a_preferences_register(&io_prefs_external_gpx[1], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);

	/* 'Advanced' Properties. */
	a_preferences_register_group(VIKING_PREFERENCES_ADVANCED_GROUP_KEY, _("Advanced"));

	tmp.u = VIK_FILE_REF_FORMAT_ABSOLUTE;
	a_preferences_register(&prefs_advanced[0], tmp, VIKING_PREFERENCES_ADVANCED_GROUP_KEY);

	tmp.b = true;
	a_preferences_register(&prefs_advanced[1], tmp, VIKING_PREFERENCES_ADVANCED_GROUP_KEY);

	tmp.b = true;
	a_preferences_register(&prefs_advanced[2], tmp, VIKING_PREFERENCES_ADVANCED_GROUP_KEY);

	tmp.i = 10; /* Seemingly GTK's default for the number of recent files. */
	a_preferences_register(&prefs_advanced[3], tmp, VIKING_PREFERENCES_ADVANCED_GROUP_KEY);
}




vik_degree_format_t a_vik_get_degree_format()
{
	vik_degree_format_t format;
	format = (vik_degree_format_t) a_preferences_get(VIKING_PREFERENCES_NAMESPACE "degree_format")->u;
	return format;
}




DistanceUnit a_vik_get_units_distance()
{
	DistanceUnit distance_unit = (DistanceUnit) a_preferences_get(VIKING_PREFERENCES_NAMESPACE "units_distance")->u;
	return distance_unit;
}




SpeedUnit a_vik_get_units_speed()
{
	SpeedUnit unit = (SpeedUnit) a_preferences_get(VIKING_PREFERENCES_NAMESPACE "units_speed")->u;
	return unit;
}




HeightUnit a_vik_get_units_height()
{
	HeightUnit unit = (HeightUnit) a_preferences_get(VIKING_PREFERENCES_NAMESPACE "units_height")->u;
	return unit;
}




bool a_vik_get_use_large_waypoint_icons()
{
	bool use_large_waypoint_icons;
	use_large_waypoint_icons = a_preferences_get(VIKING_PREFERENCES_NAMESPACE "use_large_waypoint_icons")->b;
	return use_large_waypoint_icons;
}




double a_vik_get_default_lat()
{
	double data;
	data = a_preferences_get(VIKING_PREFERENCES_NAMESPACE "default_latitude")->d;
	return data;
}




double a_vik_get_default_long()
{
	double data;
	data = a_preferences_get(VIKING_PREFERENCES_NAMESPACE "default_longitude")->d;
	return data;
}




vik_time_ref_frame_t a_vik_get_time_ref_frame()
{
	return (vik_time_ref_frame_t) a_preferences_get(VIKING_PREFERENCES_NAMESPACE "time_reference_frame")->u;
}




/* External/Export Options. */

vik_kml_export_units_t a_vik_get_kml_export_units()
{
	vik_kml_export_units_t units;
	units = (vik_kml_export_units_t) a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "kml_export_units")->u;
	return units;
}




vik_gpx_export_trk_sort_t a_vik_get_gpx_export_trk_sort()
{
	vik_gpx_export_trk_sort_t sort;
	sort = (vik_gpx_export_trk_sort_t) a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "gpx_export_track_sort")->u;
	return sort;
}




vik_gpx_export_wpt_sym_name_t a_vik_gpx_export_wpt_sym_name()
{
	vik_gpx_export_wpt_sym_name_t val;
	val = (vik_gpx_export_wpt_sym_name_t) a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "gpx_export_wpt_sym_names")->b;
	return val;
}




#ifndef WINDOWS
const char* a_vik_get_image_viewer()
{
	return a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "image_viewer")->s;
}
#endif




const char* a_vik_get_external_gpx_program_1()
{
	return a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "external_gpx_1")->s;
}

const char* a_vik_get_external_gpx_program_2()
{
	return a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "external_gpx_2")->s;
}




/* Advanced Options */

vik_file_ref_format_t a_vik_get_file_ref_format()
{
	vik_file_ref_format_t format;
	format = (vik_file_ref_format_t) a_preferences_get(VIKING_PREFERENCES_ADVANCED_NAMESPACE "save_file_reference_mode")->u;
	return format;
}




bool a_vik_get_ask_for_create_track_name()
{
	return a_preferences_get(VIKING_PREFERENCES_ADVANCED_NAMESPACE "ask_for_create_track_name")->b;
}




bool a_vik_get_create_track_tooltip()
{
	return a_preferences_get(VIKING_PREFERENCES_ADVANCED_NAMESPACE "create_track_tooltip")->b;
}




int a_vik_get_recent_number_files()
{
	return a_preferences_get(VIKING_PREFERENCES_ADVANCED_NAMESPACE "number_recent_files")->i;
}




/* Startup Options. */

bool a_vik_get_restore_window_state()
{
	bool data;
	data = a_preferences_get(VIKING_PREFERENCES_STARTUP_NAMESPACE "restore_window_state")->b;
	return data;
}




bool a_vik_get_add_default_map_layer()
{
	bool data;
	data = a_preferences_get(VIKING_PREFERENCES_STARTUP_NAMESPACE "add_default_map_layer")->b;
	return data;
}




vik_startup_method_t a_vik_get_startup_method()
{
	vik_startup_method_t data;
	data = (vik_startup_method_t) a_preferences_get(VIKING_PREFERENCES_STARTUP_NAMESPACE "startup_method")->u;
	return data;
}




const char *a_vik_get_startup_file()
{
	return a_preferences_get(VIKING_PREFERENCES_STARTUP_NAMESPACE "startup_file")->s;
}




bool a_vik_get_check_version()
{
	return a_preferences_get(VIKING_PREFERENCES_STARTUP_NAMESPACE "check_version")->b;
}
