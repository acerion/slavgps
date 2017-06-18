/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2014-2015, Rob Norris <rw_norris@hotmail.com>
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
 * See: http://www.gpsbabel.org/htmldoc-development/Data_Filters.html
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>
#include <cstdlib>

#include <glib/gi18n.h>

#include "babel.h"
#include "gpx.h"
#include "acquire.h"
#include "settings.h"
#include "globals.h"
#include "preferences.h"




using namespace SlavGPS;




/************************************ Simplify (Count) *****************************/

/* Spin button scales. */
ParameterScale simplify_params_scales[] = {
	{1, 10000, 10, 0},
};

Parameter bfilter_simplify_params[] = {
	{ (param_id_t) LayerType::NUM_TYPES, "numberofpoints", ParameterType::UINT, VIK_LAYER_GROUP_NONE, N_("Max number of points:"), WidgetType::SPINBUTTON, simplify_params_scales, NULL, NULL, NULL, NULL, NULL },
};

ParameterValue bfilter_simplify_params_defaults[] = {
	/* Annoyingly 'C' cannot initialize unions properly. */
	/* It's dependent on the standard used or the compiler support... */
#if defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L || __GNUC__
	{ .u = 100 },
#else
	{ 100 },
#endif
};




static ProcessOptions * datasource_bfilter_simplify_get_process_options(ParameterValue *paramdatas, void * not_used, const char *input_filename, const char *not_used3)
{
	ProcessOptions * po = new ProcessOptions();

	po->babelargs = strdup("-i gpx");
	po->filename = g_strdup(input_filename);
	po->babel_filters = g_strdup_printf("-x simplify,count=%d", paramdatas[0].u);

	/* Store for subsequent default use. */
	bfilter_simplify_params_defaults[0].u = paramdatas[0].u;

	return po;
}




#define VIK_SETTINGS_BFILTER_SIMPLIFY "bfilter_simplify"
static bool bfilter_simplify_default_set = false;




static void * datasource_bfilter_simplify_init(acq_vik_t *not_used)
{
	if (!bfilter_simplify_default_set) {
		int tmp;
		if (!a_settings_get_integer (VIK_SETTINGS_BFILTER_SIMPLIFY, &tmp)) {
			tmp = 100;
		}

		bfilter_simplify_params_defaults[0].u = tmp;
		bfilter_simplify_default_set = true;
	}

	return NULL;
}




VikDataSourceInterface vik_datasource_bfilter_simplify_interface = {
	N_("Simplify All Tracks..."),
	N_("Simplified Tracks"),
	DatasourceMode::CREATENEWLAYER,
	DatasourceInputtype::TRWLAYER,
	true,
	false, /* Keep dialog open after success. */
	true,

	(DataSourceInternalDialog)           NULL,
	(VikDataSourceInitFunc)              datasource_bfilter_simplify_init,
	NULL, NULL,
	(VikDataSourceGetProcessOptionsFunc) datasource_bfilter_simplify_get_process_options,
	(VikDataSourceProcessFunc)           a_babel_convert_from,
	NULL, NULL, NULL,
	(VikDataSourceOffFunc) NULL,

	bfilter_simplify_params,
	sizeof(bfilter_simplify_params)/sizeof(bfilter_simplify_params[0]),
	bfilter_simplify_params_defaults,
	NULL,
	0
};




/**************************** Compress (Simplify by Error Factor Method) *****************************/




static ParameterScale compress_spin_scales[] = { {0.0, 1.000, 0.001, 3} };

Parameter bfilter_compress_params[] = {
	//{ LayerType::NUM_TYPES, "compressmethod", ParameterType::UINT, VIK_LAYER_GROUP_NONE, N_("Simplify Method:"), WidgetType::COMBOBOX, compress_method, NULL, NULL, NULL, NULL, NULL },
	{ (param_id_t) LayerType::NUM_TYPES, "compressfactor", ParameterType::DOUBLE, VIK_LAYER_GROUP_NONE, N_("Error Factor:"), WidgetType::SPINBUTTON, compress_spin_scales, NULL, N_("Specifies the maximum allowable error that may be introduced by removing a single point by the crosstrack method. See the manual or GPSBabel Simplify Filter documentation for more detail."), NULL, NULL, NULL },
};

