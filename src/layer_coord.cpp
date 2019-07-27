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




#include <cmath>




#include <QPen>
#include <QDebug>




#include "ui_builder.h"
#include "viewport_internal.h"
#include "layer_coord.h"
#include "util.h"
#include "measurements.h"




using namespace SlavGPS;




#define SG_MODULE "Coord Layer"




static ParameterScale<double> scale_minutes_width(0.05,  60.0,    SGVariant(1.0),         0.25,    10); /* PARAM_MIN_INC */
static ParameterScale<int>    scale_line_thickness(  1,    15,    SGVariant((int32_t) 3, SGVariantType::Int),    1,     0); /* PARAM_LINE_THICKNESS */


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
	{ PARAM_COLOR_DEG,      "color",          SGVariantType::Color,  PARAMETER_GROUP_GENERIC, QObject::tr("Color of degrees/UTM lines:"),  WidgetType::Color,          NULL,                  color_default_deg,  "" },
	{ PARAM_COLOR_MIN,      "color_min",      SGVariantType::Color,  PARAMETER_GROUP_GENERIC, QObject::tr("Color of minutes lines:"),      WidgetType::Color,          NULL,                  color_default_min,  "" },
	{ PARAM_COLOR_SEC,      "color_sec",      SGVariantType::Color,  PARAMETER_GROUP_GENERIC, QObject::tr("Color of seconds lines:"),      WidgetType::Color,          NULL,                  color_default_sec,  "" },
	{ PARAM_MIN_INC,        "min_inc",        SGVariantType::Double, PARAMETER_GROUP_GENERIC, QObject::tr("Minutes Width:"),               WidgetType::SpinBoxDouble,  &scale_minutes_width,  NULL,               "" },
	{ PARAM_LINE_THICKNESS, "line_thickness", SGVariantType::Int,    PARAMETER_GROUP_GENERIC, QObject::tr("Line Thickness:"),              WidgetType::SpinBoxInt,     &scale_line_thickness, NULL,               "" },
	{ PARAM_MAX,            "",               SGVariantType::Empty,  PARAMETER_GROUP_GENERIC, "",                                          WidgetType::None,           NULL,                  NULL,               "" }, /* Guard. */
};




LayerCoordInterface vik_coord_layer_interface;




LayerCoordInterface::LayerCoordInterface()
{
	this->parameters_c = coord_layer_param_specs;

	this->fixed_layer_type_string = "Coord"; /* Non-translatable. */

	// this->action_accelerator = ...; /* Empty accelerator. */
	// this->action_icon = ...; /* Set elsewhere. */

	this->ui_labels.new_layer = QObject::tr("New Coordinates Layer");
	this->ui_labels.layer_type = QObject::tr("Coordinates");
	this->ui_labels.layer_defaults = QObject::tr("Default Settings of Coordinates Layer");
}




Layer * LayerCoordInterface::unmarshall(Pickle & pickle, GisViewport * gisview)
{
	LayerCoord * layer = new LayerCoord();
	layer->unmarshall_params(pickle);
	return layer;
}




/* GisViewport can be NULL as it's not used ATM. */
bool LayerCoord::set_param_value(param_id_t param_id, const SGVariant & param_value, bool is_file_operation)
{
	switch (param_id) {
	case PARAM_COLOR_DEG:
		qDebug() << SG_PREFIX_I << "Saving degrees color" << param_value;
		this->color_deg = param_value.val_color;
		break;
	case PARAM_COLOR_MIN:
		qDebug() << SG_PREFIX_I << "Saving minutes color" << param_value;
		this->color_min = param_value.val_color;
		break;
	case PARAM_COLOR_SEC:
		qDebug() << SG_PREFIX_I << "Saving seconds color" << param_value;
		this->color_sec = param_value.val_color;
		break;
	case PARAM_MIN_INC:
		this->deg_inc = param_value.u.val_double / 60.0;
		break;
	case PARAM_LINE_THICKNESS:
		if (param_value.u.val_int >= scale_line_thickness.min && param_value.u.val_int <= scale_line_thickness.max) {
			qDebug() << SG_PREFIX_I << "Saving line thickness" << param_value;
			this->line_thickness = param_value.u.val_int;
		}
		break;
	default:
		break;
	}
	return true;
}




