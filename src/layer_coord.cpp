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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cmath>

#include <QPen>
#include <QDebug>

#include <glib.h>

#include "ui_builder.h"
#include "viewport_internal.h"
#include "layer_coord.h"
#include "util.h"
#include "measurements.h"




using namespace SlavGPS;




#define PREFIX ": Layer Coord:" << __FUNCTION__ << __LINE__ << ">"




static ParameterScale scale_minutes_width  = { 0.05,  60.0,            SGVariant(1.0), 0.25,    10 }; /* PARAM_MIN_INC */
static ParameterScale scale_line_thickness = {    1,    15,    SGVariant((int32_t) 3),    1,     0 }; /* PARAM_LINE_THICKNESS */


static SGVariant color_default_deg(void) { return SGVariant(QColor("blue")); }
static SGVariant color_default_min(void) { return SGVariant(QColor("black")); }
static SGVariant color_default_sec(void) { return SGVariant(QColor("blue")); }


enum {
	PARAM_COLOR_DEG = 0,
	PARAM_COLOR_MIN,
	PARAM_COLOR_SEC,
	PARAM_MIN_INC,
	PARAM_LINE_THICKNESS,
	PARAM_MAX
};




static ParameterSpecification coord_layer_param_specs[] = {
	{ PARAM_COLOR_DEG,      NULL, "color",          SGVariantType::Color,  PARAMETER_GROUP_GENERIC, QObject::tr("Color of degrees/UTM lines:"),  WidgetType::Color,          NULL,                  color_default_deg,  NULL, NULL },
	{ PARAM_COLOR_MIN,      NULL, "color_min",      SGVariantType::Color,  PARAMETER_GROUP_GENERIC, QObject::tr("Color of minutes lines:"),      WidgetType::Color,          NULL,                  color_default_min,  NULL, NULL },
	{ PARAM_COLOR_SEC,      NULL, "color_sec",      SGVariantType::Color,  PARAMETER_GROUP_GENERIC, QObject::tr("Color of seconds lines:"),      WidgetType::Color,          NULL,                  color_default_sec,  NULL, NULL },
	{ PARAM_MIN_INC,        NULL, "min_inc",        SGVariantType::Double, PARAMETER_GROUP_GENERIC, QObject::tr("Minutes Width:"),               WidgetType::SpinBoxDouble,  &scale_minutes_width,  NULL,               NULL, NULL },
	{ PARAM_LINE_THICKNESS, NULL, "line_thickness", SGVariantType::Int,    PARAMETER_GROUP_GENERIC, QObject::tr("Line Thickness:"),              WidgetType::SpinBoxInt,     &scale_line_thickness, NULL,               NULL, NULL },

	{ PARAM_MAX,            NULL, NULL,             SGVariantType::Empty,  PARAMETER_GROUP_GENERIC, QString(""),                                 WidgetType::None,           NULL,                  NULL,               NULL, NULL }, /* Guard. */
};




LayerCoordInterface vik_coord_layer_interface;




LayerCoordInterface::LayerCoordInterface()
{
	this->parameters_c = coord_layer_param_specs;

	this->fixed_layer_type_string = "Coord"; /* Non-translatable. */

	// this->action_accelerator = ...; /* Empty accelerator. */
	// this->action_icon = ...; /* Set elsewhere. */

	this->menu_items_selection = TreeItemOperation::All;

	this->ui_labels.new_layer = QObject::tr("New Coordinates Layer");
	this->ui_labels.layer_type = QObject::tr("Coordinates");
	this->ui_labels.layer_defaults = QObject::tr("Default Settings of Coordinates Layer");
}




Layer * LayerCoordInterface::unmarshall(uint8_t * data, size_t data_len, Viewport * viewport)
{
	LayerCoord * layer = new LayerCoord();
	layer->unmarshall_params(data, data_len);
	return layer;
}