ParameterValue bfilter_compress_params_defaults[] = {
	/* Annoyingly 'C' cannot initialize unions properly. */
	/* It's dependent on the standard used or the compiler support... */
#if defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L || __GNUC__
	{ .d = 0.001 },
#else
	{ 0.001 },
#endif
};




/**
 * http://www.gpsbabel.org/htmldoc-development/filter_simplify.html
 */
static ProcessOptions * datasource_bfilter_compress_get_process_options(ParameterValue *paramdatas, void * not_used, const char *input_filename, const char *not_used3)
{
	ProcessOptions * po = new ProcessOptions();

	char units = Preferences::get_unit_distance() == DistanceUnit::KILOMETRES ? 'k' : ' ';
	/* I toyed with making the length,crosstrack or relative methods selectable.
	   However several things:
	   - mainly that typical values to use for the error relate to method being used - so hard to explain and then give a default sensible value in the UI.
	   - also using relative method fails when track doesn't have HDOP info - error reported to stderr - which we don't capture ATM.
	   - options make this more complicated to use - is even that useful to be allowed to change the error value?
	   NB units not applicable if relative method used - defaults to Miles when not specified. */
	po->babelargs = strdup("-i gpx");
	po->filename = g_strdup(input_filename);
	po->babel_filters = g_strdup_printf ("-x simplify,crosstrack,error=%-.5f%c", paramdatas[0].d, units);

	/* Store for subsequent default use. */
	bfilter_compress_params_defaults[0].d = paramdatas[0].d;

	return po;
}




#define VIK_SETTINGS_BFILTER_COMPRESS "bfilter_compress"
static bool bfilter_compress_default_set = false;




static void * datasource_bfilter_compress_init(acq_vik_t *not_used)
{
	if (!bfilter_compress_default_set) {
		double tmp;
		if (!a_settings_get_double (VIK_SETTINGS_BFILTER_COMPRESS, &tmp)) {
			tmp = 0.001;
		}

		bfilter_compress_params_defaults[0].d = tmp;
		bfilter_compress_default_set = true;
	}

	return NULL;
}




/**
 * Allow 'compressing' tracks/routes using the Simplify by Error Factor method.
 */
VikDataSourceInterface vik_datasource_bfilter_compress_interface = {
	N_("Compress Tracks..."),
	N_("Compressed Tracks"),
	DatasourceMode::CREATENEWLAYER,
	DatasourceInputtype::TRWLAYER,
	true,
	false, /* Close the dialog after successful operation. */
	true,

	(DataSourceInternalDialog)           NULL,
	(VikDataSourceInitFunc)              datasource_bfilter_compress_init,
	NULL, NULL,
	(VikDataSourceGetProcessOptionsFunc) datasource_bfilter_compress_get_process_options,
	(VikDataSourceProcessFunc)           a_babel_convert_from,
	NULL, NULL, NULL,
	(VikDataSourceOffFunc) NULL,

	bfilter_compress_params,
	sizeof(bfilter_compress_params)/sizeof(bfilter_compress_params[0]),
	bfilter_compress_params_defaults,
	NULL,
	0
};




/************************************ Duplicate Location ***********************************/




static ProcessOptions * datasource_bfilter_dup_get_process_options(ParameterValue *paramdatas, void * not_used, const char *input_filename, const char *not_used3)
{
	ProcessOptions * po = new ProcessOptions();

	po->babelargs = strdup("-i gpx");
	po->filename = g_strdup(input_filename);
	po->babel_filters = strdup("-x duplicate,location");

	return po;
}




VikDataSourceInterface vik_datasource_bfilter_dup_interface = {
	N_("Remove Duplicate Waypoints"),
	N_("Remove Duplicate Waypoints"),
	DatasourceMode::CREATENEWLAYER,
	DatasourceInputtype::TRWLAYER,
	true,
	false, /* Keep dialog open after success. */
	true,

	NULL,
	NULL, NULL, NULL,
	(VikDataSourceGetProcessOptionsFunc) datasource_bfilter_dup_get_process_options,
	(VikDataSourceProcessFunc)           a_babel_convert_from,
	NULL, NULL, NULL,
	(VikDataSourceOffFunc) NULL,

	NULL, 0, NULL, NULL, 0
};




/************************************ Swap Lat<->Lon ***********************************/




