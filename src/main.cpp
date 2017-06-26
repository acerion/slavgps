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
 */




#include <QApplication>
#include <QPushButton>
#include <QResource>

#include "acquire.h"
#include "window.h"
#include "layer.h"
#include "layer_defaults.h"
#include "layer_map.h"
#include "layers_panel.h"
#include "dem_cache.h"
#include "map_cache.h"
#include "download.h"
#include "background.h"
#include "preferences.h"
#include "settings.h"
#include "babel.h"
#include "modules.h"
#include "vikutils.h"




using namespace SlavGPS;




#if 0


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



/* Options. */
static GOptionEntry entries[] = {
	{ "debug",     'd', 0, G_OPTION_ARG_NONE,   &vik_debug,      N_("Enable debug output"), NULL },
	{ "verbose",   'V', 0, G_OPTION_ARG_NONE,   &vik_verbose,    N_("Enable verbose output"), NULL },
	{ "version",   'v', 0, G_OPTION_ARG_NONE,   &vik_version,    N_("Show version"), NULL },
	{ "latitude",   0,  0, G_OPTION_ARG_DOUBLE, &startup_latitude,       N_("Latitude in decimal degrees"), NULL },
	{ "longitude",  0,  0, G_OPTION_ARG_DOUBLE, &startup_longitude,      N_("Longitude in decimal degrees"), NULL },
	{ "zoom",      'z', 0, G_OPTION_ARG_INT,    &startup_zoom_level_osm, N_("Zoom Level (OSM). Value can be 0 - 22"), NULL },
	{ "map",       'm', 0, G_OPTION_ARG_INT,    &startup_map_type_id,    N_("Add a map layer by id value. Use 0 for the default map."), NULL },
	{ NULL }
};


#endif


/* Default values that won't actually get applied unless changed by command line parameter values. */
static double startup_latitude = 0.0;
static double startup_longitude = 0.0;
static int startup_zoom_level_osm = -1;
static MapTypeID startup_map_type_id = MAP_TYPE_ID_INITIAL;





int main(int argc, char ** argv)
{

#if 0
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
#endif


	/* Discover if this is the very first run. */
	SGUtils::is_very_first_run();

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


	a_layer_defaults_init();
	a_download_init();

#if 0
	curl_download_init();
#endif

	a_babel_init();

	/* Init modules/plugins. */
	modules_init();
#ifdef K
	vik_georef_layer_init();
#endif
	layer_map_init();
	map_cache_init();
#ifdef K
	a_background_init();

	a_toolbar_init();
	vik_routing_prefs_init();

	/*
	  Second stage initialization.

	  Can now use a_preferences_get()
	*/
#endif
	a_background_post_init();
	a_babel_post_init();
#if 0

	modules_post_init();

	/* May need to initialize the Positonal TimeZone lookup. */
	if (Preferences::get_time_ref_frame() == VIK_TIME_REF_WORLD) {
		vu_setup_lat_lon_tz_lookup();
	}

	/* Set the icon. */
	QPixmap * main_icon = gdk_pixbuf_from_pixdata(&viking_pixbuf, false, NULL);
	gtk_window_set_default_icon(main_icon);

	gdk_threads_enter();


	/* Create the first window. */
	SlavGPS::Window * first_window = SlavGPS::Window::new_window();

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
#endif

#ifdef K
	gtk_main();
	gdk_threads_leave();
#endif



	QApplication app(argc, argv);

	QResource::registerResource("icons.rcc");
	Layer::preconfigure_interfaces();

	acquire_init();

	/* Ask for confirmation of default settings on first run. */
	SGUtils::set_auto_features_on_first_run();

	Window window;
	window.layers_panel->set_viewport(window.viewport); /* Ugly, FIXME. */
	SGUtils::command_line(&window, startup_latitude, startup_longitude, startup_zoom_level_osm, startup_map_type_id);
	window.show();

	SGUtils::check_latest_version(&window);

	int rv = app.exec();

	a_babel_uninit();
#if 0
	a_toolbar_uninit();
	a_background_uninit();
#endif
	map_cache_uninit();
	DEMCache::uninit();
	a_layer_defaults_uninit();
	Preferences::uninit();
	a_settings_uninit();
#if 0


	modules_uninit();

	curl_download_uninit();

	vu_finalize_lat_lon_tz_lookup();

	/* Clean up any temporary files. */
	util_remove_all_in_deletion_list();

	delete first_window;
#endif
	return rv;
}