SGVariant LayerCoord::get_param_value(param_id_t param_id, bool is_file_operation) const
{
	SGVariant rv;
	switch (param_id) {
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
		rv = SGVariant((int32_t) this->line_thickness, coord_layer_param_specs[PARAM_LINE_THICKNESS].type_id);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unknown parameter id" << param_id;
		break;
	}
	qDebug() << SG_PREFIX_I << "Returning" << rv;
	return rv;
}




void LayerCoord::draw_tree_item(GisViewport * gisview, bool highlight_selected, bool parent_is_selected)
{
	const CoordMode mode = gisview->get_coord_mode();
	switch (mode) {
	case CoordMode::LatLon:
		this->draw_latlon(gisview);
		break;
	case CoordMode::UTM:
		this->draw_utm(gisview);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled viewport coord mode" << (int) mode;
		break;
	};

	/* Not really necessary, but keeping for now. */
	// gisview->sync();
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




#define DRAW_COORDINATE_LINE(pen, coord_begin, coord_end) {		\
		if (sg_ret::ok == gisview->coord_to_screen_pos(coord_begin, screen_pos_begin) \
		    && sg_ret::ok == gisview->coord_to_screen_pos(coord_end, screen_pos_end)) {	\
									\
			gisview->draw_line((pen), screen_pos_begin, screen_pos_end); \
			qDebug() << "kamil A:" << coord_begin << screen_pos_begin << coord_end << screen_pos_end; \
		}							\
	}

#define DRAW_LONGITUDE_LINE(pen, coord_begin, coord_end, text) {	\
		if (sg_ret::ok == gisview->coord_to_screen_pos(coord_begin, screen_pos_begin) \
		    && sg_ret::ok == gisview->coord_to_screen_pos(coord_end, screen_pos_end)) { \
									\
			gisview->draw_line((pen), screen_pos_begin, screen_pos_end); \
			gisview->draw_text(text_font, text_pen, screen_pos_begin.x(), screen_pos_begin.y() + 15, text); \
			qDebug() << "kamil B:" << coord_begin << screen_pos_begin << coord_end << screen_pos_end; \
		}							\
	}

#define DRAW_LATITUDE_LINE(pen, coord_begin, coord_end, text) {		\
		if (sg_ret::ok == gisview->coord_to_screen_pos(coord_begin, screen_pos_begin) \
		    && sg_ret::ok == gisview->coord_to_screen_pos(coord_end, screen_pos_end)) {	\
									\
			gisview->draw_line((pen), screen_pos_begin, screen_pos_end); \
			gisview->draw_text(text_font, text_pen, screen_pos_begin.x(), screen_pos_begin.y(), text); \
			qDebug() << "kamil C:" << coord_begin << screen_pos_begin << coord_end << screen_pos_end; \
		}							\
	}