ParameterValue bfilter_manual_params_defaults[] = {
	{ .s = (char *) NULL },
};

Parameter bfilter_manual_params[] = {
	{ (param_id_t) LayerType::NUM_TYPES, "manual", ParameterType::STRING, VIK_LAYER_GROUP_NONE, N_("Manual filter:"), WidgetType::ENTRY, NULL, NULL, N_("Manual filter command: e.g. 'swap'."), NULL, NULL, NULL },
};




static ProcessOptions * datasource_bfilter_manual_get_process_options(ParameterValue *paramdatas, void * not_used, const char *input_filename, const char *not_used3)
{
	ProcessOptions * po = new ProcessOptions();

	po->babelargs = strdup("-i gpx");
	po->filename = g_strdup(input_filename);
	po->babel_filters = g_strconcat("-x ", paramdatas[0].s, NULL);

	return po;
}




VikDataSourceInterface vik_datasource_bfilter_manual_interface = {
	N_("Manual filter"),
	N_("Manual filter"),
	DatasourceMode::CREATENEWLAYER,
	DatasourceInputtype::TRWLAYER,
	true,
	false, /* Keep dialog open after success. */
	true,

	NULL,
	NULL, NULL, NULL,
	(VikDataSourceGetProcessOptionsFunc) datasource_bfilter_manual_get_process_options,
	(VikDataSourceProcessFunc)           a_babel_convert_from,
	NULL, NULL, NULL,
	(VikDataSourceOffFunc) NULL,

	bfilter_manual_params,
	sizeof(bfilter_manual_params)/sizeof(bfilter_manual_params[0]),
	bfilter_manual_params_defaults,
	NULL,
	0
};




/************************************ Polygon ***********************************/




/* TODO: shell_escape stuff. */
static ProcessOptions * datasource_bfilter_polygon_get_process_options(ParameterValue *paramdatas, void * not_used, const char *input_filename, const char *input_track_filename)
{
	ProcessOptions * po = new ProcessOptions();

	po->shell_command = g_strdup_printf("gpsbabel -i gpx -f %s -o arc -F - | gpsbabel -i gpx -f %s -x polygon,file=- -o gpx -F -", input_track_filename, input_filename);

	return po;
}




VikDataSourceInterface vik_datasource_bfilter_polygon_interface = {
	N_("Waypoints Inside This"),
	N_("Polygonized Layer"),
	DatasourceMode::CREATENEWLAYER,
	DatasourceInputtype::TRWLAYER_TRACK,
	true,
	false, /* Keep dialog open after success. */
	true,

	NULL,
	NULL, NULL, NULL,
	(VikDataSourceGetProcessOptionsFunc) datasource_bfilter_polygon_get_process_options,
	(VikDataSourceProcessFunc)           a_babel_convert_from,
	NULL, NULL, NULL,
	(VikDataSourceOffFunc) NULL,

	NULL,
	0,
	NULL,
	NULL,
	0
};




/************************************ Exclude Polygon ***********************************/




/* TODO: shell_escape stuff */
static ProcessOptions * datasource_bfilter_exclude_polygon_get_process_options(ParameterValue *paramdatas, void * not_used, const char *input_filename, const char *input_track_filename)
{
	ProcessOptions * po = new ProcessOptions();
	po->shell_command = g_strdup_printf("gpsbabel -i gpx -f %s -o arc -F - | gpsbabel -i gpx -f %s -x polygon,exclude,file=- -o gpx -F -", input_track_filename, input_filename);

	return po;
}




VikDataSourceInterface vik_datasource_bfilter_exclude_polygon_interface = {
	N_("Waypoints Outside This"),
	N_("Polygonzied Layer"),
	DatasourceMode::CREATENEWLAYER,
	DatasourceInputtype::TRWLAYER_TRACK,
	true,
	false, /* Keep dialog open after success. */
	true,

	NULL,
	NULL, NULL, NULL,
	(VikDataSourceGetProcessOptionsFunc) datasource_bfilter_exclude_polygon_get_process_options,
	(VikDataSourceProcessFunc)           a_babel_convert_from,
	NULL, NULL, NULL,
	(VikDataSourceOffFunc) NULL,

	NULL,
	0,
	NULL,
	NULL,
	0
};
