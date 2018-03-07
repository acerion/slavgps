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
#include <QResource>




#include "acquire.h"
#include "window.h"
#include "layer.h"
#include "layer_defaults.h"
#include "layer_map.h"
#include "layer_georef.h"
#include "layer_interface.h"
#include "layers_panel.h"
#include "dem_cache.h"
#include "map_cache.h"
#include "download.h"
#include "background.h"
#include "preferences.h"
#include "application_state.h"
#include "babel.h"
#include "modules.h"
#include "vikutils.h"
#include "util.h"
#include "version_check.h"
#include "routing.h"




using namespace SlavGPS;




#ifdef K_TODO


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
	qDebug() << QObject::tr("Ignoring Xlib error: error code %1 request code %2")
		.arg(theEvent->error_code)
		.arg(theEvent->request_code);
	/* No exit on X errors!
	   Mainly to handle out of memory error when requesting large pixbuf from user request.
	   See vikwindow.c::save_image_file() */
	return 0;
}
#endif
#endif




int main(int argc, char ** argv)
{
	QApplication app(argc, argv);
	CommandLineOptions command_line_options;


	QTranslator translator;
	/* Look up e.g. :/translations/myapp_de.qm. */
	if (translator.load(QLocale(), QLatin1String(PACKAGE_NAME), QLatin1String("_"), QLatin1String(":/translations"))) {
		app.installTranslator(&translator);
	}


	if (!command_line_options.parse(app)) {
		return EXIT_FAILURE;
	}
	if (command_line_options.version) {
		qDebug() << QObject::tr("%1 %2\nCopyright (c) 2003-2008 Evan Battaglia\nCopyright (c) 2008-%3 Viking's contributors\n")
			.arg(PACKAGE_NAME).arg(PACKAGE_VERSION).arg(THEYEAR);
		return EXIT_SUCCESS;
	}


#ifdef K_TODO
#if HAVE_X11_XLIB_H
	XSetErrorHandler(myXErrorHandler);
#endif
#endif


	/* Discover if this is the very first run. */
	SGUtils::is_very_first_run();

	ApplicationState::init();

	/*
	  First stage initialization.

	  Should not use Preferences::get_param_value() yet.

	  Since the first time Preferences::get_param_value() is called it loads
	  any preferences values from disk, but of course for
	  preferences not registered yet it can't actually understand
	  them so subsequent initial attempts to get those preferences
	  return the default value, until the values have changed.
	*/
	Preferences::register_default_values();


	LayerDefaults::init();

	Download::init();

	Babel::init();

	/* Init modules/plugins. */
	modules_init();
	layer_georef_init();
	layer_map_init();
	map_cache_init();
	a_background_init();
	routing_prefs_init();

	/*
	  Second stage initialization.

	  Can now use Preferences::get_param_value()
	*/

	a_background_post_init();
	Babel::post_init();

	modules_post_init();

	/* May need to initialize the Positonal TimeZone lookup. */
	if (Preferences::get_time_ref_frame() == SGTimeReference::World) {
		vu_setup_lat_lon_tz_lookup();
	}



	QResource::registerResource("icons.rcc");
	Layer::preconfigure_interfaces();

	Acquire::init();

	/* Ask for confirmation of default settings on first run. */
	SGUtils::set_auto_features_on_first_run();


	/* Create the first window. */
	Window * first_window = Window::new_window();

	for (int i = 0; i < command_line_options.files.size(); i++) {
		const QString & file_path = command_line_options.files.at(i);

		Window * new_window = first_window;
		bool set_as_current_document = (i == 1);

		/* Open any subsequent .vik files in their own window. */
		if (i > 1 && VikFile::has_vik_file_magic(file_path)) {
			new_window = Window::new_window();
			set_as_current_document = true;
		}

		new_window->open_file(file_path, set_as_current_document);
	}

	first_window->finish_new();
	command_line_options.apply(first_window);
	first_window->show();



	VersionCheck::run_check(first_window);

	int rv = app.exec();

	Babel::uninit();
	a_background_uninit();

	map_cache_uninit();
	DEMCache::uninit();
	LayerDefaults::uninit();
	Preferences::uninit();
	ApplicationState::uninit();
	modules_uninit();

	Download::uninit();

	vu_finalize_lat_lon_tz_lookup();

	/* Clean up any temporary files. */
	Util::remove_all_in_deletion_list();

	delete first_window;

	return rv;
}
