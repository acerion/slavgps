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

#include "window.h"
#include "layer.h"
#include "layer_defaults.h"
#include "download.h"
#include "background.h"
#include "preferences.h"
#include "settings.h"
#include "babel.h"
#include "modules.h"




using namespace SlavGPS;




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

	/* Discover if this is the very first run. */
	a_vik_very_first_run();

#endif
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
	maps_layer_init();
	map_cache_init();
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
	GdkPixbuf * main_icon = gdk_pixbuf_from_pixdata(&viking_pixbuf, false, NULL);
	gtk_window_set_default_icon(main_icon);

	gdk_threads_enter();

	/* Ask for confirmation of default settings on first run. */
	vu_set_auto_features_on_first_run();

	/* Create the first window. */
	SlavGPS::Window * first_window = SlavGPS::Window::new_window();

	vu_check_latest_version(first_window->get_toolkit_window());

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
#endif



	QApplication app(argc, argv);

	QResource::registerResource("icons.rcc");
	Layer::preconfigure_interfaces();

	Window window;
	window.layers_panel->set_viewport(window.viewport); /* Ugly, FIXME. */
	window.show();

	int rv = app.exec();

	a_babel_uninit();
#if 0
	a_toolbar_uninit();
	a_background_uninit();
	map_cache_uninit();
	dem_cache_uninit();
#endif
	a_layer_defaults_uninit();
	Preferences::uninit();
	a_settings_uninit();
#if 0


	modules_uninit();

	curl_download_uninit();

	vu_finalize_lat_lon_tz_lookup();

	/* Clean up any temporary files. */
	util_remove_all_in_deletion_list();
#endif
	return rv;
}
