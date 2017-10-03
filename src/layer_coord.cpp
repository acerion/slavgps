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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cmath>
#include <cstdlib>

#include <QPen>
#include <QDebug>

#include "viewport_internal.h"
#include "layer_coord.h"
#include "util.h"




using namespace SlavGPS;




static ParameterScale scale_minutes_width  = { 0.05,  60.0,            SGVariant(1.0), 0.25,    10 }; /* PARAM_MIN_INC */
static ParameterScale scale_line_thickness = {    1,    15,    SGVariant((int32_t) 3),    1,     0 }; /* PARAM_LINE_THICKNESS */


static SGVariant color_default(void) { return SGVariant(255, 0, 0, 100); }


enum {
	PARAM_COLOR = 0,
	PARAM_MIN_INC,
	PARAM_LINE_THICKNESS,
	PARAM_MAX
};




static Parameter coord_layer_params[] = {
	{ PARAM_COLOR,          "color",          SGVariantType::COLOR,  PARAMETER_GROUP_GENERIC, N_("Color:"),          WidgetType::COLOR,          NULL,                  color_default,   NULL, NULL },
	{ PARAM_MIN_INC,        "min_inc",        SGVariantType::DOUBLE, PARAMETER_GROUP_GENERIC, N_("Minutes Width:"),  WidgetType::SPINBOX_DOUBLE, &scale_minutes_width,  NULL,            NULL, NULL },
	{ PARAM_LINE_THICKNESS, "line_thickness", SGVariantType::INT,    PARAMETER_GROUP_GENERIC, N_("Line Thickness:"), WidgetType::SPINBOX_INT,    &scale_line_thickness, NULL,            NULL, NULL },

	{ PARAM_MAX,            NULL,             SGVariantType::EMPTY,  PARAMETER_GROUP_GENERIC, NULL,                  WidgetType::NONE,           NULL,                  NULL,            NULL, NULL }, /* Guard. */
};




LayerCoordInterface vik_coord_layer_interface;




LayerCoordInterface::LayerCoordInterface()
{
	this->parameters_c = coord_layer_params;

	this->fixed_layer_type_string = "Coord"; /* Non-translatable. */

	// this->action_accelerator = ...; /* Empty accelerator. */
	// this->action_icon = ...; /* Set elsewhere. */

	this->menu_items_selection = LayerMenuItem::ALL;

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
	case PARAM_COLOR:
		qDebug() << "II: Layer Coordinate: saving color" << param_value.c.r << param_value.c.g << param_value.c.b << param_value.c.a;
		this->color.setRed(param_value.c.r);
		this->color.setGreen(param_value.c.g);
		this->color.setBlue(param_value.c.b);
		this->color.setAlpha(param_value.c.a);
		break;
	case PARAM_MIN_INC:
		this->deg_inc = param_value.d / 60.0;
		break;
	case PARAM_LINE_THICKNESS:
		if (param_value.i >= scale_line_thickness.min && param_value.i <= scale_line_thickness.max) {
			qDebug() << "II: Layer Coordinate: saving line thickness" << param_value.i;
			this->line_thickness = param_value.i;
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
	case PARAM_COLOR:
		rv.c.r = this->color.red();
		rv.c.g = this->color.green();
		rv.c.b = this->color.blue();
		rv.c.a = this->color.alpha();
		qDebug() << "II: Layer Coordinate: returning color" << rv.c.r << rv.c.g << rv.c.b << rv.c.a;
		break;
	case PARAM_MIN_INC:
		rv.d = this->deg_inc * 60.0;
		break;
	case PARAM_LINE_THICKNESS:
		rv.i = this->line_thickness;
		break;
	default:
		break;
	}
	return rv;
}




void LayerCoord::draw(Viewport * viewport)
{
	if (viewport->get_coord_mode() != CoordMode::UTM) {
		this->draw_latlon(viewport);
	}
	if (viewport->get_coord_mode() == CoordMode::UTM) {
		this->draw_utm(viewport);
	}

	/* Not really necessary, but keeping for now. */
	// viewport->sync();
}




