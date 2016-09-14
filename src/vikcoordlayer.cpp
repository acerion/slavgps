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

#ifdef HAVE_MATH_H
#include <math.h>
#endif
#include <glib/gi18n.h>
#include <stdlib.h>

#include "vikcoordlayer.h"
#include "icons/icons.h"





using namespace SlavGPS;





static Layer * coord_layer_unmarshall(uint8_t * data, int len, Viewport * viewport);




static LayerParamScale param_scales[] = {
	{ 0.05, 60.0, 0.25, 10 },
	{ 1, 15, 1, 0 },
};
static LayerParamData color_default(void)
{
	LayerParamData data;
	data.c.r = 1;
	data.c.g = 1;
	data.c.b = 1;
	data.c.a = 100;
	return data;
	// or: return VIK_LPD_COLOR (0, 65535, 0, 0);
}
static LayerParamData min_inc_default(void)
{
	return VIK_LPD_DOUBLE (1.0);
}
static LayerParamData line_thickness_default(void)
{
	return VIK_LPD_UINT (3);
}


enum {
	PARAM_COLOR = 0,
	PARAM_MIN_INC,
	PARAM_LINE_THICKNESS,
	NUM_PARAMS
};




static LayerParam coord_layer_params[] = {
	{ LayerType::COORD, PARAM_COLOR,          "color",          LayerParamType::COLOR,  VIK_LAYER_GROUP_NONE, N_("Color:"),          LayerWidgetType::COLOR,      NULL,             NULL, NULL, color_default,          NULL, NULL },
	{ LayerType::COORD, PARAM_MIN_INC,        "min_inc",        LayerParamType::DOUBLE, VIK_LAYER_GROUP_NONE, N_("Minutes Width:"),  LayerWidgetType::SPINBOX_DOUBLE, &param_scales[0], NULL, NULL, min_inc_default,        NULL, NULL },
	{ LayerType::COORD, PARAM_LINE_THICKNESS, "line_thickness", LayerParamType::UINT,   VIK_LAYER_GROUP_NONE, N_("Line Thickness:"), LayerWidgetType::SPINBUTTON, &param_scales[1], NULL, NULL, line_thickness_default, NULL, NULL },
};






VikLayerInterface vik_coord_layer_interface = {
	"Coord",
	N_("Coordinate"),
	NULL,
#ifdef SLAVGPS_QT
	NULL,
#else
	&vikcoordlayer_pixbuf,
#endif

	{ NULL, NULL, NULL, NULL, NULL, NULL, NULL },
	NULL,
	0,

	coord_layer_params,
	NUM_PARAMS,

	NULL,
	0,

	VIK_MENU_ITEM_ALL,

	/* (VikLayerFuncUnmarshall) */    coord_layer_unmarshall,

	/* (VikLayerFuncSetParam) */      layer_set_param,
	/* (VikLayerFuncGetParam) */      layer_get_param,
	/* (VikLayerFuncChangeParam) */   NULL,
};




static Layer * coord_layer_unmarshall(uint8_t * data, int len, Viewport * viewport)
{
	LayerCoord * layer = new LayerCoord();

	layer->unmarshall_params(data, len, viewport);
	layer->update_gc(viewport);

	return layer;
}





// NB Viewport can be null as it's not used ATM
bool LayerCoord::set_param(uint16_t id, LayerParamData data, Viewport * viewport, bool is_file_operation)
{
	fprintf(stderr, "set param %d\n", id);
	switch (id) {
	case PARAM_COLOR:
		fprintf(stderr, "++++ saving colors: %d %d %d %d\n", data.c.r, data.c.g, data.c.b, data.c.a);
		this->color.setRed(data.c.r);
		this->color.setGreen(data.c.g);
		this->color.setBlue(data.c.b);
		this->color.setAlpha(50);
		break;
	case PARAM_MIN_INC:
		this->deg_inc = data.d / 60.0;
		break;
	case PARAM_LINE_THICKNESS:
		fprintf(stderr, "set param %d line thickness %d\n", id, data.u);
		if (data.u >= 1 && data.u <= 15) {
			this->line_thickness = data.u;
		}
		break;
	default:
		break;
	}
	return true;
}





