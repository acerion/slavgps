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
#include <string.h>
#include <stdlib.h>
#include <glib/gi18n.h>

#include "viking.h"
#include "babel.h"
#include "gpx.h"
#include "acquire.h"
#include "settings.h"
#include "globals.h"





using namespace SlavGPS;





/************************************ Simplify (Count) *****************************/

/* spin button scales */
VikLayerParamScale simplify_params_scales[] = {
	{1, 10000, 10, 0},
};

VikLayerParam bfilter_simplify_params[] = {
	{ LayerType::NUM_TYPES, "numberofpoints", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Max number of points:"), VIK_LAYER_WIDGET_SPINBUTTON, simplify_params_scales, NULL, NULL, NULL, NULL, NULL },
};

VikLayerParamData bfilter_simplify_params_defaults[] = {
	/* Annoyingly 'C' cannot initialize unions properly */
	/* It's dependent on the standard used or the compiler support... */
#if defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L || __GNUC__
	{ .u = 100 },
#else
	{ 100 },
#endif
};

static void datasource_bfilter_simplify_get_process_options(VikLayerParamData *paramdatas, ProcessOptions *po, void * not_used, const char *input_filename, const char *not_used3)
{
	po->babelargs = strdup("-i gpx");
	po->filename = g_strdup(input_filename);
	po->babel_filters = g_strdup_printf("-x simplify,count=%d", paramdatas[0].u);

	// Store for subsequent default use
	bfilter_simplify_params_defaults[0].u = paramdatas[0].u;
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
	VIK_DATASOURCE_CREATENEWLAYER,
	VIK_DATASOURCE_INPUTTYPE_TRWLAYER,
	true,
	false, /* keep dialog open after success */
	true,
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

static VikLayerParamScale compress_spin_scales[] = { {0.0, 1.000, 0.001, 3} };

VikLayerParam bfilter_compress_params[] = {
	//{ LayerType::NUM_TYPES, "compressmethod", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Simplify Method:"), VIK_LAYER_WIDGET_COMBOBOX, compress_method, NULL, NULL, NULL, NULL, NULL },
	{ LayerType::NUM_TYPES, "compressfactor", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_GROUP_NONE, N_("Error Factor:"), VIK_LAYER_WIDGET_SPINBUTTON, compress_spin_scales, NULL,
	  N_("Specifies the maximum allowable error that may be introduced by removing a single point by the crosstrack method. See the manual or GPSBabel Simplify Filter documentation for more detail."), NULL, NULL, NULL },
};

VikLayerParamData bfilter_compress_params_defaults[] = {
	/* Annoyingly 'C' cannot initialize unions properly */
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
static void datasource_bfilter_compress_get_process_options(VikLayerParamData *paramdatas, ProcessOptions *po, void * not_used, const char *input_filename, const char *not_used3)
{
	char units = a_vik_get_units_distance() == DistanceUnit::KILOMETRES ? 'k' : ' ';
	// I toyed with making the length,crosstrack or relative methods selectable
	// However several things:
	// - mainly that typical values to use for the error relate to method being used - so hard to explain and then give a default sensible value in the UI
	// - also using relative method fails when track doesn't have HDOP info - error reported to stderr - which we don't capture ATM
	// - options make this more complicated to use - is even that useful to be allowed to change the error value?
	// NB units not applicable if relative method used - defaults to Miles when not specified
	po->babelargs = strdup("-i gpx");
	po->filename = g_strdup(input_filename);
	po->babel_filters = g_strdup_printf ("-x simplify,crosstrack,error=%-.5f%c", paramdatas[0].d, units);

	// Store for subsequent default use
	bfilter_compress_params_defaults[0].d = paramdatas[0].d;
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
 * Allow 'compressing' tracks/routes using the Simplify by Error Factor method
 */
VikDataSourceInterface vik_datasource_bfilter_compress_interface = {
	N_("Compress Tracks..."),
	N_("Compressed Tracks"),
	VIK_DATASOURCE_CREATENEWLAYER,
	VIK_DATASOURCE_INPUTTYPE_TRWLAYER,
	true,
	false, // Close the dialog after successful operation
	true,
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

static void datasource_bfilter_dup_get_process_options(VikLayerParamData *paramdatas, ProcessOptions *po, void * not_used, const char *input_filename, const char *not_used3)
{
	po->babelargs = strdup("-i gpx");
	po->filename = g_strdup(input_filename);
	po->babel_filters = strdup("-x duplicate,location");
}

VikDataSourceInterface vik_datasource_bfilter_dup_interface = {
	N_("Remove Duplicate Waypoints"),
	N_("Remove Duplicate Waypoints"),
	VIK_DATASOURCE_CREATENEWLAYER,
	VIK_DATASOURCE_INPUTTYPE_TRWLAYER,
	true,
	false, /* keep dialog open after success */
	true,
	NULL, NULL, NULL,
	(VikDataSourceGetProcessOptionsFunc) datasource_bfilter_dup_get_process_options,
	(VikDataSourceProcessFunc)           a_babel_convert_from,
	NULL, NULL, NULL,
	(VikDataSourceOffFunc) NULL,

	NULL, 0, NULL, NULL, 0
};


/************************************ Swap Lat<->Lon ***********************************/

VikLayerParamData bfilter_manual_params_defaults[] = {
	{ .s = NULL },
};

VikLayerParam bfilter_manual_params[] = {
	{ LayerType::NUM_TYPES, "manual", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("Manual filter:"), VIK_LAYER_WIDGET_ENTRY, NULL, NULL,
	  N_("Manual filter command: e.g. 'swap'."), NULL, NULL, NULL },
};

static void datasource_bfilter_manual_get_process_options(VikLayerParamData *paramdatas, ProcessOptions *po, void * not_used, const char *input_filename, const char *not_used3)
{
	po->babelargs = strdup("-i gpx");
	po->filename = g_strdup(input_filename);
	po->babel_filters = g_strconcat("-x ", paramdatas[0].s, NULL);
}

VikDataSourceInterface vik_datasource_bfilter_manual_interface = {
	N_("Manual filter"),
	N_("Manual filter"),
	VIK_DATASOURCE_CREATENEWLAYER,
	VIK_DATASOURCE_INPUTTYPE_TRWLAYER,
	true,
	false, /* keep dialog open after success */
	true,
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

static void datasource_bfilter_polygon_get_process_options(VikLayerParamData *paramdatas, ProcessOptions *po, void * not_used, const char *input_filename, const char *input_track_filename)
{
	po->shell_command = g_strdup_printf("gpsbabel -i gpx -f %s -o arc -F - | gpsbabel -i gpx -f %s -x polygon,file=- -o gpx -F -", input_track_filename, input_filename);
}
/* TODO: shell_escape stuff */

VikDataSourceInterface vik_datasource_bfilter_polygon_interface = {
	N_("Waypoints Inside This"),
	N_("Polygonized Layer"),
	VIK_DATASOURCE_CREATENEWLAYER,
	VIK_DATASOURCE_INPUTTYPE_TRWLAYER_TRACK,
	true,
	false, /* keep dialog open after success */
	true,
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

static void datasource_bfilter_exclude_polygon_get_process_options(VikLayerParamData *paramdatas, ProcessOptions *po, void * not_used, const char *input_filename, const char *input_track_filename)
{
	po->shell_command = g_strdup_printf("gpsbabel -i gpx -f %s -o arc -F - | gpsbabel -i gpx -f %s -x polygon,exclude,file=- -o gpx -F -", input_track_filename, input_filename);
}
/* TODO: shell_escape stuff */

VikDataSourceInterface vik_datasource_bfilter_exclude_polygon_interface = {
	N_("Waypoints Outside This"),
	N_("Polygonzied Layer"),
	VIK_DATASOURCE_CREATENEWLAYER,
	VIK_DATASOURCE_INPUTTYPE_TRWLAYER_TRACK,
	true,
	false, /* keep dialog open after success */
	true,
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
