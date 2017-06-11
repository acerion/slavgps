/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#ifdef HAVE_CONFIG
#include "config.h"
#endif /* HAVE_CONFIG */

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <cstdlib>

#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixdata.h>

#include "icons/icons.h"
#include "map_cache.h"
#include "background.h"
#include "dems.h"
#include "babel.h"
#include "curl_download.h"
#include "preferences.h"
#include "layer_defaults.h"
#include "globals.h"
#include "layer_map.h"
#include "layer_georef.h"
#include "vikrouting.h"
#include "vikutils.h"
#include "util.h"
#include "toolbar.h"
#include "map_ids.h"
#include "file.h"
#include "settings.h"
#include "window.h"
#include "modules.h"

/* FIXME LOCALEDIR must be configured by ./configure --localedir */
/* But something does not work actually. */
/* So, we need to redefine this variable on windows. */
#ifdef WINDOWS
#undef LOCALEDIR
#define LOCALEDIR "locale"
#endif

#ifdef HAVE_X11_XLIB_H
#include "X11/Xlib.h"
#endif




using namespace SlavGPS;




#if GLIB_CHECK_VERSION (2, 32, 0)
/* Callback to log message. */
static void log_debug(const char * log_domain, GLogLevelFlags log_level, const char * message, void * user_data)
{
	fprintf(stdout, "** (viking): DEBUG: %s\n", message);
}
#else
/* Callback to mute log message. */
static void mute_log(const char * log_domain, GLogLevelFlags log_level, const char * message, void * user_data)
{
	/* Nothing to do, we just want to mute. */
}
#endif




#if HAVE_X11_XLIB_H
static int myXErrorHandler(Display * display, XErrorEvent * theEvent)
{
	fprintf(stderr,
		_("Ignoring Xlib error: error code %d request code %d\n"),
		theEvent->error_code,
		theEvent->request_code);
	/* No exit on X errors!
	   Mainly to handle out of memory error when requesting large pixbuf from user request.
	   See vikwindow.c::save_image_file() */
	return 0;
}
#endif




/* Default values that won't actually get applied unless changed by command line parameter values. */
static double latitude = 0.0;
static double longitude = 0.0;
static int zoom_level_osm = -1;
static MapTypeID map_type_id = MAP_TYPE_ID_INITIAL;




/* Options. */
static GOptionEntry entries[] = {
	{ "debug",     'd', 0, G_OPTION_ARG_NONE,   &vik_debug,      N_("Enable debug output"), NULL },
	{ "verbose",   'V', 0, G_OPTION_ARG_NONE,   &vik_verbose,    N_("Enable verbose output"), NULL },
	{ "version",   'v', 0, G_OPTION_ARG_NONE,   &vik_version,    N_("Show version"), NULL },
	{ "latitude",   0,  0, G_OPTION_ARG_DOUBLE, &latitude,       N_("Latitude in decimal degrees"), NULL },
	{ "longitude",  0,  0, G_OPTION_ARG_DOUBLE, &longitude,      N_("Longitude in decimal degrees"), NULL },
	{ "zoom",      'z', 0, G_OPTION_ARG_INT,    &zoom_level_osm, N_("Zoom Level (OSM). Value can be 0 - 22"), NULL },
	{ "map",       'm', 0, G_OPTION_ARG_INT,    &map_type_id,    N_("Add a map layer by id value. Use 0 for the default map."), NULL },
	{ NULL }
};




