/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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

#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <cstdlib>

#include <mapnik/version.hpp>
#include <mapnik/map.hpp>
#include <mapnik/datasource_cache.hpp>
#include <mapnik/agg_renderer.hpp>
#include <mapnik/load_map.hpp>
#include <mapnik/projection.hpp>
#if MAPNIK_VERSION < 300000
#include <mapnik/graphics.hpp>
#else
#include <mapnik/value.hpp>
#endif
#include <mapnik/image_util.hpp>

#include <glib.h>
#include <glib/gstdio.h>

#include <QPixmap>

#include "mapnik_interface.h"
#include "globals.h"
#include "application_state.h"
#include "util.h"




using namespace SlavGPS;




#if MAPNIK_VERSION < 200000
#include <mapnik/envelope.hpp>
#define image_32 Image32
#define image_data_32 ImageData32
#define box2d Envelope
#define zoom_to_box zoomToBox
#else
#include <mapnik/box2d.hpp>
#if MAPNIK_VERSION >= 300000
/* In Mapnik3 'image_32' has changed names once again. */
#define image_32 image_rgba8
#define raw_data data
#endif
#endif





class SlavGPS::MapnikInterface {
public:
	mapnik::Map * myMap = NULL;
	char * copyright = NULL; /* Cached Mapnik parameter to save looking it up each time. */
};



#ifdef K
/* Can't change prj after init - but ATM only support drawing in Spherical Mercator. */
static mapnik::projection prj(mapnik::MAPNIK_GMERC_PROJ);
#endif



SlavGPS::MapnikInterface * SlavGPS::mapnik_interface_new()
{
	MapnikInterface * mi = new MapnikInterface;
	mi->myMap = new mapnik::Map;
	mi->copyright = NULL;
	return mi;
}




void SlavGPS::mapnik_interface_free(MapnikInterface * mi)
{
	if (mi) {
		if (mi->copyright) {
			free(mi->copyright);
		}
		delete mi->myMap;
	}
	delete mi;
}




void SlavGPS::mapnik_interface_initialize(const char * plugins_dir, const char * font_dir, int font_dir_recurse)
{
#ifdef K
	g_debug ("using mapnik version %s", MAPNIK_VERSION_STRING);
	try {
		if (plugins_dir) {
#if MAPNIK_VERSION >= 200200
			mapnik::datasource_cache::instance().register_datasources(plugins_dir);
#else
			mapnik::datasource_cache::instance()->register_datasources(plugins_dir);
#endif
		}
		if (font_dir) {
			if (!mapnik::freetype_engine::register_fonts(font_dir, font_dir_recurse ? true : false)) {
				g_warning("%s: No fonts found", __FUNCTION__);
			}
		}
	} catch (std::exception const& ex) {
		g_warning("An error occurred while initialising mapnik: %s", ex.what());
	} catch (...) {
		g_warning("An unknown error occurred while initialising mapnik");
	}
#endif
}




/**
 *  Caching this answer instead of looking it up each time.
 */
static void set_copyright(MapnikInterface * mi)
{
#ifdef K
	g_free(mi->copyright);
	mi->copyright = NULL;

	mapnik::parameters pmts = mi->myMap->get_extra_parameters();
#if MAPNIK_VERSION < 300000
	for (mapnik::parameters::const_iterator ii = pmts.begin(); ii != pmts.end(); ii++) {
		if (ii->first == "attribution" || ii->first == "copyright") {
			std::stringstream ss;
			ss << ii->second;
			// Copy it
			mi->copyright = g_strdup((char *)ss.str().c_str());
			break;
		}
	}
#else
	if (pmts.get<std::string>("attribution")) {
		mi->copyright = g_strdup((*pmts.get<std::string>("attribution")).c_str());
	}

	if (!mi->copyright) {
		if (pmts.get<std::string>("copyright")) {
			mi->copyright = g_strdup((*pmts.get<std::string>("copyright")).c_str());
		}
	}
#endif
#endif
}




#define VIK_SETTINGS_MAPNIK_BUFFER_SIZE "mapnik_buffer_size"




/**
 * Returns NULL on success otherwise a string about what went wrong.
 * This string should be freed once it has been used.
 */
char * SlavGPS::mapnik_interface_load_map_file(MapnikInterface * mi, const char * filename, unsigned int width, unsigned int height)
{
	char * msg = NULL;
	if (!mi) {
		return strdup("Internal Error");
	}

	try {
#ifdef K
		mi->myMap->remove_all(); /* Support reloading. */
		mapnik::load_map(*mi->myMap, filename);

		mi->myMap->resize(width,height);
		mi->myMap->set_srs(mapnik::MAPNIK_GMERC_PROJ); /* ONLY WEB MERCATOR output supported ATM. */

		/* IIRC This size is the number of pixels outside the tile to be considered so stuff is shown (i.e. particularly labels).
		   Only set buffer size if the buffer size isn't explicitly set in the mapnik stylesheet.
		   Alternatively render a bigger 'virtual' tile and then only use the appropriate subset */
		if (mi->myMap->buffer_size() == 0) {
			int buffer_size = (width + height / 4); /* e.g. 128 for a 256x256 image. */
			int tmp;
			if (ApplicationState::get_integer(VIK_SETTINGS_MAPNIK_BUFFER_SIZE, &tmp)) {
				buffer_size = tmp;
			}

			mi->myMap->set_buffer_size(buffer_size);
		}
#endif
		set_copyright(mi);

		g_debug("%s layers: %d", __FUNCTION__, (unsigned int) mi->myMap->layer_count());
	} catch (std::exception const& ex) {
		msg = g_strdup(ex.what());
	} catch (...) {
		msg = strdup("unknown error");
	}
	return msg;
}