void LayerCoord::draw_latlon(GisViewport * gisview)
{
	QPen degrees_pen(this->color_deg);
	degrees_pen.setWidth(this->line_thickness);
	QPen minutes_pen(this->color_min);
	minutes_pen.setWidth(std::max(this->line_thickness / 2, 1));
	QPen seconds_pen(this->color_sec);
	seconds_pen.setWidth(std::max(this->line_thickness / 5, 1));

	QPen text_pen(QColor("black"));
	QFont text_font("Helvetica", 10);

	bool draw_labels = true;

	ScreenPos screen_pos_begin;
	ScreenPos screen_pos_end;



	const LatLonBBox bbox = gisview->get_bbox();
	const double minimum_lon = bbox.west.get_value();
	const double maximum_lon = bbox.east.get_value();
	const double minimum_lat = bbox.south.get_value();
	const double maximum_lat = bbox.north.get_value();


	const double width_degrees = std::fabs(minimum_lon - maximum_lon);
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
		Coord ul_local = Coord(LatLon(maximum_lat, minimum_lon), CoordMode::LatLon);
		Coord bl_local = Coord(LatLon(minimum_lat, minimum_lon), CoordMode::LatLon);

		const int minutes_start = (int) floor(minimum_lon * 60);
		const int minutes_end = (int) ceil(maximum_lon * 60);

		for (int minute = minutes_start; minute < minutes_end; minute++) {
			if (zoom_level_allows_for_seconds && modulo_seconds) {
				const int seconds_begin = minute * 60 + 1;
				const int seconds_end = (minute + 1) * 60;
				for (int second = seconds_begin; second < seconds_end; second++) {
					if (second % modulo_seconds == 0) {
						/* Seconds -> degrees. */
						ul_local.lat_lon.lon = second / 3600.0;
						bl_local.lat_lon.lon = second / 3600.0;

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
					ul_local.lat_lon.lon = minute / 60.0;
					bl_local.lat_lon.lon = minute / 60.0;

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

					ul_local.lat_lon.lon = degree;
					bl_local.lat_lon.lon = degree;

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
		Coord ul_local = Coord(LatLon(maximum_lat, minimum_lon), CoordMode::LatLon);
		Coord ur_local = Coord(LatLon(maximum_lat, maximum_lon), CoordMode::LatLon);

		const int minutes_start = (int) floor(minimum_lat * 60);
		const int minutes_end = (int) ceil(maximum_lat * 60);

		for (int minute = minutes_start; minute < minutes_end; minute++) {
			if (zoom_level_allows_for_seconds && modulo_seconds) {
				const int seconds_begin = minute * 60 + 1;
				const int seconds_end = (minute + 1) * 60;
				for (int second = seconds_begin; second < seconds_end; second++) {
					if (second % modulo_seconds == 0) {
						/* Seconds -> degrees. */
						ul_local.lat_lon.lat = second / 3600.0;
						ur_local.lat_lon.lat = second / 3600.0;

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
					ul_local.lat_lon.lat = minute / 60.0;
					ur_local.lat_lon.lat = minute / 60.0;

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
					ul_local.lat_lon.lat = degree;
					ur_local.lat_lon.lat = degree;

					if (draw_labels) {
						DRAW_LATITUDE_LINE(degrees_pen, ul_local, ur_local, QString("%1%2").arg((int) degree).arg(DEGREE_SYMBOL));
					} else {
						DRAW_COORDINATE_LINE(degrees_pen, ul_local, ur_local);
					}
				}
			}
		}
	}

	return;
}





void LayerCoord::draw_utm(GisViewport * gisview)
{
	QPen pen(this->color_deg);
	pen.setWidth(this->line_thickness);

#if 1
	QPen degrees_pen(this->color_deg);
	degrees_pen.setWidth(this->line_thickness);
	QPen minutes_pen(this->color_min);
	minutes_pen.setWidth(std::max(this->line_thickness / 2, 1));
	QPen seconds_pen(this->color_sec);
	seconds_pen.setWidth(std::max(this->line_thickness / 5, 1));

	QPen text_pen(QColor("black"));
	QFont text_font("Helvetica", 10);

	bool draw_labels = true;

	ScreenPos screen_pos_begin;
	ScreenPos screen_pos_end;


	const LatLonBBox bbox = gisview->get_bbox();
	const double minimum_lon = bbox.west.get_value();
	const double maximum_lon = bbox.east.get_value();
	const double minimum_lat = bbox.south.get_value();
	const double maximum_lat = bbox.north.get_value();


	const double width_degrees = std::fabs(minimum_lon - maximum_lon);


#if 1   /* Debug. */
	qDebug() << "-----------------width" << width_degrees << "degrees, lat/lon:" << minimum_lon << maximum_lon;
#endif


	/* Vertical lines. */
	{
		Coord ul_local = Coord(LatLon(maximum_lat, minimum_lon), CoordMode::LatLon);
		Coord bl_local = Coord(LatLon(minimum_lat, minimum_lon), CoordMode::LatLon);

		const int interval = 6; /* Every zone is 6 degrees wide. */

		int n;
		n = floor(minimum_lon / interval);
		const int degrees_start = n * interval - interval;
		n = ceil(maximum_lon / interval);
		const int degrees_end = n * interval + interval;

		for (int degree = degrees_start; degree < degrees_end; degree += interval) {
			ul_local.lat_lon.lon = degree;
			bl_local.lat_lon.lon = degree;
			Coord ul = ul_local;
			Coord bl = bl_local;
			ul.recalculate_to_mode(CoordMode::UTM);
			bl.recalculate_to_mode(CoordMode::UTM);

			if (draw_labels) {
				DRAW_LONGITUDE_LINE(degrees_pen, ul, bl, QString("%1%2").arg(degree).arg(DEGREE_SYMBOL));
			} else {
				DRAW_COORDINATE_LINE(degrees_pen, ul, bl);
			}
		}
	}


	/* Horizontal lines. */
	{
		Coord ul_local = Coord(LatLon(maximum_lat, minimum_lon), CoordMode::LatLon);
		Coord ur_local = Coord(LatLon(maximum_lat, maximum_lon), CoordMode::LatLon);

		const int interval = 8; /* Every band is 8 degrees high. */

		int n;
		n = floor(minimum_lat / interval);
		const int degrees_start = n * interval - interval;
		n = ceil(maximum_lat / interval);
		const int degrees_end = n * interval + interval;

		for (int degree = degrees_start; degree < degrees_end; degree += interval) {
			ul_local.lat_lon.lat = degree;
			ur_local.lat_lon.lat = degree;
			Coord ul = ul_local;
			Coord ur = ur_local;
			ul.recalculate_to_mode(CoordMode::UTM);
			ur.recalculate_to_mode(CoordMode::UTM);

			if (draw_labels) {
				DRAW_LATITUDE_LINE(degrees_pen, ul, ur, QString("%1%2").arg(degree).arg(DEGREE_SYMBOL));
			} else {
				DRAW_COORDINATE_LINE(degrees_pen, ul, ur);
			}
		}
	}

#else

	const UTM center = gisview->get_center_coord().get_utm();
	const double xmpp = gisview->get_viking_scale().get_x();
	const double ympp = gisview->get_viking_scale().get_y();
	const int width = gisview->central_get_width();
	const int height = gisview->central_get_height();


	LatLon min, max;
	{
		/*
		  Find corner coords in lat/lon.
		  Start at whichever is less: top or bottom left lon. Go to whichever more: top or bottom right lon.
		*/
		UTM temp_utm;
		temp_utm = center;

		temp_utm.easting = temp_utm.easting - (width / 2) * xmpp;
		temp_utm.northing = temp_utm.northing + (height / 2) * ympp;
		const LatLon topleft = UTM::to_lat_lon(temp_utm);

		temp_utm = center;
		temp_utm.easting = temp_utm.easting + (width / 2 * xmpp);
		const LatLon topright = UTM::to_lat_lon(temp_utm);

		temp_utm = center;
		temp_utm.northing = temp_utm.northing - (height / 2 * ympp);
		const LatLon bottomright = UTM::to_lat_lon(temp_utm);

		temp_utm = center;
		temp_utm.easting = temp_utm.easting - (width / 2 * xmpp);
		const LatLon bottomleft = UTM::to_lat_lon(temp_utm);

		min.lon = (topleft.lon < bottomleft.lon) ? topleft.lon : bottomleft.lon;
		max.lon = (topright.lon > bottomright.lon) ? topright.lon : bottomright.lon;
		min.lat = (bottomleft.lat < bottomright.lat) ? bottomleft.lat : bottomright.lat;
		max.lat = (topleft.lat > topright.lat) ? topleft.lat : topright.lat;



		/* Can zoom out more than whole world and so the above can give invalid positions */
		/* Restrict values properly so drawing doesn't go into a near 'infinite' loop */
		if (min.lon < SG_LONGITUDE_MIN) {
			min.lon = SG_LONGITUDE_MIN;
		}

		if (max.lon > SG_LONGITUDE_MAX) {
			max.lon = SG_LONGITUDE_MAX;
		}

		if (min.lat < SG_LATITUDE_MIN) {
			min.lat = SG_LATITUDE_MIN;
		}

		if (max.lat > SG_LATITUDE_MAX) {
			max.lat = SG_LATITUDE_MAX;
		}

		qDebug() << "============= min/max latitude = " << min << max << gisview->get_bbox();

		LatLonBBox bbox = gisview->get_bbox();
		min.lat = bbox.south.get_value();
		min.lon = bbox.west.get_value();
		max.lat = bbox.north.get_value();
		max.lon = bbox.east.get_value();
	}


	/* Vertical lines. */
	if (1) {
		UTM utm = center;

		const double degrees_delta = this->deg_inc;

		/* Distance from center to either upper or lower edge
		   of pixmap that we draw to. [meters] */
		const double vert_distance_m = (height / 2) * ympp;

		utm.northing = center.get_northing() - vert_distance_m;
		LatLon lat_lon_bottom = UTM::to_lat_lon(utm);

		utm.northing = center.get_northing() + vert_distance_m;
		LatLon lat_lon_top = UTM::to_lat_lon(utm);


		double lon = ((double) ((long) (min.lon / degrees_delta))) * degrees_delta;
		lat_lon_bottom.lon = lon;
		lat_lon_top.lon = lon;

		ScreenPos begin;
		ScreenPos end;
		begin.y = height;
		end.y = 0;

		for (;
		     lat_lon_bottom.lon <= max.lon,       lat_lon_top.lon <= max.lon;
		     lat_lon_bottom.lon += degrees_delta, lat_lon_top.lon += degrees_delta) {

			utm = LatLon::to_utm(lat_lon_bottom);
			begin.x = ((utm.easting - center.easting) / xmpp) + (width / 2);

			utm = LatLon::to_utm(lat_lon_top);
			end.x = ((utm.easting - center.easting) / xmpp) + (width / 2);

			gisview->draw_line(pen, begin, end);
		}
	}



	/* Horizontal lines. */
	if (1) {
		UTM utm = center;

		const double degrees_delta = this->deg_inc;

		/* Distance from center to either left or right edge
		   of pixmap that we draw to. [meters] */
		const double horiz_distance_m = (width / 2) * xmpp;

		utm.easting = center.easting - horiz_distance_m;
		LatLon lat_lon_left = UTM::to_lat_lon(utm);

		utm.easting = center.easting + horiz_distance_m;
		LatLon lat_lon_right = UTM::to_lat_lon(utm);

		const double lat = ((double) ((long) (min.lat / degrees_delta))) * degrees_delta;
		lat_lon_left.lat = lat;
		lat_lon_right.lat = lat;

		ScreenPos begin;
		ScreenPos end;
		begin.x = width;
		end.x = 0;

		for (;
		     lat_lon_left.lat <= max.lat,       lat_lon_right.lat <= max.lat;
		     lat_lon_left.lat += degrees_delta, lat_lon_right.lat += degrees_delta) {

			utm = LatLon::to_utm(lat_lon_left);
			end.y = (height / 2) - ((utm.get_northing() - center.get_northing()) / ympp);

			utm = LatLon::to_utm(lat_lon_right);
			begin.y = (height / 2) - ((utm.get_northing() - center.get_northing()) / ympp);

			gisview->draw_line(pen, begin, end);
		}
	}
#endif
}
#undef DRAW_COORDINATE_LINE
#undef DRAW_LATITUDE_LINE
#undef DRAW_LONGITUDE_LINE




LayerCoord::LayerCoord()
{
	this->type = LayerType::Coordinates;
	strcpy(this->debug_string, "LayerType::COORD");
	this->interface = &vik_coord_layer_interface;

	this->set_initial_parameter_values();
	this->set_name(Layer::get_type_ui_label());
}