LayerParamData LayerCoord::get_param(uint16_t id, bool is_file_operation) const
{
	LayerParamData rv;
	switch (id) {
	case PARAM_COLOR:
		rv.c.r = this->color.red();
		rv.c.g = this->color.green();
		rv.c.b = this->color.blue();
		rv.c.a = this->color.alpha();
		fprintf(stderr, "++++ getting colors: %d %d %d %d\n", rv.c.r, rv.c.g, rv.c.b, rv.c.a);
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




void LayerCoord::post_read(Viewport * viewport, bool from_file)
{
	this->pen.setColor(this->color);
	this->pen.setWidth(this->line_thickness);
}




void LayerCoord::draw(Viewport * viewport)
{
#ifndef SLAVGPS_QT
	if (!this->gc) {
		return;
	}
#endif

	if (viewport->get_coord_mode() != VIK_COORD_UTM) {
		this->draw_latlon(viewport);
	}
	if (viewport->get_coord_mode() == VIK_COORD_UTM) {
		this->draw_utm(viewport);
	}
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
		VikCoord ul, ur, bl;
		viewport->screen_to_coord(0,                     0,                      &ul);
		viewport->screen_to_coord(viewport->get_width(), 0,                      &ur);
		viewport->screen_to_coord(0,                     viewport->get_height(), &bl);

		const double min = ul.east_west;
		const double max = ur.east_west;

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
					ul.east_west = j / 3600.0;
					bl.east_west = j / 3600.0;
					if ((int) j % smod == 0) {
						CLINE(sgc, &ul, &bl);
					}
				}
			}
			if (mmod && minutes) {
				ul.east_west = i / 60.0;
				bl.east_west = i / 60.0;
				if ((int) i % mmod == 0) {
					CLINE(mgc, &ul, &bl);
				}
			}
			if ((int) i % 60 == 0) {
				ul.east_west = i / 60.0;
				bl.east_west = i / 60.0;
				CLINE(dgc, &ul, &bl);
			}
		}
	}

	/* Horizontal lines. */
	{
		VikCoord ul, ur, bl;
		viewport->screen_to_coord(0,                     0,                      &ul);
		viewport->screen_to_coord(viewport->get_width(), 0,                      &ur);
		viewport->screen_to_coord(0,                     viewport->get_height(), &bl);

		const double min = bl.north_south;
		const double max = ul.north_south;

		for (double i = floor(min * 60); i < ceil(max * 60); i += 1.0) {
			if (smod && seconds) {
				for (double j = i * 60 + 1; j < (i + 1) * 60; j += 1.0) {
					ul.north_south = j / 3600.0;
					ur.north_south = j / 3600.0;
					if ((int) j % smod == 0) {
						CLINE(sgc, &ul, &ur);
					}
				}
			}
			if (mmod && minutes) {
				ul.north_south = i / 60.0;
				ur.north_south = i / 60.0;
				if ((int) i % mmod == 0) {
					CLINE(mgc, &ul, &ur);
				}
			}
			if ((int) i % 60 == 0) {
				ul.north_south = i / 60.0;
				ur.north_south = i / 60.0;
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
	const struct UTM * center = (const struct UTM *) viewport->get_center();
	double xmpp = viewport->get_xmpp(), ympp = viewport->get_ympp();
	uint16_t width = viewport->get_width(), height = viewport->get_height();
	struct LatLon ll, ll2, min, max;

	struct UTM utm = *center;
	utm.northing = center->northing - (ympp * height / 2);

	a_coords_utm_to_latlon(&utm, &ll);

	utm.northing = center->northing + (ympp * height / 2);

	a_coords_utm_to_latlon(&utm, &ll2);

	{
		/* find corner coords in lat/lon.
		   start at whichever is less: top or bottom left lon. goto whichever more: top or bottom right lon
		*/
		struct LatLon topleft, topright, bottomleft, bottomright;
		struct UTM temp_utm;
		temp_utm = *center;
		temp_utm.easting -= (width/2)*xmpp;
		temp_utm.northing += (height/2)*ympp;
		a_coords_utm_to_latlon(&temp_utm, &topleft);
		temp_utm.easting += (width*xmpp);
		a_coords_utm_to_latlon(&temp_utm, &topright);
		temp_utm.northing -= (height*ympp);
		a_coords_utm_to_latlon(&temp_utm, &bottomright);
		temp_utm.easting -= (width*xmpp);
		a_coords_utm_to_latlon(&temp_utm, &bottomleft);
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
		a_coords_latlon_to_utm(&ll, &utm);
		int x1 = ((utm.easting - center->easting) / xmpp) + (width / 2);
		a_coords_latlon_to_utm(&ll2, &utm);
		int x2 = ((utm.easting - center->easting) / xmpp) + (width / 2);
		viewport->draw_line(this->pen, x1, height, x2, 0);
	}

	utm = *center;
	utm.easting = center->easting - (xmpp * width / 2);

	a_coords_utm_to_latlon(&utm, &ll);

	utm.easting = center->easting + (xmpp * width / 2);

	a_coords_utm_to_latlon(&utm, &ll2);

	/* really lat, just reusing a variable */
	lon = ((double) ((long) ((min.lat)/ this->deg_inc))) * this->deg_inc;
	ll.lat = ll2.lat = lon;

	for (; ll.lat <= max.lat ; ll.lat += this->deg_inc, ll2.lat += this->deg_inc) {
		a_coords_latlon_to_utm (&ll, &utm);
		int x1 = (height / 2) - ((utm.northing - center->northing) / ympp);
		a_coords_latlon_to_utm (&ll2, &utm);
		int x2 = (height / 2) - ((utm.northing - center->northing) / ympp);
		viewport->draw_line(this->pen, width, x2, 0, x1);
	}
}





LayerCoord::~LayerCoord()
{
}





void LayerCoord::update_gc(Viewport * viewport)
{
	this->pen.setColor(this->color);
	this->pen.setWidth(this->line_thickness);
}





LayerCoord::LayerCoord()
{
	fprintf(stderr, "LayerCoord()\n");
	this->type = LayerType::COORD;
	strcpy(this->type_string, "COORD");

	this->color.setNamedColor("black");
	this->line_thickness = 3;

	this->rename("My Coord Layer"); /* kamilFIXME: this shouldn't be here. Shouldn't we get the default name from layer defaults or layer interface? */
}





LayerCoord::LayerCoord(Viewport * viewport)
{
	fprintf(stderr, "LayerCoord()\n");

	this->type = LayerType::COORD;
	strcpy(this->type_string, "COORD");

	this->color.setNamedColor("black");
	this->line_thickness = 3;


	this->set_defaults(viewport);
	if (viewport) {
		this->update_gc(viewport);
	}

	this->rename("My Coord Layer"); /* kamilFIXME: this shouldn't be here. Shouldn't we get the default name from layer defaults or layer interface? */
}
