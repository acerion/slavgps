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
 */

#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <cstdlib>
#include <vector>




#include <mapnik/version.hpp>
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




#include <QPixmap>
#include <QDebug>




#include "mapnik_interface.h"
#include "application_state.h"
#include "util.h"
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "Mapnik"





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





/* Can't change projection after init - but ATM only support drawing in Spherical Mercator. */
static mapnik::projection projection(mapnik::MAPNIK_GMERC_PROJ);




MapnikInterface::MapnikInterface()
{
	this->map = new mapnik::Map;
}




MapnikInterface::~MapnikInterface()
{
	delete this->map;
}




void MapnikInterface::initialize(const QString & plugins_dir, const QString & font_dir, bool font_dir_recurse)
{
	qDebug() << SG_PREFIX_D << "Using Mapnik version" << MAPNIK_VERSION_STRING;
	try {
		if (!plugins_dir.isEmpty()) {
#if MAPNIK_VERSION >= 200200
			mapnik::datasource_cache::instance().register_datasources(plugins_dir.toUtf8().constData());
#else
			mapnik::datasource_cache::instance()->register_datasources(plugins_dir.toUtf8().constData());
#endif
		}
		if (!font_dir.isEmpty()) {
			if (!mapnik::freetype_engine::register_fonts(font_dir.toUtf8().constData(), font_dir_recurse)) {
				qDebug() << QObject::tr("Warning: Mapnik: no fonts found");
			}
		}
	} catch (std::exception const& ex) {
		qDebug() << QObject::tr("Error: Mapnik: An error occurred while initialising mapnik: %1").arg(ex.what());
	} catch (...) {
		qDebug() << QObject::tr("Error: Mapnik: An unknown error occurred while initialising mapnik");
	}
}




/**
   Caching this answer instead of looking it up each time
*/
void MapnikInterface::set_copyright(void)
{
	this->copyright = "";

	mapnik::parameters map_parameters = this->map->get_extra_parameters();
#if MAPNIK_VERSION < 300000
	for (mapnik::parameters::const_iterator ii = map_parameters.begin(); ii != map_parameters.end(); ii++) {
		if (ii->first == "attribution" || ii->first == "copyright") {
			std::stringstream ss;
			ss << ii->second;
			// Copy it
			this->copyright = QString::fromStdString(ss);
			break;
		}
	}
#else
	if (map_parameters.get<std::string>("attribution")) {
		this->copyright = QString::fromStdString(*map_parameters.get<std::string>("attribution"));
	}

	if (this->copyright.isEmpty()) {
		if (map_parameters.get<std::string>("copyright")) {
			this->copyright = QString::fromStdString(*map_parameters.get<std::string>("copyright"));
		}
	}
#endif
}




#define VIK_SETTINGS_MAPNIK_BUFFER_SIZE "mapnik_buffer_size"




/**
   Returns empty string on success otherwise a string about what went wrong
*/
QString MapnikInterface::load_map_file(const QString & filename, unsigned int width, unsigned int height)
{
	QString msg;

	try {
		this->map->remove_all(); /* Support reloading. */
		mapnik::load_map(*this->map, filename.toUtf8().constData());

		this->map->resize(width,height);
		this->map->set_srs(mapnik::MAPNIK_GMERC_PROJ); /* ONLY WEB MERCATOR output supported ATM. */

		/* IIRC This size is the number of pixels outside the tile to be considered so stuff is shown (i.e. particularly labels).
		   Only set buffer size if the buffer size isn't explicitly set in the mapnik stylesheet.
		   Alternatively render a bigger 'virtual' tile and then only use the appropriate subset */
		if (this->map->buffer_size() == 0) {
			int buffer_size = (width + height / 4); /* e.g. 128 for a 256x256 image. */
			int tmp;
			if (ApplicationState::get_integer(VIK_SETTINGS_MAPNIK_BUFFER_SIZE, &tmp)) {
				buffer_size = tmp;
			}

			this->map->set_buffer_size(buffer_size);
		}
		this->set_copyright();

		qDebug() << QObject::tr("Debug: Mapnik: layers: %1").arg(this->map->layer_count());
	} catch (std::exception const& ex) {
		msg = ex.what();
	} catch (...) {
		msg = QObject::tr("unknown error");
	}

	return msg;
}




