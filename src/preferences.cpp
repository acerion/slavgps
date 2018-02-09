/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
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
 */

#include <string>
#include <map>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <sys/stat.h> /* chmod() */

#include <QDebug>

#include "layer.h"
#include "preferences.h"
#include "dir.h"
#include "file.h"
#include "util.h"
#include "globals.h"
#include "ui_builder.h"
#include "vikutils.h"




using namespace SlavGPS;




static std::vector<SGLabelID> params_degree_formats = {
	SGLabelID("DDD", (int) DegreeFormat::DDD),
	SGLabelID("DMM", (int) DegreeFormat::DMM),
	SGLabelID("DMS", (int) DegreeFormat::DMS),
	SGLabelID("Raw", (int) DegreeFormat::RAW),
};

static std::vector<SGLabelID> params_units_distance = {
	SGLabelID("Kilometres",     (int) DistanceUnit::KILOMETRES),
	SGLabelID("Miles",          (int) DistanceUnit::MILES),
	SGLabelID("Nautical Miles", (int) DistanceUnit::NAUTICAL_MILES),
};

static std::vector<SGLabelID> params_units_speed = {
	SGLabelID("km/h",  (int) SpeedUnit::KILOMETRES_PER_HOUR),
	SGLabelID("mph",   (int) SpeedUnit::MILES_PER_HOUR),
	SGLabelID("m/s",   (int) SpeedUnit::METRES_PER_SECOND),
	SGLabelID("knots", (int) SpeedUnit::KNOTS)
};

static std::vector<SGLabelID> params_units_height = {
	SGLabelID("Metres", (int) HeightUnit::METRES),
	SGLabelID("Feet",   (int) HeightUnit::FEET),
};


/* Hardwired default location is New York. */
static ParameterScale scale_lat = {  -90.0,  90.0, SGVariant(40.714490),  0.05, 2 };
static ParameterScale scale_lon = { -180.0, 180.0, SGVariant(-74.007130), 0.05, 2 };

static std::vector<SGLabelID> params_time_ref_frame = {
	SGLabelID("Locale", 0),
	SGLabelID("World",  1),
	SGLabelID("UTC",    2),
};

static ParameterSpecification general_prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_GENERAL, "degree_format",            SGVariantType::Int,     PARAMETER_GROUP_GENERIC, QObject::tr("Degree format:"),            WidgetType::COMBOBOX,        &params_degree_formats, NULL, NULL, NULL },
	{ 1, PREFERENCES_NAMESPACE_GENERAL, "units_distance",           SGVariantType::Int,     PARAMETER_GROUP_GENERIC, QObject::tr("Distance units:"),           WidgetType::COMBOBOX,        &params_units_distance, NULL, NULL, NULL },
	{ 2, PREFERENCES_NAMESPACE_GENERAL, "units_speed",              SGVariantType::Int,     PARAMETER_GROUP_GENERIC, QObject::tr("Speed units:"),              WidgetType::COMBOBOX,        &params_units_speed,    NULL, NULL, NULL },
	{ 3, PREFERENCES_NAMESPACE_GENERAL, "units_height",             SGVariantType::Int,     PARAMETER_GROUP_GENERIC, QObject::tr("Height units:"),             WidgetType::COMBOBOX,        &params_units_height,   NULL, NULL, NULL },
	{ 4, PREFERENCES_NAMESPACE_GENERAL, "use_large_waypoint_icons", SGVariantType::Boolean, PARAMETER_GROUP_GENERIC, QObject::tr("Use large waypoint icons:"), WidgetType::CHECKBUTTON,     NULL,                   NULL, NULL, NULL },
	{ 5, PREFERENCES_NAMESPACE_GENERAL, "default_latitude",         SGVariantType::Double,  PARAMETER_GROUP_GENERIC, QObject::tr("Default latitude:"),         WidgetType::SPINBOX_DOUBLE,  &scale_lat,             NULL, NULL, NULL },
	{ 6, PREFERENCES_NAMESPACE_GENERAL, "default_longitude",        SGVariantType::Double,  PARAMETER_GROUP_GENERIC, QObject::tr("Default longitude:"),        WidgetType::SPINBOX_DOUBLE,  &scale_lon,             NULL, NULL, NULL },
	{ 7, PREFERENCES_NAMESPACE_GENERAL, "time_reference_frame",     SGVariantType::Int,     PARAMETER_GROUP_GENERIC, QObject::tr("Time Display:"),             WidgetType::COMBOBOX,        &params_time_ref_frame, NULL, NULL, N_("Display times according to the reference frame. Locale is the user's system setting. World is relative to the location of the object.") },

	{ 8, NULL,                          NULL,                       SGVariantType::Empty,   PARAMETER_GROUP_GENERIC, QString(""),                              WidgetType::NONE,            NULL,                   NULL, NULL, NULL },
};

