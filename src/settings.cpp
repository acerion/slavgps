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
 /*
  * Sort of like the globals file, but values are automatically saved via program use.
  * Some settings are *not* intended to have any GUI controls.
  * Other settings be can used to set other GUI elements.
  *
  * ATM This is implemented using the simple (for me!) GKeyFile API - AKA an .ini file
  *  One might wish to consider the more modern alternative such as:
  *            http://developer.gnome.org/gio/2.26/GSettings.html
  * Since these settings are 'internal' I have no problem with them *not* being supported
  *  between various Viking versions, should one switch to different API/storage methods.
  * Indeed even the internal settings themselves can be liable to change.
  */
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include "dir.h"

#include "settings.h"

static GKeyFile *keyfile;

#define VIKING_INI_FILE "viking.ini"

static bool settings_load_from_file()
{
	GKeyFileFlags flags = G_KEY_FILE_KEEP_COMMENTS;

	GError *error = NULL;

	char *fn = g_build_filename ( a_get_viking_dir(), VIKING_INI_FILE, NULL );

	if ( !g_key_file_load_from_file ( keyfile, fn, flags, &error ) ) {
		fprintf(stderr, "WARNING: %s: %s\n", error->message, fn );
		free( fn );
		g_error_free ( error );
		return false;
	}

	free( fn );

	return true;
}

void a_settings_init()
{
	keyfile = g_key_file_new();
	settings_load_from_file();
}

/**
 * a_settings_uninit:
 *
 *  ATM: The only time settings are saved is on program exit
 *   Could change this to occur on window exit or dialog exit or have memory hash of values...?
 */
void a_settings_uninit()
{
	GError *error = NULL;
	char *fn = g_build_filename ( a_get_viking_dir(), VIKING_INI_FILE, NULL );
	size_t size;

	char *keyfilestr = g_key_file_to_data ( keyfile, &size, &error );

	if ( error ) {
		fprintf(stderr, "WARNING: %s\n", error->message );
		g_error_free ( error );
		goto tidy;
	}

	g_file_set_contents ( fn, keyfilestr, size, &error );
	if ( error ) {
		fprintf(stderr, "WARNING: %s: %s\n", error->message, fn );
		g_error_free ( error );
	}

	g_key_file_free ( keyfile );
 tidy:
	free( keyfilestr );
	free( fn );
}

// ATM, can't see a point in having any more than one group for various settings
#define VIKING_SETTINGS_GROUP "viking"

static bool settings_get_boolean ( const char *group, const char *name, bool *val )
{
	GError *error = NULL;
	bool success = true;
	bool bb = g_key_file_get_boolean ( keyfile, group, name, &error );
	if ( error ) {
		// Only print on debug - as often may have requests for keys not in the file
		fprintf(stderr, "DEBUG: %s\n", error->message );
		g_error_free ( error );
		success = false;
	}
	*val = bb;
	return success;
}

bool a_settings_get_boolean ( const char *name, bool *val )
{
	return settings_get_boolean ( VIKING_SETTINGS_GROUP, name, val );
}

void a_settings_set_boolean ( const char *name, bool val )
{
	g_key_file_set_boolean ( keyfile, VIKING_SETTINGS_GROUP, name, val );
}

static bool settings_get_string ( const char *group, const char *name, char **val )
{
	GError *error = NULL;
	bool success = true;
	char *str = g_key_file_get_string ( keyfile, group, name, &error );
	if ( error ) {
		// Only print on debug - as often may have requests for keys not in the file
		fprintf(stderr, "DEBUG: %s\n", error->message );
		g_error_free ( error );
		success = false;
	}
	*val = str;
	return success;
}

bool a_settings_get_string ( const char *name, char **val )
{
	return settings_get_string ( VIKING_SETTINGS_GROUP, name, val );
}

void a_settings_set_string ( const char *name, const char *val )
{
	g_key_file_set_string ( keyfile, VIKING_SETTINGS_GROUP, name, val );
}