/* Viewport can be NULL as it's not used ATM. */
bool LayerCoord::set_param_value(uint16_t id, const SGVariant & param_value, bool is_file_operation)
{
	switch (id) {
	case PARAM_COLOR_DEG:
		qDebug() << "II: Layer Coordinate:" << __FUNCTION__ << "saving degrees color" << param_value;
		this->color_deg = param_value.val_color;
		break;
	case PARAM_COLOR_MIN:
		qDebug() << "II: Layer Coordinate:" << __FUNCTION__ << "saving minutes color" << param_value;
		this->color_min = param_value.val_color;
		break;
	case PARAM_COLOR_SEC:
		qDebug() << "II: Layer Coordinate:" << __FUNCTION__ << "saving seconds color" << param_value;
		this->color_sec = param_value.val_color;
		break;
	case PARAM_MIN_INC:
		this->deg_inc = param_value.val_double / 60.0;
		break;
	case PARAM_LINE_THICKNESS:
		if (param_value.val_int >= scale_line_thickness.min && param_value.val_int <= scale_line_thickness.max) {
			qDebug() << "II: Layer Coordinate: saving line thickness" << param_value;
			this->line_thickness = param_value.val_int;
		}
		break;
	default:
		break;
	}
	return true;
}




SGVariant LayerCoord::get_param_value(param_id_t id, bool is_file_operation) const
{
	SGVariant rv;
	switch (id) {
	case PARAM_COLOR_DEG:
		rv = SGVariant(this->color_deg);
		break;
	case PARAM_COLOR_MIN:
		rv = SGVariant(this->color_min);
		break;
	case PARAM_COLOR_SEC:
		rv = SGVariant(this->color_sec);
		break;
	case PARAM_MIN_INC:
		rv = SGVariant((double) this->deg_inc * 60.0);
		break;
	case PARAM_LINE_THICKNESS:
		rv = SGVariant((int32_t) this->line_thickness);
		break;
	default:
		qDebug() << "EE: Layer Coordinate:" << __FUNCTION__ << "unexpected param id" << id;
		break;
	}
	qDebug() << "II: Layer Coordinate:" << __FUNCTION__ << "returning" << rv;
	return rv;
}




void LayerCoord::draw(Viewport * viewport)
{
	const CoordMode mode = viewport->get_coord_mode();
	switch (mode) {
	case CoordMode::LATLON:
		this->draw_latlon(viewport);
		break;
	case CoordMode::UTM:
		this->draw_utm(viewport);
		break;
	default:
		qDebug() << "EE" PREFIX << "unhandled viewport coord mode" << (int) mode;
		break;
	};

	/* Not really necessary, but keeping for now. */
	// viewport->sync();
}




/* We may want to draw only every N-th lat/lon line.
   Otherwise the non-skipped lines would be drawn too closely,
   completely obscuring other layers. */


int get_modulo_degrees(double width_in_degrees)
{
	if (width_in_degrees > 120) {
		return 6;
	} else if (width_in_degrees > 60) {
		return 3;
	} else {
		return 1;
	}
}

int get_modulo_minutes(double width_in_minutes)
{
	if (width_in_minutes > 120) {
		return 6;
	} else if (width_in_minutes > 60) {
		return 3;
	} else {
		return 1;
	}
}

int get_modulo_seconds(double width_in_seconds)
{
	if (width_in_seconds > 120) {
		return 6;
	} else if (width_in_seconds > 60) {
		return 3;
	} else {
		return 1;
	}
}




void LayerCoord::draw_latlon(Viewport * viewport)
{
	QPen degrees_pen(this->color_deg);
	degrees_pen.setWidth(this->line_thickness);
	QPen minutes_pen(this->color_min);
	minutes_pen.setWidth(MAX(this->line_thickness / 2, 1));
	QPen seconds_pen(this->color_sec);
	seconds_pen.setWidth(MAX(this->line_thickness / 5, 1));

	QPen text_pen(QColor("black"));
	QFont text_font("Helvetica", 10);

	bool draw_labels = true;


	int x1, y1, x2, y2;

#define DRAW_COORDINATE_LINE(pen, coord_begin, coord_end) {		\
		viewport->coord_to_screen_pos((coord_begin), &x1, &y1);	\
		viewport->coord_to_screen_pos((coord_end), &x2, &y2);	\
		viewport->draw_line((pen), x1 + 1, y1 + 1, x2, y2);	\
	}