/* External/Export Options */

static std::vector<SGLabelID> params_kml_export_units = {
	SGLabelID("Metric",   0),
	SGLabelID("Statute",  1),
	SGLabelID("Nautical", 2),
};

static std::vector<SGLabelID> params_gpx_export_trk_sort = {
	SGLabelID("Alphabetical", 0),
	SGLabelID("Time",         1),
	SGLabelID("Creation",     2),
};

static std::vector<SGLabelID> params_gpx_export_wpt_symbols = {
	SGLabelID("Title Case", 0),
	SGLabelID("Lowercase",  1),
};

static ParameterSpecification io_prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_IO, "kml_export_units",         SGVariantType::Int, PARAMETER_GROUP_GENERIC,    QObject::tr("KML File Export Units:"),  WidgetType::COMBOBOX, &params_kml_export_units,       NULL, NULL, NULL },
	{ 1, PREFERENCES_NAMESPACE_IO, "gpx_export_track_sort",    SGVariantType::Int, PARAMETER_GROUP_GENERIC,    QObject::tr("GPX Track Order:"),        WidgetType::COMBOBOX, &params_gpx_export_trk_sort,    NULL, NULL, NULL },
	{ 2, PREFERENCES_NAMESPACE_IO, "gpx_export_wpt_sym_names", SGVariantType::Int, PARAMETER_GROUP_GENERIC,    QObject::tr("GPX Waypoint Symbols:"),   WidgetType::COMBOBOX, &params_gpx_export_wpt_symbols, NULL, NULL, N_("Save GPX Waypoint Symbol names in the specified case. May be useful for compatibility with various devices") },
	{ 3, NULL,                     NULL,                       SGVariantType::Empty, PARAMETER_GROUP_GENERIC,  QString(""),                            WidgetType::NONE,     NULL,                           NULL, NULL, NULL }, /* Guard. */
};

#ifndef WINDOWS
static ParameterSpecification io_prefs_non_windows[] = {
	{ 0, PREFERENCES_NAMESPACE_IO, "image_viewer", SGVariantType::String, PARAMETER_GROUP_GENERIC, QObject::tr("Image Viewer:"), WidgetType::FILEENTRY, NULL, NULL, NULL, NULL },
	{ 1, NULL,                     NULL,           SGVariantType::Empty,  PARAMETER_GROUP_GENERIC, QString(""),                  WidgetType::NONE,      NULL, NULL, NULL, NULL }, /* Guard. */
};
#endif

static ParameterSpecification io_prefs_external_gpx[] = {
	{ 0, PREFERENCES_NAMESPACE_IO, "external_gpx_1", SGVariantType::String, PARAMETER_GROUP_GENERIC, QObject::tr("External GPX Program 1:"), WidgetType::FILEENTRY, NULL, NULL, NULL, NULL },
	{ 1, PREFERENCES_NAMESPACE_IO, "external_gpx_2", SGVariantType::String, PARAMETER_GROUP_GENERIC, QObject::tr("External GPX Program 2:"), WidgetType::FILEENTRY, NULL, NULL, NULL, NULL },
	{ 2, NULL,                     NULL,             SGVariantType::Empty,  PARAMETER_GROUP_GENERIC, QString(""),                            WidgetType::NONE,      NULL, NULL, NULL, NULL }, /* Guard. */
};

static std::vector<SGLabelID> params_vik_fileref = {
	SGLabelID("Absolute", 0),
	SGLabelID("Relative", 1),
};

static ParameterScale scale_recent_files = { -1, 25, SGVariant((int32_t) 10), 1, 0 }; /* Viking's comment about value of hardwired default: "Seemingly GTK's default for the number of recent files.". */

