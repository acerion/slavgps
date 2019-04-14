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
#include <mapnik/value.hpp>
#include <mapnik/image_util.hpp>
#include <mapnik/box2d.hpp>




#include <QPixmap>
#include <QDebug>




#include "layer_mapnik_wrapper.h"
#include "application_state.h"
#include "util.h"
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "Mapnik"









/* Can't change projection after init - but ATM only support drawing in Spherical Mercator. */
static mapnik::projection projection(mapnik::MAPNIK_GMERC_PROJ);




MapnikWrapper::MapnikWrapper()
{
}




MapnikWrapper::~MapnikWrapper()
{
}




void MapnikWrapper::initialize(const QString & plugins_dir, const QString & font_dir, bool font_dir_recurse)
{
	qDebug() << SG_PREFIX_D << "Using Mapnik version" << MAPNIK_VERSION_STRING;
	try {
		if (!plugins_dir.isEmpty()) {
			mapnik::datasource_cache::instance().register_datasources(plugins_dir.toUtf8().constData());
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
void MapnikWrapper::set_copyright(void)
{
	this->copyright = "";

	mapnik::parameters map_parameters = this->map.get_extra_parameters();
	if (map_parameters.get<std::string>("attribution")) {
		this->copyright = QString::fromStdString(*map_parameters.get<std::string>("attribution"));
	}

	if (this->copyright.isEmpty()) {
		if (map_parameters.get<std::string>("copyright")) {
			this->copyright = QString::fromStdString(*map_parameters.get<std::string>("copyright"));
		}
	}
}




#define VIK_SETTINGS_MAPNIK_BUFFER_SIZE "mapnik_buffer_size"




/**
   Returns empty string on success otherwise a string about what went wrong
*/
sg_ret MapnikWrapper::load_map_file(const QString & map_file_full_path, unsigned int width, unsigned int height, QString & msg)
{
	qDebug() << SG_PREFIX_I << "Loading map file" << map_file_full_path << "with width/height" << width << height;

	sg_ret ret = sg_ret::ok;
	try {
		this->map.remove_all(); /* Support reloading. */
		mapnik::load_map(this->map, map_file_full_path.toUtf8().constData());

		this->map.resize(width, height);
		this->map.set_srs(mapnik::MAPNIK_GMERC_PROJ); /* ONLY WEB MERCATOR output supported ATM. */

		/* IIRC This size is the number of pixels outside the tile to be considered so stuff is shown (i.e. particularly labels).
		   Only set buffer size if the buffer size isn't explicitly set in the mapnik stylesheet.
		   Alternatively render a bigger 'virtual' tile and then only use the appropriate subset */
		if (this->map.buffer_size() == 0) {
			int buffer_size = (width + height / 4); /* e.g. 128 for a 256x256 image. */
			int tmp;
			if (ApplicationState::get_integer(VIK_SETTINGS_MAPNIK_BUFFER_SIZE, &tmp)) {
				buffer_size = tmp;
			}
			qDebug() << SG_PREFIX_I << "Buffer size will be" << buffer_size;

			this->map.set_buffer_size(buffer_size);
		}
		this->set_copyright();

		qDebug() << QObject::tr("Debug: Mapnik: layers count: %1").arg(this->map.layer_count());
	} catch (std::exception const& ex) {
		msg = ex.what();
		ret = sg_ret::err;
	} catch (...) {
		msg = QObject::tr("unknown error");
		ret = sg_ret::err;
	}

	return ret;
}




/**
   Returns a pixmap of the specified area
*/
QPixmap MapnikWrapper::render_map(double lat_tl, double lon_tl, double lat_br, double lon_br)
{
	QPixmap result; /* Initially the pixmap returns true for ::isNull(). */

	try {
		/* Copy main object to local map variable.
		   This enables rendering to work when this function is called from different threads. */
		mapnik::Map & local_map = this->map; // TODO: this should be a copy?
		const unsigned width  = local_map.width();
		const unsigned height = local_map.height();

		/* Note projection & bbox want stuff in lon,lat order! */
		double p0x = lon_tl;
		double p0y = lat_tl;
		double p1x = lon_br;
		double p1y = lat_br;

		/* Convert into projection coordinates for bbox. */
		projection.forward(p0x, p0y);
		projection.forward(p1x, p1y);

		mapnik::box2d<double> bbox(p0x, p0y, p1x, p1y);
		qDebug() << SG_PREFIX_I << "Mapnik 2d box" << p0x << p0y << p1x << p1y;

		local_map.zoom_to_box(bbox);

		mapnik::image_rgba8 image(width, height);
		mapnik::agg_renderer<mapnik::image_rgba8> renderer(local_map, image);
		renderer.apply();

		if (image.painted()) {
			if (1) { /* Debug. */
				char buffer[64] = { 0 };
				mapnik::save_to_file(image, std::tmpnam(buffer), "png");
			}

			const size_t data_size = image.size();
			if (1) { /* Debug. */
				const size_t data_size_aux = width * height * 4; /* Four bytes per pixel: RGBA. */
				if (data_size != data_size_aux) {
					qDebug() << SG_PREFIX_W << "Unexpected image size calculations" << data_size << "!=" << data_size_aux;
				}
				qDebug() << SG_PREFIX_I << "Loading image from data with size" << data_size;
			}
			unsigned char * image_raw_data = (unsigned char *) malloc(data_size);
			if (!image_raw_data) {
				return result;
			}
			std::string buf = mapnik::save_to_string(image, "png");
			std::copy(buf.begin(), buf.end(), image_raw_data);

			const bool loaded = result.loadFromData(image_raw_data, data_size, "PNG");
			if (loaded) {
				qDebug() << SG_PREFIX_I << "Image successfully loaded from mapnik rendering";
			} else {
				qDebug() << SG_PREFIX_E << "Failed to load image from mapnik rendering";
			}
			free(image_raw_data);
			/* TODO_LATER: in original application the image_raw_data was not deallocated. */
		} else {
			qDebug() << QObject::tr("Warning: Mapnik: image not rendered");
		}
	}
	catch (const std::exception & ex) {
		qDebug() << QObject::tr("Error: Mapnik: An error occurred while rendering: %s", ex.what());
	}
	catch (...) {
		qDebug() << QObject::tr("Error: Mapnik: An unknown error occurred while rendering");
	}
	return result;
}




/**
   Copyright/Attribution information about the Map

   Returned string may be empty.
*/
QString MapnikWrapper::get_copyright(void) const
{
	return this->copyright;
}




/**
   'Parameter' information about the Map configuration.
*/
QStringList MapnikWrapper::get_parameters(void) const
{
	QStringList parameters;

	mapnik::parameters map_parameters = this->map.get_extra_parameters();
	/* Simply want the strings of each parameter so we can display something... */
	for (auto const& param : map_parameters) {
		std::stringstream ss;
		ss << param.first << ": " << *(map_parameters.get<std::string>(param.first,"empty"));
		/* Copy - otherwise ss goes output scope and junk memory would be referenced. */
		const QString param_string = QString(ss.str().c_str());
		parameters << param_string;
	}

	return parameters;
}




/**
   General information about Mapnik
*/
QString MapnikWrapper::about(void)
{
	QString msg;

	/* Normally about 10 plugins so list them all. */
	std::vector<std::string> plugins = mapnik::datasource_cache::instance().plugin_names();
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