static bool settings_get_integer ( const char *group, const char *name, int *val )
{
	GError *error = NULL;
	bool success = true;
	int ii = g_key_file_get_integer ( keyfile, group, name, &error );
	if ( error ) {
		// Only print on debug - as often may have requests for keys not in the file
		fprintf(stderr, "DEBUG: %s\n", error->message );
		g_error_free ( error );
		success = false;
	}
	*val = ii;
	return success;
}

bool a_settings_get_integer ( const char *name, int *val )
{
	return settings_get_integer ( VIKING_SETTINGS_GROUP, name, val );
}

void a_settings_set_integer ( const char *name, int val )
{
	g_key_file_set_integer ( keyfile, VIKING_SETTINGS_GROUP, name, val );
}

static bool settings_get_double ( const char *group, const char *name, double *val )
{
	GError *error = NULL;
	bool success = true;
	double dd = g_key_file_get_double ( keyfile, group, name, &error );
	if ( error ) {
		// Only print on debug - as often may have requests for keys not in the file
		fprintf(stderr, "DEBUG: %s\n", error->message );
		g_error_free ( error );
		success = false;
	}
	*val = dd;
	return success;
}

bool a_settings_get_double ( const char *name, double *val )
{
	return settings_get_double ( VIKING_SETTINGS_GROUP, name, val );
}

void a_settings_set_double ( const char *name, double val )
{
	g_key_file_set_double ( keyfile, VIKING_SETTINGS_GROUP, name, val );
}

static bool settings_get_integer_list ( const char *group, const char *name, int **vals, size_t *length )
{
	GError *error = NULL;
	bool success = true;
	int *ints = g_key_file_get_integer_list ( keyfile, group, name, length, &error );
	if ( error ) {
		// Only print on debug - as often may have requests for keys not in the file
		fprintf(stderr, "DEBUG: %s\n", error->message );
		g_error_free ( error );
		success = false;
	}
	*vals = ints;
	return success;
}

/*
 * The returned list of integers should be freed when no longer needed
 */
static bool a_settings_get_integer_list ( const char *name, int **vals, size_t* length )
{
	return settings_get_integer_list ( VIKING_SETTINGS_GROUP, name, vals, length );
}

static void a_settings_set_integer_list ( const char *name, int vals[], size_t length )
{
	g_key_file_set_integer_list ( keyfile, VIKING_SETTINGS_GROUP, name, vals, length );
}

bool a_settings_get_integer_list_contains ( const char *name, int val )
{
	int* vals = NULL;
	size_t length;
	// Get current list and see if the value supplied is in the list
	bool contains = false;
	// Get current list
	if ( a_settings_get_integer_list ( name, &vals, &length ) ) {
		// See if it's not already there
		int ii = 0;
		if ( vals && length ) {
			while ( ii < length ) {
				if ( vals[ii] == val ) {
					contains = true;
					break;
				}
				ii++;
			}
			// Free old array
			free(vals);
		}
	}
	return contains;
}

void a_settings_set_integer_list_containing ( const char *name, int val )
{
	int* vals = NULL;
	size_t length = 0;
	bool need_to_add = true;
	int ii = 0;
	// Get current list
	if ( a_settings_get_integer_list ( name, &vals, &length ) ) {
		// See if it's not already there
		if ( vals ) {
			while ( ii < length ) {
				if ( vals[ii] == val ) {
					need_to_add = false;
					break;
				}
				ii++;
			}
		}
	}
	// Add value into array if necessary
	if ( vals && need_to_add ) {
		// NB not bothering to sort this 'list' ATM as there is not much to be gained
		unsigned int new_length = length + 1;
		int new_vals[new_length];
		// Copy array
		for ( ii = 0; ii < length; ii++ ) {
			new_vals[ii] = vals[ii];
		}
		new_vals[length] = val; // Set the new value
		// Apply
		a_settings_set_integer_list ( name, new_vals, new_length );
		// Free old array
		free(vals);
	}
}