#define DRAW_LONGITUDE_LINE(pen, coord_begin, coord_end, text) {	\
		viewport->coord_to_screen_pos((coord_begin), &x1, &y1);	\
		viewport->coord_to_screen_pos((coord_end), &x2, &y2);	\
		viewport->draw_line((pen), x1 + 1, y1 + 1, x2, y2);	\
		viewport->draw_text(text_font, text_pen, x1, y1 + 15, text); \
	}

#define DRAW_LATITUDE_LINE(pen, coord_begin, coord_end, text) {		\
		viewport->coord_to_screen_pos((coord_begin), &x1, &y1);	\
		viewport->coord_to_screen_pos((coord_end), &x2, &y2);	\
		viewport->draw_line((pen), x1 + 1, y1 + 1, x2, y2);	\
		viewport->draw_text(text_font, text_pen, x1, y1, text); \
	}


	const Coord ul = viewport->screen_pos_to_coord(0,                     0);
	const Coord ur = viewport->screen_pos_to_coord(viewport->get_width(), 0);
	const Coord bl = viewport->screen_pos_to_coord(0,                     viewport->get_height());

	const double minimum_lon = ul.ll.lon;
	const double maximum_lon = ur.ll.lon;
	const double minimum_lat = bl.ll.lat;
	const double maximum_lat = ul.ll.lat;

	const double width_degrees = fabs(minimum_lon - maximum_lon);
	const double width_minutes = 60.0 * width_degrees;
	const double width_seconds = 60.0 * width_minutes;

	const bool zoom_level_allows_for_seconds = width_seconds < 4 * 60;
	const int modulo_seconds = get_modulo_seconds(width_seconds);

	const bool zoom_level_allows_for_minutes = width_minutes < 4 * 60;
	const int modulo_minutes = get_modulo_minutes(width_minutes);

	const bool zoom_level_allows_for_degrees = true;
	const int modulo_degrees = get_modulo_degrees(width_degrees);


#if 1   /* Debug. */
	qDebug() << "-----------------color" << "seconds:" << seconds_pen.color().name() << ", minutes:" << minutes_pen.color().name() << "degrees:" << degrees_pen.color().name();
	qDebug() << "-----------------width" << width_seconds << "seconds," << width_minutes << "minutes," << width_degrees << "degrees, lat/lon:" << minimum_lon << maximum_lon;
	qDebug() << "--------------- modulo" << modulo_seconds << modulo_minutes << modulo_degrees;
	qDebug() << "------------------zoom" << zoom_level_allows_for_seconds << zoom_level_allows_for_minutes << zoom_level_allows_for_degrees;