void LayerCoord::draw_latlon(Viewport * viewport)
{
	QPen dgc(this->color);
	dgc.setWidth(this->line_thickness);
	QPen mgc(this->color);
	mgc.setWidth(MAX(this->line_thickness / 2, 1));
	QPen sgc(this->color);
	sgc.setWidth(MAX(this->line_thickness / 5, 1));

	int x1, y1, x2, y2;
#define CLINE(gc, c1, c2) {						\
		viewport->coord_to_screen((c1), &x1, &y1);		\
		viewport->coord_to_screen((c2), &x2, &y2);		\
		viewport->draw_line((gc), x1 + 1, y1 + 1, x2, y2);	\
	}


	bool minutes = false;
	bool seconds = false;
	int smod = 1;
	int mmod = 1;

	/* Vertical lines. */
	{
		Coord ul = viewport->screen_to_coord(0,                     0);
		Coord ur = viewport->screen_to_coord(viewport->get_width(), 0);
		Coord bl = viewport->screen_to_coord(0,                     viewport->get_height());

		const double min = ul.ll.lon;
		const double max = ur.ll.lon;

		if (60 * fabs(min - max) < 4) {
			seconds = true;
			smod = MIN(6, (int) ceil(3600 * fabs(min - max) / 30.0));
		}
		if (fabs(min - max) < 4) {
			minutes = true;
			mmod = MIN(6, (int) ceil(60 * fabs(min - max) / 30.0));
		}

		for (double i = floor(min * 60); i < ceil(max * 60); i += 1.0) {
			if (smod && seconds) {
				for (double j = i * 60 + 1; j < (i + 1) * 60; j += 1.0) {
					ul.ll.lon = j / 3600.0;
					bl.ll.lon = j / 3600.0;
					if ((int) j % smod == 0) {
						CLINE(sgc, &ul, &bl);
					}
				}
			}
			if (mmod && minutes) {
				ul.ll.lon = i / 60.0;
				bl.ll.lon = i / 60.0;
				if ((int) i % mmod == 0) {
					CLINE(mgc, &ul, &bl);
				}
			}
			if ((int) i % 60 == 0) {
				ul.ll.lon = i / 60.0;
				bl.ll.lon = i / 60.0;
				CLINE(dgc, &ul, &bl);
			}
		}
	}

	/* Horizontal lines. */
	{
		Coord ul = viewport->screen_to_coord(0,                     0);
		Coord ur = viewport->screen_to_coord(viewport->get_width(), 0);
		Coord bl = viewport->screen_to_coord(0,                     viewport->get_height());

		const double min = bl.ll.lat;
		const double max = ul.ll.lat;

		for (double i = floor(min * 60); i < ceil(max * 60); i += 1.0) {
			if (smod && seconds) {
				for (double j = i * 60 + 1; j < (i + 1) * 60; j += 1.0) {
					ul.ll.lat = j / 3600.0;
					ur.ll.lat = j / 3600.0;
					if ((int) j % smod == 0) {
						CLINE(sgc, &ul, &ur);
					}
				}
			}
			if (mmod && minutes) {
				ul.ll.lat = i / 60.0;
				ur.ll.lat = i / 60.0;
				if ((int) i % mmod == 0) {
					CLINE(mgc, &ul, &ur);
				}
			}
			if ((int) i % 60 == 0) {
				ul.ll.lat = i / 60.0;
				ur.ll.lat = i / 60.0;
				CLINE(dgc, &ul, &ur);
			}
		}
	}
#undef CLINE
#ifndef SLAVGPS_QT
	g_object_unref(dgc);
	g_object_unref(sgc);
	g_object_unref(mgc);
#endif
	return;
}





void LayerCoord::draw_utm(Viewport * viewport)
{
	QPen pen(this->color);
	pen.setWidth(this->line_thickness);

	const struct UTM center = viewport->get_center()->get_utm();
	double xmpp = viewport->get_xmpp(), ympp = viewport->get_ympp();
	uint16_t width = viewport->get_width(), height = viewport->get_height();
	struct LatLon ll, ll2, min, max;

	struct UTM utm = center;
	utm.northing = center.northing - (ympp * height / 2);

	a_coords_utm_to_latlon(&ll, &utm);

	utm.northing = center.northing + (ympp * height / 2);

	a_coords_utm_to_latlon(&ll2, &utm);

	{
		/*
		  Find corner coords in lat/lon.
		  Start at whichever is less: top or bottom left lon. Go to whichever more: top or bottom right lon.
		*/
		struct LatLon topleft, topright, bottomleft, bottomright;
		struct UTM temp_utm;
		temp_utm = center;
		temp_utm.easting -= (width/2)*xmpp;
		temp_utm.northing += (height/2)*ympp;
		a_coords_utm_to_latlon(&topleft, &temp_utm);
		temp_utm.easting += (width*xmpp);
		a_coords_utm_to_latlon(&topright, &temp_utm);
		temp_utm.northing -= (height*ympp);
		a_coords_utm_to_latlon(&bottomright, &temp_utm);
		temp_utm.easting -= (width*xmpp);
		a_coords_utm_to_latlon(&bottomleft, &temp_utm);
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
		a_coords_latlon_to_utm(&utm, &ll);
		int x1 = ((utm.easting - center.easting) / xmpp) + (width / 2);
		a_coords_latlon_to_utm(&utm, &ll2);
		int x2 = ((utm.easting - center.easting) / xmpp) + (width / 2);
		viewport->draw_line(pen, x1, height, x2, 0);
	}

	utm = center;
	utm.easting = center.easting - (xmpp * width / 2);

	a_coords_utm_to_latlon(&ll, &utm);

	utm.easting = center.easting + (xmpp * width / 2);

	a_coords_utm_to_latlon(&ll2, &utm);

	/* Really lat, just reusing a variable. */
	lon = ((double) ((long) ((min.lat)/ this->deg_inc))) * this->deg_inc;
	ll.lat = ll2.lat = lon;

	for (; ll.lat <= max.lat ; ll.lat += this->deg_inc, ll2.lat += this->deg_inc) {
		a_coords_latlon_to_utm(&utm, &ll);
		int x1 = (height / 2) - ((utm.northing - center.northing) / ympp);
		a_coords_latlon_to_utm(&utm, &ll2);
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