static ParameterSpecification prefs_advanced[] = {
	{ 0, PREFERENCES_NAMESPACE_ADVANCED, "save_file_reference_mode",  SGVariantType::Int,     PARAMETER_GROUP_GENERIC, QObject::tr("Save File Reference Mode:"),           WidgetType::COMBOBOX,    &params_vik_fileref,  NULL, NULL, N_("When saving a Viking .vik file, this determines how the directory paths of filenames are written.") },
	{ 1, PREFERENCES_NAMESPACE_ADVANCED, "ask_for_create_track_name", SGVariantType::Boolean, PARAMETER_GROUP_GENERIC, QObject::tr("Ask for Name before Track Creation:"), WidgetType::CHECKBUTTON, NULL,                 NULL, NULL, NULL },
	{ 2, PREFERENCES_NAMESPACE_ADVANCED, "create_track_tooltip",      SGVariantType::Boolean, PARAMETER_GROUP_GENERIC, QObject::tr("Show Tooltip during Track Creation:"), WidgetType::CHECKBUTTON, NULL,                 NULL, NULL, NULL },
	{ 3, PREFERENCES_NAMESPACE_ADVANCED, "number_recent_files",       SGVariantType::Int,     PARAMETER_GROUP_GENERIC, QObject::tr("The number of recent files:"),         WidgetType::SPINBOX_INT, &scale_recent_files,  NULL, NULL, N_("Only applies to new windows or on application restart. -1 means all available files.") },
	{ 4, NULL,                           NULL,                        SGVariantType::Empty,   PARAMETER_GROUP_GENERIC, QString(""),                                        WidgetType::NONE,        NULL,                 NULL, NULL, NULL },  /* Guard. */
};

static std::vector<SGLabelID> params_startup_methods = {
	SGLabelID("Home Location",  0),
	SGLabelID("Last Location",  1),
	SGLabelID("Specified File", 2),
	SGLabelID("Auto Location",  3),
};

static ParameterSpecification startup_prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_STARTUP, "restore_window_state",  SGVariantType::Boolean, PARAMETER_GROUP_GENERIC, QObject::tr("Restore Window Setup:"),    WidgetType::CHECKBUTTON, NULL,                    NULL, NULL, N_("Restore window size and layout") },
	{ 1, PREFERENCES_NAMESPACE_STARTUP, "add_default_map_layer", SGVariantType::Boolean, PARAMETER_GROUP_GENERIC, QObject::tr("Add a Default Map Layer:"), WidgetType::CHECKBUTTON, NULL,                    NULL, NULL, N_("The default map layer added is defined by the Layer Defaults. Use the menu Edit->Layer Defaults->Map... to change the map type and other values.") },
	{ 2, PREFERENCES_NAMESPACE_STARTUP, "startup_method",        SGVariantType::Int,     PARAMETER_GROUP_GENERIC, QObject::tr("Startup Method:"),          WidgetType::COMBOBOX,    &params_startup_methods, NULL, NULL, NULL },
	{ 3, PREFERENCES_NAMESPACE_STARTUP, "startup_file",          SGVariantType::String,  PARAMETER_GROUP_GENERIC, QObject::tr("Startup File:"),            WidgetType::FILEENTRY,   NULL,                    NULL, NULL, N_("The default file to load on startup. Only applies when the startup method is set to 'Specified File'") },
	{ 4, PREFERENCES_NAMESPACE_STARTUP, "check_version",         SGVariantType::Boolean, PARAMETER_GROUP_GENERIC, QObject::tr("Check For New Version:"),   WidgetType::CHECKBUTTON, NULL,                    NULL, NULL, N_("Periodically check to see if a new version of Viking is available") },
	{ 5, NULL,                          NULL,                    SGVariantType::Empty,   PARAMETER_GROUP_GENERIC, QString(""),                             WidgetType::NONE,        NULL,                    NULL, NULL, NULL }, /* Guard. */
};
/* End of Options static stuff. */





static Preferences preferences;




/* TODO: STRING_LIST. */
/* TODO: share code in file reading. */
/* TODO: remove hackaround in show_window. */




#define VIKING_PREFERENCES_FILE "viking.prefs"