#endif


	/* Vertical lines. */
	{
		Coord ul_local = ul;
		Coord bl_local = bl;

		const int minutes_start = (int) floor(minimum_lon * 60);
		const int minutes_end = (int) ceil(maximum_lon * 60);

		for (int minute = minutes_start; minute < minutes_end; minute++) {
			if (zoom_level_allows_for_seconds && modulo_seconds) {
				const int seconds_begin = minute * 60 + 1;
				const int seconds_end = (minute + 1) * 60;
				for (int second = seconds_begin; second < seconds_end; second++) {
					if (second % modulo_seconds == 0) {
						/* Seconds -> degrees. */
						ul_local.ll.lon = second / 3600.0;
						bl_local.ll.lon = second / 3600.0;

						if (draw_labels) {
							DRAW_LONGITUDE_LINE(seconds_pen, ul_local, bl_local, QString("%1''").arg(second % 60));
						} else {
							DRAW_COORDINATE_LINE(seconds_pen, ul_local, bl_local);
						}
					}
				}
			}

			if (zoom_level_allows_for_minutes && modulo_minutes) {
				if (minute % modulo_minutes == 0) {
					/* Minutes -> degrees. */
					ul_local.ll.lon = minute / 60.0;
					bl_local.ll.lon = minute / 60.0;

					if (draw_labels) {
						DRAW_LONGITUDE_LINE(minutes_pen, ul_local, bl_local, QString("%1'").arg(minute % 60));
					} else {
						DRAW_COORDINATE_LINE(minutes_pen, ul_local, bl_local);
					}
				}
			}

			if (zoom_level_allows_for_degrees && modulo_degrees) {
				const bool is_degree = minute % 60 == 0; /* We want to draw degrees and every 60-th minute is a degree. */
				const double degree = minute / 60.0;
				if (is_degree && (((int) degree) % modulo_degrees == 0)) {

					ul_local.ll.lon = degree;
					bl_local.ll.lon = degree;

					if (draw_labels) {
						DRAW_LONGITUDE_LINE(degrees_pen, ul_local, bl_local, QString("%1%2").arg((int) degree).arg(DEGREE_SYMBOL));
					} else {
						DRAW_COORDINATE_LINE(degrees_pen, ul_local, bl_local);
					}
				}
			}
		}
	}


	/* Horizontal lines. */
	{
		Coord ul_local = ul;
		Coord ur_local = ur;

		const int minutes_start = (int) floor(minimum_lat * 60);
		const int minutes_end = (int) ceil(maximum_lat * 60);

		for (int minute = minutes_start; minute < minutes_end; minute++) {
			if (zoom_level_allows_for_seconds && modulo_seconds) {
				const int seconds_begin = minute * 60 + 1;
				const int seconds_end = (minute + 1) * 60;
				for (int second = seconds_begin; second < seconds_end; second++) {
					if (second % modulo_seconds == 0) {
						/* Seconds -> degrees. */
						ul_local.ll.lat = second / 3600.0;
						ur_local.ll.lat = second / 3600.0;

						if (draw_labels) {
							DRAW_LATITUDE_LINE(seconds_pen, ul_local, ur_local, QString("%1''").arg(second % 60));
						} else {
							DRAW_COORDINATE_LINE(seconds_pen, ul_local, ur_local);
						}
					}
				}
			}

			if (zoom_level_allows_for_minutes && modulo_minutes) {
				if (minute % modulo_minutes == 0) {
					/* Minutes -> degrees. */
					ul_local.ll.lat = minute / 60.0;
					ur_local.ll.lat = minute / 60.0;

					if (draw_labels) {
						DRAW_LATITUDE_LINE(minutes_pen, ul_local, ur_local, QString("%1'").arg(minute % 60));
					} else {
						DRAW_COORDINATE_LINE(minutes_pen, ul_local, ur_local);
					}
				}
			}
			if (zoom_level_allows_for_degrees && modulo_degrees) {
				const bool is_degree = minute % 60 == 0; /* We want to draw degrees and every 60-th minute is a degree. */
				const double degree = minute / 60.0;
				if (is_degree && (((int) degree) % modulo_degrees == 0)) {

					/* Minutes -> degrees. */
					ul_local.ll.lat = degree;
					ur_local.ll.lat = degree;

					if (draw_labels) {
						DRAW_LATITUDE_LINE(degrees_pen, ul_local, ur_local, QString("%1%2").arg((int) degree).arg(DEGREE_SYMBOL));
					} else {
						DRAW_COORDINATE_LINE(degrees_pen, ul_local, ur_local);
					}
				}
			}
		}
	}
#undef DRAW_COORDINATE_LINE
#undef DRAW_LATITUDE_LINE
#undef DRAW_LONGITUDE_LINE
	return;
}





