/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
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
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib/gstdio.h>
#include "viking.h"
#include "viklayer_defaults.h"
#include "dir.h"
#include "file.h"
#include "globals.h"

#define VIKING_LAYER_DEFAULTS_INI_FILE "viking_layer_defaults.ini"

// A list of the parameter types in use
static GPtrArray *paramsVD;

static GKeyFile *keyfile;

static bool loaded;

static VikLayerParamData get_default_data_answer ( const char *group, const char *name, VikLayerParamType ptype, void * *success )
{
	VikLayerParamData data = VIK_LPD_BOOLEAN ( false );

	GError *error = NULL;

	switch ( ptype ) {
	case VIK_LAYER_PARAM_DOUBLE: {
		double dd = g_key_file_get_double ( keyfile, group, name, &error );
		if ( !error ) data.d = dd;
		break;
	}
	case VIK_LAYER_PARAM_UINT: {
		uint32_t uu = g_key_file_get_integer ( keyfile, group, name, &error );
		if ( !error ) data.u = uu;
		break;
	}
	case VIK_LAYER_PARAM_INT: {
		int32_t ii = g_key_file_get_integer ( keyfile, group, name, &error );
		if ( !error ) data.i = ii;
		break;
	}
	case VIK_LAYER_PARAM_BOOLEAN: {
		bool bb = g_key_file_get_boolean ( keyfile, group, name, &error );
		if ( !error ) data.b = bb;
		break;
	}
	case VIK_LAYER_PARAM_STRING: {
		char *str = g_key_file_get_string ( keyfile, group, name, &error );
		if ( !error ) data.s = str;
		break;
	}
	//case VIK_LAYER_PARAM_STRING_LIST: {
	//	char **str = g_key_file_get_string_list ( keyfile, group, name, &error );
	//	data.sl = str_to_glist (str); //TODO convert
	//	break;
	//}
	case VIK_LAYER_PARAM_COLOR: {
		char *str = g_key_file_get_string ( keyfile, group, name, &error );
		if ( !error ) {
			memset(&(data.c), 0, sizeof(data.c));
			gdk_color_parse ( str, &(data.c) );
		}
		free( str );
		break;
	}
	default: break;
	}
	*success = KINT_TO_POINTER (true);
	if ( error ) {
		fprintf(stderr, "WARNING: %s\n", error->message );
		g_error_free ( error );
		*success = KINT_TO_POINTER (false);
	}

	return data;
}

static VikLayerParamData get_default_data ( const char *group, const char *name, VikLayerParamType ptype )
{
	void * success = KINT_TO_POINTER (true);
	// NB This should always succeed - don't worry about 'success'
	return get_default_data_answer ( group, name, ptype, &success );
}

static void set_default_data ( VikLayerParamData data, const char *group, const char *name, VikLayerParamType ptype )
{
	switch ( ptype ) {
	case VIK_LAYER_PARAM_DOUBLE:
		g_key_file_set_double ( keyfile, group, name, data.d );
		break;
	case VIK_LAYER_PARAM_UINT:
		g_key_file_set_integer ( keyfile, group, name, data.u );
		break;
	case VIK_LAYER_PARAM_INT:
		g_key_file_set_integer ( keyfile, group, name, data.i );
		break;
	case VIK_LAYER_PARAM_BOOLEAN:
		g_key_file_set_boolean ( keyfile, group, name, data.b );
		break;
	case VIK_LAYER_PARAM_STRING:
		g_key_file_set_string ( keyfile, group, name, data.s );
		break;
	case VIK_LAYER_PARAM_COLOR: {
		char *str = g_strdup_printf ( "#%.2x%.2x%.2x", (int)(data.c.red/256),(int)(data.c.green/256),(int)(data.c.blue/256));
		g_key_file_set_string ( keyfile, group, name, str );
		free( str );
		break;
	}
	default: break;
	}
}

static void defaults_run_setparam ( void * index_ptr, uint16_t i, VikLayerParamData data, VikLayerParam *params )
{
	// Index is only an index into values from this layer
	int index = KPOINTER_TO_INT ( index_ptr );
	VikLayerParam *vlp = (VikLayerParam *)g_ptr_array_index(paramsVD,index+i);

	set_default_data ( data, vik_layer_get_interface(vlp->layer)->fixed_layer_name, vlp->name, vlp->type );
}