/* How to get a value of parameter:
   1. use id to get a parameter from registered_parameter_specifications, then
   2. use parameter->name to look up parameter's value in registered_parameter_values.
   You probably can skip right to point 2 if you know full name of parameter.

   How to set a value of parameter:
   1.
   2.
*/
static QHash<QString, ParameterSpecification> registered_parameter_specifications; /* Parameter name -> Parameter specification. */
static QHash<QString, SGVariant> registered_parameter_values; /* Parameter name -> Value of parameter. */




/************ groups *********/




static QHash<QString, param_id_t> groups_keys_to_indices;




void Preferences::register_group(const QString & group_key, const QString & group_name)
{
	static param_id_t group_id = 1;

	if (groups_keys_to_indices.contains(group_key)) {
		qDebug() << "EE: Preferences: duplicate preferences group key" << group_key;
	} else {
		preferences.group_names.insert(group_id, group_name);
		groups_keys_to_indices.insert(group_key, group_id);
		group_id++;
	}
}




/* Returns -1 if not found. */
static param_id_t preferences_group_key_to_group_id(const QString & key)
{
	int index = groups_keys_to_indices[key];
	if (0 == index) {
		/* "default-constructed value" of QT container for situations when there is no key-value pair in the container. */
		return PARAMETER_GROUP_GENERIC; /* Which should be -1 anyway. */
	}
	return (param_id_t) index;
}




/*****************************/




static bool preferences_load_from_file()
{
	const QString full_path = get_viking_dir() + QDir::separator() + VIKING_PREFERENCES_FILE;
	FILE * file = fopen(full_path.toUtf8().constData(), "r");

	if (!file) {
		return false;
	}

	char buf[4096];
	char * key = NULL;
	char * val = NULL;
	while (!feof(file)) {
		if (fgets(buf,sizeof (buf), file) == NULL) {
			break;
		}
		if (split_string_from_file_on_equals(buf, &key, &val)) {
			/* If it's not in there, ignore it. */
			auto old_val_iter = registered_parameter_values.find(key);
			if (old_val_iter == registered_parameter_values.end()) {
				qDebug() << "II: Preferences: load from file: ignoring key/val" << key << val;
				free(key);
				free(val);
				continue;
			} else {
				qDebug() << "II: Preferences: load from file: modifying key/val" << key << val;
			}


			/* Otherwise change it (you know the type!).
			   If it's a string list do some funky stuff ... yuck... not yet. */
			if (old_val_iter.value().type_id == SGVariantType::StringList) {
				qDebug() << "EE: Preferences: Load from File: 'string list' not implemented";
			}

			const SGVariant new_val = SGVariant(old_val_iter.value().type_id, val); /* String representation -> variant. */
			registered_parameter_values.insert(QString(key), new_val);

			free(key);
			free(val);
			/* Change value. */
		}
	}
	fclose(file);
	file = NULL;

	return true;
}




bool Preferences::set_param_value(const char * param_name, const SGVariant & param_value)
{
	auto iter = registered_parameter_specifications.find(QString(param_name));
	if (iter == registered_parameter_specifications.end()) {
		qDebug() << "EE: Preferences: Set Param Value: parameter" << param_name << "not found";
		return false;
	}

	const ParameterSpecification & param_spec = *iter;

	/* Don't change stored pointer values. */
	if (param_spec.type_id == SGVariantType::Pointer) {
		return false;
	}
	if (param_spec.type_id == SGVariantType::StringList) {
		qDebug() << "EE: Preferences: Set Parameter Value: 'string list' not implemented";
		return false;
	}
	if (param_value.type_id != param_spec.type_id) {
		qDebug() << "EE: Preferences: mismatch of variant type for parameter" << param_name << param_value.type_id << param_spec.type_id;
		return false;
	}

	/* [] operator will return modifiable reference.  */
	registered_parameter_values[QString(param_name)] = param_value;

	qDebug() << "II: Preferences: set new value of parameter" << param_name << "=" << param_value;

	return true;
}




bool Preferences::set_param_value(const QString & param_name, const SGVariant & param_value)
{
	return Preferences::set_param_value(param_name.toUtf8().constData(), param_value);
}




/**
 * Returns: true on success.
 */