/**
 * Returns a QPixmap of the specified area. QPixmap may be NULL.
 */
QPixmap * SlavGPS::mapnik_interface_render(MapnikInterface * mi, double lat_tl, double lon_tl, double lat_br, double lon_br)
{
	if (!mi) {
		return NULL;
	}

	QPixmap * pixmap = NULL;

#ifdef K
	/* Note prj & bbox want stuff in lon,lat order! */
	double p0x = lon_tl;
	double p0y = lat_tl;
	double p1x = lon_br;
	double p1y = lat_br;

	/* Convert into projection coordinates for bbox. */
	prj.forward(p0x, p0y);
	prj.forward(p1x, p1y);

	/* Copy main object to local map variable.
	   This enables rendering to work when this function is called from different threads. */
	mapnik::Map myMap(*mi->myMap);

	try {
		unsigned width  = myMap.width();
		unsigned height = myMap.height();
		mapnik::image_32 image(width,height);
		mapnik::box2d<double> bbox(p0x, p0y, p1x, p1y);
		myMap.zoom_to_box(bbox);
		/* FUTURE: option to use cairo / grid renderers? */
		mapnik::agg_renderer<mapnik::image_32> render(myMap,image);
		render.apply();

		if (image.painted()) {
			unsigned char *ImageRawDataPtr = (unsigned char *) g_malloc(width * 4 * height);
			if (!ImageRawDataPtr) {
				return NULL;
			}
			memcpy(ImageRawDataPtr, image.raw_data(), width * height * 4);
#ifdef K
			pixmap = gdk_pixbuf_new_from_data(ImageRawDataPtr, GDK_COLORSPACE_RGB, TRUE, 8, width, height, width * 4, NULL, NULL);
#endif
		} else {
			g_warning("%s not rendered", __FUNCTION__);
		}
	}
	catch (const std::exception & ex) {
		g_warning("An error occurred while rendering: %s", ex.what());
	} catch (...) {
		g_warning("An unknown error occurred while rendering");
	}
#endif
	return pixmap;
}




/**
 * Copyright/Attribution information about the Map - string maybe NULL.
 *
 * Free returned string  after use.
 */
char * SlavGPS::mapnik_interface_get_copyright(MapnikInterface * mi)
{
	if (!mi) {
		return NULL;
	}
	return mi->copyright;
}




/**
 * 'Parameter' information about the Map configuration.
 *
 * Free every string element and the returned GArray itself after use.
 */
QStringList * SlavGPS::mapnik_interface_get_parameters(MapnikInterface * mi)
{
	QStringList * parameters = new QStringList;
#ifdef K
	mapnik::parameters pmts = mi->myMap->get_extra_parameters();
	/* Simply want the strings of each parameter so we can display something... */
#if MAPNIK_VERSION < 300000
	for (mapnik::parameters::const_iterator pmt = pmts.begin(); pmt != pmts.end(); pmt++) {
		std::stringstream ss;
		ss << pmt->first << ": " << pmt->second;
#else
	for (auto const& pmt : pmts) {
		std::stringstream ss;
		ss << pmt.first << ": " << *(pmts.get<std::string>(pmt.first,"empty"));
#endif
		/* Copy - otherwise ss goes output scope and junk memory would be referenced. */
		QString param = QString(ss.str().c_str());
		parameters.push_back(param);
	}
#endif
	return parameters;
}




/**
 * General information about Mapnik.
 *
 * Free the returned string after use.
 */
char * SlavGPS::mapnik_interface_about(void)
{
	char * msg = NULL;
#ifdef K
	/* Normally about 10 plugins so list them all. */
#if MAPNIK_VERSION >= 200200
	std::vector<std::string> plugins = mapnik::datasource_cache::instance().plugin_names();
#else
	std::vector<std::string> plugins = mapnik::datasource_cache::instance()->plugin_names();
#endif
	std::string str;
	for (int nn = 0; nn < plugins.size(); nn++) {
		str += plugins[nn] + ',';
	}
	str += '\n';
	/* NB Can have a couple hundred fonts loaded when using system directories.
	   So ATM don't list them all - otherwise need better GUI feedback display. */
	msg = g_strdup_printf(_("%s %s\nPlugins=%sFonts loaded=%d"),
			      _("Mapnik"),
			      MAPNIK_VERSION_STRING,
			      str.c_str(),
			      (unsigned int) mapnik::freetype_engine::face_names().size());
#endif
	return msg;
}