/**
   Returns a pixmap of the specified area
*/
QPixmap MapnikInterface::render_map(double lat_tl, double lon_tl, double lat_br, double lon_br)
{
	QPixmap result; /* Initially the pixmap returns true for ::isNull(). */

	/* Note projection & bbox want stuff in lon,lat order! */
	double p0x = lon_tl;
	double p0y = lat_tl;
	double p1x = lon_br;
	double p1y = lat_br;

	/* Convert into projection coordinates for bbox. */
	projection.forward(p0x, p0y);
	projection.forward(p1x, p1y);

	/* Copy main object to local map variable.
	   This enables rendering to work when this function is called from different threads. */
	mapnik::Map map_copy(*this->map);

	try {
		unsigned width  = map_copy.width();
		unsigned height = map_copy.height();
		mapnik::image_32 image(width, height);
		mapnik::box2d<double> bbox(p0x, p0y, p1x, p1y);
		map_copy.zoom_to_box(bbox);

		mapnik::agg_renderer<mapnik::image_32> render(map_copy, image);
		render.apply();

		if (image.painted()) {
			unsigned char * image_raw_data = (unsigned char *) malloc(width * 4 * height);
			if (!image_raw_data) {
				return result;
			}
			memcpy(image_raw_data, image.raw_data(), width * height * 4);
#ifdef K_FIXME_RESTORE
			/* TODO: QPixmap::loadFromData() ? */
			result = gdk_pixbuf_new_from_data(image_raw_data, GDK_COLORSPACE_RGB, TRUE, 8, width, height, width * 4, NULL, NULL);
			/* TODO: free image_raw_data? */
			/* TODO: in original application the image_raw_data was not deallocated. */
#endif
		} else {
			qDebug() << QObject::tr("Warning: Mapnik: image not rendered");
		}
	}
	catch (const std::exception & ex) {
		qDebug() << QObject::tr("Error: Mapnik: An error occurred while rendering: %s", ex.what());
	} catch (...) {
		qDebug() << QObject::tr("Error: Mapnik: An unknown error occurred while rendering");
	}
	return result;
}




/**
   Copyright/Attribution information about the Map

   Returned string may be empty.
*/
QString MapnikInterface::get_copyright(void) const
{
	return this->copyright;
}




/**
   'Parameter' information about the Map configuration.
*/
QStringList MapnikInterface::get_parameters(void) const
{
	QStringList parameters;

	mapnik::parameters map_parameters = this->map->get_extra_parameters();
	/* Simply want the strings of each parameter so we can display something... */
#if MAPNIK_VERSION < 300000
	for (mapnik::parameters::const_iterator param = map_parameters.begin(); param != map_parameters.end(); param++) {
		std::stringstream ss;
		ss << param->first << ": " << param->second;
#else
	for (auto const& param : map_parameters) {
		std::stringstream ss;
		ss << param.first << ": " << *(map_parameters.get<std::string>(param.first,"empty"));
#endif
		/* Copy - otherwise ss goes output scope and junk memory would be referenced. */
		const QString param_string = QString(ss.str().c_str());
		parameters << param_string;
	}

	return parameters;
}




/**
   General information about Mapnik
*/
QString MapnikInterface::about(void)
{
	QString msg;

	/* Normally about 10 plugins so list them all. */
#if MAPNIK_VERSION >= 200200
	std::vector<std::string> plugins = mapnik::datasource_cache::instance().plugin_names();
#else
	std::vector<std::string> plugins = mapnik::datasource_cache::instance()->plugin_names();
#endif
	std::string str;
	for (unsigned int nn = 0; nn < plugins.size(); nn++) {
		str += plugins[nn] + ',';
	}
	str += '\n';
	/* NB Can have a couple hundred fonts loaded when using system directories.
	   So ATM don't list them all - otherwise need better GUI feedback display. */
	msg = QObject::tr("Mapnik %1\nPlugins=%2Fonts loaded=%3")
		.arg(MAPNIK_VERSION_STRING)
		.arg(str.c_str())
		.arg(mapnik::freetype_engine::face_names().size());

	return msg;
}