void LayerCoord::draw_utm(Viewport * viewport)
{
	QPen pen(this->color_deg);
	pen.setWidth(this->line_thickness);

	const UTM center = viewport->get_center()->get_utm();
	double xmpp = viewport->get_xmpp();
	double ympp = viewport->get_ympp();
	uint16_t width = viewport->get_width(), height = viewport->get_height();
	LatLon ll, ll2, min, max;

	UTM utm = center;
	utm.northing = center.northing - (ympp * height / 2);

	ll = UTM::to_latlon(utm);

	utm.northing = center.northing + (ympp * height / 2);

	ll2 = UTM::to_latlon(utm);

	{
		/*
		  Find corner coords in lat/lon.
		  Start at whichever is less: top or bottom left lon. Go to whichever more: top or bottom right lon.
		*/
		UTM temp_utm;
		temp_utm = center;
		temp_utm.easting -= (width/2)*xmpp;
		temp_utm.northing += (height/2)*ympp;
		const LatLon topleft = UTM::to_latlon(temp_utm);

		temp_utm.easting += (width*xmpp);
		const LatLon topright = UTM::to_latlon(temp_utm);

		temp_utm.northing -= (height*ympp);
		const LatLon bottomright = UTM::to_latlon(temp_utm);

		temp_utm.easting -= (width*xmpp);
		const LatLon bottomleft = UTM::to_latlon(temp_utm);

		min.lon = (topleft.lon < bottomleft.lon) ? topleft.lon : bottomleft.lon;
		max.lon = (topright.lon > bottomright.lon) ? topright.lon : bottomright.lon;
		min.lat = (bottomleft.lat < bottomright.lat) ? bottomleft.lat : bottomright.lat;
		max.lat = (topleft.lat > topright.lat) ? topleft.lat : topright.lat;
	}

	/* Can zoom out more than whole world and so the above can give invalid positions */
	/* Restrict values properly so drawing doesn't go into a near 'infinite' loop */
	if (min.lon < -180.0) {
		min.lon = -180.0;
	}

	if (max.lon > 180.0) {
		max.lon = 180.0;
	}

	if (min.lat < -90.0) {
		min.lat = -90.0;
	}

	if (max.lat > 90.0) {
		max.lat = 90.0;
	}

	double lon = ((double) ((long) ((min.lon)/ this->deg_inc))) * this->deg_inc;
	ll.lon = ll2.lon = lon;

	for (; ll.lon <= max.lon; ll.lon += this->deg_inc, ll2.lon += this->deg_inc) {
		utm = LatLon::to_utm(ll);
		int x1 = ((utm.easting - center.easting) / xmpp) + (width / 2);
		utm = LatLon::to_utm(ll2);
		int x2 = ((utm.easting - center.easting) / xmpp) + (width / 2);
		viewport->draw_line(pen, x1, height, x2, 0);
	}

	utm = center;
	utm.easting = center.easting - (xmpp * width / 2);

	ll = UTM::to_latlon(utm);

	utm.easting = center.easting + (xmpp * width / 2);

	ll2 = UTM::to_latlon(utm);

	/* Really lat, just reusing a variable. */
	lon = ((double) ((long) ((min.lat)/ this->deg_inc))) * this->deg_inc;
	ll.lat = ll2.lat = lon;

	for (; ll.lat <= max.lat ; ll.lat += this->deg_inc, ll2.lat += this->deg_inc) {
		utm = LatLon::to_utm(ll);
		int x1 = (height / 2) - ((utm.northing - center.northing) / ympp);
		utm = LatLon::to_utm(ll2);
		int x2 = (height / 2) - ((utm.northing - center.northing) / ympp);
		viewport->draw_line(pen, width, x2, 0, x1);
	}
}




LayerCoord::LayerCoord()
{
	this->type = LayerType::COORD;
	strcpy(this->debug_string, "LayerType::COORD");
	this->interface = &vik_coord_layer_interface;

	this->set_initial_parameter_values();
	this->set_name(Layer::get_type_ui_label());
}