int main(int argc, char *argv[])
{
	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

#if ! GLIB_CHECK_VERSION (2, 32, 0)
	g_thread_init(NULL);
#endif
	gdk_threads_init();

	GError *error = NULL;
	bool gui_initialized = gtk_init_with_args(&argc, &argv, "files+", entries, NULL, &error);
	if (!gui_initialized) {
		/* Check if we have an error message. */
		if (error == NULL) {
			/* No error message, the GUI initialization failed. */
			const char *display_name = gdk_get_display_arg_name();
			fprintf(stderr, "Failed to open display: %s\n", (display_name != NULL) ? display_name : " ");
		} else {
			/* Yep, there's an error, so print it. */
			fprintf(stderr, "Parsing command line options failed: %s\n", error->message);
			g_error_free(error);
			fprintf(stderr, "Run \"%s --help\" to see the list of recognized options.\n",argv[0]);
		}
		return EXIT_FAILURE;
	}

	if (vik_version) {
		g_printf ("%s %s\nCopyright (c) 2003-2008 Evan Battaglia\nCopyright (c) 2008-" THEYEAR" Viking's contributors\n", PACKAGE_NAME, PACKAGE_VERSION);
		return EXIT_SUCCESS;
	}

#if GLIB_CHECK_VERSION (2, 32, 0)
	if (vik_debug) {
		g_log_set_handler(NULL, G_LOG_LEVEL_DEBUG, log_debug, NULL);
	}
#else
	if (!vik_debug) {
		g_log_set_handler(NULL, G_LOG_LEVEL_DEBUG, mute_log, NULL);
	}
#endif

#if HAVE_X11_XLIB_H
	XSetErrorHandler(myXErrorHandler);
#endif

	/* Discover if this is the very first run. */
	a_vik_very_first_run();

	a_settings_init();
	Preferences::init();

	/*
	  First stage initialization.

	  Should not use a_preferences_get() yet.

	  Since the first time a_preferences_get() is called it loads
	  any preferences values from disk, but of course for
	  preferences not registered yet it can't actually understand
	  them so subsequent initial attempts to get those preferences
	  return the default value, until the values have changed.
	*/
	Preferences::register_default_values();

	layer_defaults_init();

	a_download_init();
	curl_download_init();

	a_babel_init();

	/* Init modules/plugins. */
	modules_init();

	vik_georef_layer_init();
	layer_map_init();
	maps_layer_init();
	map_cache_init();
	a_background_init();

	a_toolbar_init();
	vik_routing_prefs_init();

	/*
	  Second stage initialization.

	  Can now use a_preferences_get()
	*/
	a_background_post_init();
	a_babel_post_init();
	modules_post_init();

	/* May need to initialize the Positonal TimeZone lookup. */
	if (Preferences::get_time_ref_frame() == VIK_TIME_REF_WORLD) {
		vu_setup_lat_lon_tz_lookup();
	}

	/* Set the icon. */
	QPixmap * main_icon = gdk_pixbuf_from_pixdata(&viking_pixbuf, false, NULL);
	gtk_window_set_default_icon(main_icon);

	gdk_threads_enter();

	/* Ask for confirmation of default settings on first run. */
	vu_set_auto_features_on_first_run();

	/* Create the first window. */
	SlavGPS::Window * first_window = SlavGPS::Window::new_window();

	vu_check_latest_version(first_window);

	int i = 0;
	bool dashdash_already = false;
	while (++i < argc) {
		if (strcmp(argv[i],"--") == 0 && !dashdash_already) {
			dashdash_already = true; /* Hack to open '-' */
		} else {
			SlavGPS::Window * new_window = first_window;
			bool change_filename = (i == 1);

			/* Open any subsequent .vik files in their own window. */
			if (i > 1 && check_file_magic_vik(argv[i])) {
				new_window = SlavGPS::Window::new_window();
				change_filename = true;
			}

			new_window->open_file(argv[i], change_filename);
		}
	}

	first_window->finish_new();

	vu_command_line(first_window, latitude, longitude, zoom_level_osm, map_type_id);

	gtk_main();
	gdk_threads_leave();

	a_babel_uninit();
	a_toolbar_uninit();
	a_background_uninit();
	map_cache_uninit();
	dem_cache_uninit();
	a_layer_defaults_uninit();
	a_preferences_uninit();
	a_settings_uninit();

	modules_uninit();

	curl_download_uninit();

	vu_finalize_lat_lon_tz_lookup();

	/* Clean up any temporary files. */
	util_remove_all_in_deletion_list();

	delete first_window;

	return 0;
}