static VikLayerParamData defaults_run_getparam ( gpointer index_ptr, uint16_t i, bool notused2 )
{
	// Index is only an index into values from this layer
	int index = KPOINTER_TO_INT ( index_ptr );
	VikLayerParam *vlp = (VikLayerParam *)g_ptr_array_index(paramsVD,index+i);

	return get_default_data ( vik_layer_get_interface(vlp->layer)->fixed_layer_name, vlp->name, vlp->type );
}

static void use_internal_defaults_if_missing_default ( VikLayerTypeEnum type )
{
	VikLayerParam *params = vik_layer_get_interface(type)->params;
	if ( ! params )
		return;

	uint16_t params_count = vik_layer_get_interface(type)->params_count;
	uint16_t i;
	// Process each parameter
	for ( i = 0; i < params_count; i++ ) {
		if ( params[i].group != VIK_LAYER_NOT_IN_PROPERTIES ) {
			void * success = KINT_TO_POINTER (false);
			// Check current default is available
			get_default_data_answer ( vik_layer_get_interface(type)->fixed_layer_name, params[i].name, params[i].type, &success );
			// If no longer have a viable default
			if ( ! KPOINTER_TO_INT (success) ) {
				// Reset value
				if ( params[i].default_value ) {
					VikLayerParamData paramd = params[i].default_value();
					set_default_data ( paramd, vik_layer_get_interface(type)->fixed_layer_name, params[i].name, params[i].type );
				}
			}
		}
	}
}

static bool defaults_load_from_file()
{
	GKeyFileFlags flags = G_KEY_FILE_KEEP_COMMENTS;

	GError *error = NULL;

	char *fn = g_build_filename ( a_get_viking_dir(), VIKING_LAYER_DEFAULTS_INI_FILE, NULL );

	if ( !g_key_file_load_from_file ( keyfile, fn, flags, &error ) ) {
		fprintf(stderr, "WARNING: %s: %s\n", error->message, fn );
		free( fn );
		g_error_free ( error );
		return false;
	}

	free( fn );

	// Ensure if have a key file, then any missing values are set from the internal defaults
	int layer;
	for ( layer = 0; ((VikLayerTypeEnum) layer) < VIK_LAYER_NUM_TYPES; layer++ ) {
		use_internal_defaults_if_missing_default ( (VikLayerTypeEnum) layer );
	}

	return true;
}

/* true on success */
static bool layer_defaults_save_to_file()
{
	bool answer = true;
	GError *error = NULL;
	char *fn = g_build_filename ( a_get_viking_dir(), VIKING_LAYER_DEFAULTS_INI_FILE, NULL );
	size_t size;

    char *keyfilestr = g_key_file_to_data ( keyfile, &size, &error );

    if ( error ) {
		fprintf(stderr, "WARNING: %s\n", error->message );
		g_error_free ( error );
		answer = false;
		goto tidy;
	}

	// optionally could do:
	// g_file_set_contents ( fn, keyfilestr, size, &error );
    // if ( error ) {
	//	fprintf(stderr, "WARNING: %s: %s\n", error->message, fn );
	//	 g_error_free ( error );
	//  answer = false;
	//	goto tidy;
	// }

	FILE *ff;
	if ( !(ff = g_fopen ( fn, "w")) ) {
		fprintf(stderr, _("WARNING: Could not open file: %s\n"), fn );
		answer = false;
		goto tidy;
	}
	// Layer defaults not that secret, but just incase...
	if ( g_chmod ( fn, 0600 ) != 0 )
		fprintf(stderr, "WARNING: %s: Failed to set permissions on %s\n", __FUNCTION__, fn );

	fputs ( keyfilestr, ff );
	fclose ( ff );

tidy:
	free( keyfilestr );
	free( fn );

	return answer;
}

/**
 * a_layer_defaults_show_window:
 * @parent:     The Window
 * @layername:  The layer
 *
 * This displays a Window showing the default parameter values for the selected layer
 * It allows the parameters to be changed.
 *
 * Returns: %true if the window is displayed (because there are parameters to view)
 */