bool Preferences::save_to_file(void)
{
	const QString full_path = get_viking_dir() + QDir::separator() + VIKING_PREFERENCES_FILE;

	FILE * file = fopen(full_path.toUtf8().constData(), "w");
	/* Since preferences files saves OSM login credentials, it'll be better to store it in secret. */
	if (chmod(full_path.toUtf8().constData(), 0600) != 0) {
		qDebug() << "WW: Preferences: failed to set permissions on" << full_path; /* TODO: shouldn't we abort saving to file? */
	}

	if (!file) {
		return false;
	}

	qDebug() << "II: Preferences: saving to file" << full_path;

	for (auto iter = registered_parameter_specifications.begin(); iter != registered_parameter_specifications.end(); iter++) {
		const ParameterSpecification & param_spec = *iter;

		const QString param_name = QString(param_spec.name_space) + "." + QString(param_spec.name);

		auto val_iter = registered_parameter_values.find(param_name);
		if (val_iter != registered_parameter_values.end()) {

			const SGVariant param_value = val_iter.value();
			if (param_value.type_id != SGVariantType::Pointer) {
				qDebug() << "II: Preferences: saving param" << param_name << "=" << param_value.type_id << param_value;
				file_write_layer_param(file, param_name.toUtf8().constData(), param_value.type_id, param_value);
			}
		}
	}
	fclose(file);
	file = NULL;

	return true;
}




void Preferences::show_window(QWidget * parent)
{
	// preferences.loaded = true;
	// preferences_load_from_file();

	PropertiesDialog dialog(QObject::tr("Preferences"), parent);
	dialog.fill(&preferences);

	if (QDialog::Accepted == dialog.exec()) {
		for (auto iter = registered_parameter_specifications.begin(); iter != registered_parameter_specifications.end(); iter++) {
			const QString param_name = iter.key();
			const ParameterSpecification & param_spec = iter.value();

			const SGVariant param_value = dialog.get_param_value(param_name, param_spec);

			qDebug() << "II: Preferences: extracted from dialog parameter" << param_name << "=" << param_value;
			Preferences::set_param_value(param_name, param_value);
		}
		Preferences::save_to_file();
	}
}




void SlavGPS::Preferences::register_parameter(const ParameterSpecification & param_spec, const SGVariant & default_value)
{
	/* In the debug message print name and name space separately, to improve debugging. */
	qDebug() << "II: Preferences: registering preference" << param_spec.name_space << param_spec.name << "=" << default_value;

	const QString key = QString(param_spec.name_space) + "." + QString(param_spec.name);

	/* All preferences should be registered before loading. */
	if (preferences.loaded) {
		qDebug() << "EE: Preferences: registering preference" << key << "after loading preferences from" << VIKING_PREFERENCES_FILE;
	}

	/* Copy value. */
        ParameterSpecification new_param_spec = ParameterSpecification(param_spec);

	if (default_value.type_id != new_param_spec.type_id) {
		qDebug() << "EE: Preferences: Register Parameter: mismatch of type id for parameter" << key << default_value.type_id << new_param_spec.type_id;
	}
	if (new_param_spec.name_space) {
		new_param_spec.group_id = preferences_group_key_to_group_id(QString(new_param_spec.name_space));
	}

	registered_parameter_specifications.insert(key, new_param_spec);
	registered_parameter_values.insert(key, default_value);
}




void Preferences::uninit()
{
	registered_parameter_specifications.clear();
	registered_parameter_values.clear();
}




SGVariant Preferences::get_param_value(const char * param_name)
{
	SGVariant empty; /* Will have type id == Empty */

	if (!preferences.loaded) {
		qDebug() << "DD: Preferences: calling _get() for the first time (param name is" << param_name << ")";

		/* Since we can't load the file in Preferences::init() (no params registered yet),
		   do it once before we get the first param name. */
		preferences_load_from_file();
		preferences.loaded = true;
	}

	auto val_iter = registered_parameter_values.find(QString(param_name));
	if (val_iter == registered_parameter_values.end()) {
		return empty;
	} else {
		return val_iter.value();
	}
}