bool a_layer_defaults_show_window ( GtkWindow *parent, const char *layername )
{
	if ( ! loaded ) {
		// since we can't load the file in a_defaults_init (no params registered yet),
		// do it once before we display the params.
		defaults_load_from_file();
		loaded = true;
	}

    VikLayerTypeEnum layer = Layer::type_from_string(layername);

    // Need to know where the params start and they finish for this layer

    // 1. inspect every registered param - see if it has the layer value required to determine overall size
    //    [they are contiguous from the start index]
    // 2. copy the these params from the main list into a tmp struct
    //
    // Then pass this tmp struct to uibuilder for display

    unsigned int layer_params_count = 0;

    bool found_first = false;
    int index = 0;
    int i;
    for ( i = 0; i < paramsVD->len; i++ ) {
		VikLayerParam *param = (VikLayerParam*)(g_ptr_array_index(paramsVD,i));
		if ( param->layer == layer ) {
			layer_params_count++;
			if ( !found_first ) {
				index = i;
				found_first = true;
			}
		}
    }

	// Have we any parameters to show!
    if ( !layer_params_count )
		return false;

    VikLayerParam *params = (VikLayerParam *) malloc(layer_params_count * sizeof (VikLayerParam));
    for ( i = 0; i < layer_params_count; i++ ) {
      params[i] = *((VikLayerParam*)(g_ptr_array_index(paramsVD,i+index)));
    }

    char *title = g_strconcat ( layername, ": ", _("Layer Defaults"), NULL );

	if ( a_uibuilder_properties_factory ( title,
	                                      parent,
	                                      params,
	                                      layer_params_count,
	                                      vik_layer_get_interface(layer)->params_groups,
	                                      vik_layer_get_interface(layer)->params_groups_count,
	                                      (bool (*) (void *,uint16_t,VikLayerParamData,void *,bool)) defaults_run_setparam,
	                                      KINT_TO_POINTER ( index ),
	                                      params,
	                                      defaults_run_getparam,
	                                      KINT_TO_POINTER ( index ),
	                                      NULL ) ) {
		// Save
		layer_defaults_save_to_file();
    }

    free( title );
    free( params );

    return true;
}

/**
 * a_layer_defaults_register:
 * @vlp:        The parameter
 * @defaultval: The default value
 * @layername:  The layer in which the parameter resides
 *
 * Call this function on to set the default value for the particular parameter
 */
void a_layer_defaults_register (VikLayerParam *vlp, VikLayerParamData defaultval, const char *layername )
{
	/* copy value */
	VikLayerParam *newvlp = (VikLayerParam *) malloc(1 * sizeof (VikLayerParam));
	*newvlp = *vlp;

	g_ptr_array_add ( paramsVD, newvlp );

	set_default_data ( defaultval, layername, vlp->name, vlp->type );
}

/**
 * a_layer_defaults_init:
 *
 * Call this function at startup
 */
void a_layer_defaults_init()
{
	keyfile = g_key_file_new();

	/* not copied */
	paramsVD = g_ptr_array_new ();

	loaded = false;
}

/**
 * a_layer_defaults_uninit:
 *
 * Call this function on program exit
 */
void a_layer_defaults_uninit()
{
	g_key_file_free ( keyfile );
	g_ptr_array_foreach ( paramsVD, (GFunc)g_free, NULL );
	g_ptr_array_free ( paramsVD, true );
}

/**
 * a_layer_defaults_get:
 * @layername:  String name of the layer
 * @param_name: String name of the parameter
 * @param_type: The parameter type
 *
 * Call this function to get the default value for the parameter requested
 */
VikLayerParamData a_layer_defaults_get ( const char *layername, const char *param_name, VikLayerParamType param_type )
{
	if ( ! loaded ) {
		// since we can't load the file in a_defaults_init (no params registered yet),
		// do it once before we get the first key.
		defaults_load_from_file();
		loaded = true;
	}

	return get_default_data ( layername, param_name, param_type );
}

/**
 * a_layer_defaults_save:
 *
 * Call this function to save the current layer defaults
 * Normally should only be performed if any layer defaults have been changed via direct manipulation of the layer
 *  rather than the user changing the preferences via the dialog window above
 *
 * This must only be performed once all layer parameters have been initialized
 *
 * Returns: %true if saving was successful
 */
bool a_layer_defaults_save ()
{
	// Generate defaults
	int layer;
	for ( layer = 0; (VikLayerTypeEnum) layer < VIK_LAYER_NUM_TYPES; layer++ ) {
		use_internal_defaults_if_missing_default ( (VikLayerTypeEnum) layer );
	}

	return layer_defaults_save_to_file();
}