SGVariant Preferences::get_param_value(const QString & param_name)
{
	SGVariant empty; /* Will have type id == Empty */

	if (!preferences.loaded) {
		qDebug() << "DD: Preferences: calling _get() for the first time (param name is" << param_name << ")";

		/* Since we can't load the file in Preferences::init() (no params registered yet),
		   do it once before we get the first param name. */
		preferences_load_from_file();
		preferences.loaded = true;
	}

	auto val_iter = registered_parameter_values.find(param_name);
	if (val_iter == registered_parameter_values.end()) {
		return empty;
	} else {
		return val_iter.value();
	}
}




QHash<QString, ParameterSpecification>::iterator Preferences::begin()
{
	return registered_parameter_specifications.begin();
}




QHash<QString, ParameterSpecification>::iterator Preferences::end()
{
	return registered_parameter_specifications.end();
}




bool Preferences::get_restore_window_state(void)
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_STARTUP ".restore_window_state").val_bool;
}




/* Defaults for preferences are setup here. */
void Preferences::register_default_values()
{
	qDebug() << "DD: Preferences: VIKING VERSION as number:" << viking_version_to_number(VIKING_VERSION);

	int i = 0;

	/* New tab. */
	Preferences::register_group(PREFERENCES_NAMESPACE_GENERAL, QObject::tr("General"));

	i = 0;
	Preferences::register_parameter(general_prefs[i], SGVariant((int32_t) DegreeFormat::DMS, general_prefs[i].type_id));
	i++;
	Preferences::register_parameter(general_prefs[i], SGVariant((int32_t) DistanceUnit::KILOMETRES, general_prefs[i].type_id));
	i++;
	Preferences::register_parameter(general_prefs[i], SGVariant((int32_t) SpeedUnit::KILOMETRES_PER_HOUR, general_prefs[i].type_id));
	i++;
	Preferences::register_parameter(general_prefs[i], SGVariant((int32_t) HeightUnit::METRES, general_prefs[i].type_id));
	i++;
	Preferences::register_parameter(general_prefs[i], SGVariant(true, general_prefs[i].type_id));
	i++;
	Preferences::register_parameter(general_prefs[i], scale_lat.initial);
	i++;
	Preferences::register_parameter(general_prefs[i], scale_lon.initial);
	i++;
	Preferences::register_parameter(general_prefs[i], SGVariant((int32_t) VIK_TIME_REF_LOCALE, general_prefs[i].type_id));


	/* New Tab. */
	Preferences::register_group(PREFERENCES_NAMESPACE_STARTUP, QObject::tr("Startup"));

	i = 0;
	Preferences::register_parameter(startup_prefs[i], SGVariant(false, startup_prefs[i].type_id));
	i++;
	Preferences::register_parameter(startup_prefs[i], SGVariant(false, startup_prefs[i].type_id));
	i++;
	Preferences::register_parameter(startup_prefs[i], SGVariant((int32_t) VIK_STARTUP_METHOD_HOME_LOCATION, startup_prefs[i].type_id));
	i++;
	Preferences::register_parameter(startup_prefs[i], SGVariant("", startup_prefs[i].type_id));
	i++;
	Preferences::register_parameter(startup_prefs[i], SGVariant(false, startup_prefs[i].type_id));


	/* New Tab. */
	Preferences::register_group(PREFERENCES_NAMESPACE_IO, QObject::tr("Export/External"));

	i = 0;
	Preferences::register_parameter(io_prefs[i], SGVariant((int32_t) VIK_KML_EXPORT_UNITS_METRIC, io_prefs[i].type_id));
	i++;
	Preferences::register_parameter(io_prefs[i], SGVariant((int32_t) VIK_GPX_EXPORT_TRK_SORT_TIME, io_prefs[i].type_id));
	i++;
	Preferences::register_parameter(io_prefs[i], SGVariant((int32_t) VIK_GPX_EXPORT_WPT_SYM_NAME_TITLECASE, io_prefs[i].type_id));
	i++;

#ifndef WINDOWS
	Preferences::register_parameter(io_prefs_non_windows[0], SGVariant("xdg-open", io_prefs_non_windows[0].type_id));
#endif

	i = 0;
	/* JOSM for OSM editing around a GPX track. */
	Preferences::register_parameter(io_prefs_external_gpx[i], SGVariant("josm", io_prefs_external_gpx[i].type_id));
	i++;
	/* Add a second external program - another OSM editor by default. */
	Preferences::register_parameter(io_prefs_external_gpx[i], SGVariant("merkaartor", io_prefs_external_gpx[i].type_id));
	i++;


	/* New tab. */
	Preferences::register_group(PREFERENCES_NAMESPACE_ADVANCED, QObject::tr("Advanced"));

	i = 0;
	Preferences::register_parameter(prefs_advanced[i], SGVariant((int32_t) VIK_FILE_REF_FORMAT_ABSOLUTE, prefs_advanced[i].type_id));
	i++;
	Preferences::register_parameter(prefs_advanced[i], SGVariant(true, prefs_advanced[i].type_id));
	i++;
	Preferences::register_parameter(prefs_advanced[i], SGVariant(true, prefs_advanced[i].type_id));
	i++;
	Preferences::register_parameter(prefs_advanced[i], scale_recent_files.initial);
	i++;
}




DegreeFormat Preferences::get_degree_format(void)
{
	return (DegreeFormat) Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL ".degree_format").val_int;
}




DistanceUnit Preferences::get_unit_distance()
{
	return (DistanceUnit) Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL ".units_distance").val_int;
}




SpeedUnit Preferences::get_unit_speed(void)
{
	return (SpeedUnit) Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL ".units_speed").val_int;
}




HeightUnit Preferences::get_unit_height(void)
{
	return (HeightUnit) Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL ".units_height").val_int;
}


//

bool Preferences::get_use_large_waypoint_icons()
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL ".use_large_waypoint_icons").val_bool;
}




double Preferences::get_default_lat()
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL ".default_latitude").val_double;
}




double Preferences::get_default_lon()
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL ".default_longitude").val_double;
}




vik_time_ref_frame_t Preferences::get_time_ref_frame()
{
	return (vik_time_ref_frame_t) Preferences::get_param_value(PREFERENCES_NAMESPACE_GENERAL ".time_reference_frame").val_int;
}




/* External/Export Options. */

vik_kml_export_units_t Preferences::get_kml_export_units()
{
	return (vik_kml_export_units_t) Preferences::get_param_value(PREFERENCES_NAMESPACE_IO ".kml_export_units").val_int;
}




vik_gpx_export_trk_sort_t Preferences::get_gpx_export_trk_sort()
{
	return (vik_gpx_export_trk_sort_t) Preferences::get_param_value(PREFERENCES_NAMESPACE_IO ".gpx_export_track_sort").val_int;
}




vik_gpx_export_wpt_sym_name_t Preferences::get_gpx_export_wpt_sym_name()
{
	return (vik_gpx_export_wpt_sym_name_t) Preferences::get_param_value(PREFERENCES_NAMESPACE_IO ".gpx_export_wpt_sym_names").val_int;
}




#ifndef WINDOWS
QString Preferences::get_image_viewer(void)
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_IO ".image_viewer").val_string;
}
#endif




QString Preferences::get_external_gpx_program_1(void)
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_IO ".external_gpx_1").val_string;
}

QString Preferences::get_external_gpx_program_2(void)
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_IO ".external_gpx_2").val_string;
}




/* Advanced Options */

vik_file_ref_format_t Preferences::get_file_ref_format()
{
	return (vik_file_ref_format_t) Preferences::get_param_value(PREFERENCES_NAMESPACE_ADVANCED ".save_file_reference_mode").val_uint;
}




bool Preferences::get_ask_for_create_track_name()
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_ADVANCED ".ask_for_create_track_name").val_bool;
}




bool Preferences::get_create_track_tooltip()
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_ADVANCED ".create_track_tooltip").val_bool;
}




int Preferences::get_recent_number_files()
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_ADVANCED ".number_recent_files").val_int;
}




bool Preferences::get_add_default_map_layer()
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_STARTUP ".add_default_map_layer").val_bool;
}




vik_startup_method_t Preferences::get_startup_method()
{
	return (vik_startup_method_t) Preferences::get_param_value(PREFERENCES_NAMESPACE_STARTUP ".startup_method").val_uint;
}




QString Preferences::get_startup_file(void)
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_STARTUP ".startup_file").val_string;
}




bool Preferences::get_check_version()
{
	return Preferences::get_param_value(PREFERENCES_NAMESPACE_STARTUP ".check_version").val_bool;
}
