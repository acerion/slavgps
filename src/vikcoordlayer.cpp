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

#include "viking.h"
#include "vikcoordlayer.h"
#include "icons/icons.h"

static VikCoordLayer *coord_layer_new(VikViewport *vp);
static void coord_layer_free(VikCoordLayer *vcl);
static VikCoordLayer *coord_layer_create(VikViewport *vp);
static VikCoordLayer *coord_layer_unmarshall(uint8_t *data, int len, VikViewport *vvp);
static bool coord_layer_set_param(VikCoordLayer *vcl, uint16_t id, VikLayerParamData data, VikViewport *vp, bool is_file_operation);
static VikLayerParamData coord_layer_get_param(VikCoordLayer *vcl, uint16_t id, bool is_file_operation);


static VikLayerParamScale param_scales[] = {
	{ 0.05, 60.0, 0.25, 10 },
	{ 1, 10, 1, 0 },
};

static VikLayerParamData color_default(void) {
	VikLayerParamData data; gdk_color_parse("red", &data.c); return data;
  // or: return VIK_LPD_COLOR (0, 65535, 0, 0);
}
static VikLayerParamData min_inc_default(void) { return VIK_LPD_DOUBLE(1.0); }
static VikLayerParamData line_thickness_default(void) { return VIK_LPD_UINT(3); }

static VikLayerParam coord_layer_params[] = {
  { VIK_LAYER_COORD, "color", VIK_LAYER_PARAM_COLOR, VIK_LAYER_GROUP_NONE, N_("Color:"), VIK_LAYER_WIDGET_COLOR, NULL, NULL, NULL, color_default, NULL, NULL },
  { VIK_LAYER_COORD, "min_inc", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_GROUP_NONE, N_("Minutes Width:"), VIK_LAYER_WIDGET_SPINBUTTON, &param_scales[0], NULL, NULL, min_inc_default, NULL, NULL },
  { VIK_LAYER_COORD, "line_thickness", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Line Thickness:"), VIK_LAYER_WIDGET_SPINBUTTON, &param_scales[1], NULL, NULL, line_thickness_default, NULL, NULL },
};

enum { PARAM_COLOR = 0, PARAM_MIN_INC, PARAM_LINE_THICKNESS, NUM_PARAMS };

VikLayerInterface vik_coord_layer_interface = {
	"Coord",
	N_("Coordinate"),
	NULL,
	&vikcoordlayer_pixbuf,

	NULL,
	0,

	coord_layer_params,
	NUM_PARAMS,
	NULL,
	0,

	VIK_MENU_ITEM_ALL,

	(VikLayerFuncCreate)                  coord_layer_create,
	(VikLayerFuncRealize)                 NULL,
	(VikLayerFuncFree)                    coord_layer_free,

	(VikLayerFuncUnmarshall)		coord_layer_unmarshall,

	(VikLayerFuncSetParam)                coord_layer_set_param,
	(VikLayerFuncGetParam)                coord_layer_get_param,
	(VikLayerFuncChangeParam)             NULL,
};

GType vik_coord_layer_get_type()
{
	static GType vcl_type = 0;

	if (!vcl_type) {
		static const GTypeInfo vcl_info = {
			sizeof (VikCoordLayerClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			NULL, /* class init */
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (VikCoordLayer),
			0,
			NULL /* instance init */
		};
		vcl_type = g_type_register_static(VIK_LAYER_TYPE, "VikCoordLayer", &vcl_info, (GTypeFlags) 0);
	}

	return vcl_type;
}

void LayerCoord::marshall(uint8_t **data, int *len)
{
	VikCoordLayer *vcl = (VikCoordLayer *) this->vl;
	vik_layer_marshall_params(VIK_LAYER(vcl), data, len);
}

static VikCoordLayer *coord_layer_unmarshall(uint8_t *data, int len, VikViewport *vvp)
{
	VikCoordLayer *rv = coord_layer_new(vvp);
	vik_layer_unmarshall_params(VIK_LAYER(rv), data, len, vvp);

	LayerCoord * layer = (LayerCoord *) ((VikLayer * ) rv)->layer;
	layer->update_gc(&vvp->port);
	return rv;
}

// NB VikViewport can be null as it's not used ATM
bool coord_layer_set_param(VikCoordLayer *vcl, uint16_t id, VikLayerParamData data, VikViewport *vp, bool is_file_operation)
{
	LayerCoord * layer = (LayerCoord *) ((VikLayer * ) vcl)->layer;
	switch (id) {
	case PARAM_COLOR:
		layer->color = data.c;
		break;
	case PARAM_MIN_INC:
		layer->deg_inc = data.d / 60.0;
		break;
	case PARAM_LINE_THICKNESS:
		if (data.u >= 1 && data.u <= 15) {
			layer->line_thickness = data.u;
		}
		break;
	default:
		break;
	}
	return true;
}

static VikLayerParamData coord_layer_get_param(VikCoordLayer *vcl, uint16_t id, bool is_file_operation)
{
	LayerCoord * layer = (LayerCoord *) ((VikLayer * ) vcl)->layer;

	VikLayerParamData rv;
	switch (id) {
	case PARAM_COLOR:
		rv.c = layer->color;
		break;
	case PARAM_MIN_INC:
		rv.d = layer->deg_inc * 60.0;
		break;
	case PARAM_LINE_THICKNESS:
		rv.i = layer->line_thickness;
		break;
	default:
		break;
	}
	return rv;
}

void LayerCoord::post_read(Viewport * viewport, bool from_file)
{
	if (this->gc) {
		g_object_unref(G_OBJECT(this->gc));
	}

	this->gc = viewport->new_gc_from_color(&(this->color), this->line_thickness);
}

static VikCoordLayer *coord_layer_new(VikViewport *vvp)
{
	VikCoordLayer *vcl = VIK_COORD_LAYER(g_object_new(VIK_COORD_LAYER_TYPE, NULL));
	vik_layer_set_type(VIK_LAYER(vcl), VIK_LAYER_COORD);

	((VikLayer *) vcl)->layer = new LayerCoord((VikLayer *) vcl);

	vik_layer_set_defaults(VIK_LAYER(vcl), vvp);

	LayerCoord * layer = (LayerCoord *) ((VikLayer * ) vcl)->layer;

	layer->gc = NULL;

	return vcl;
}

void LayerCoord::draw(Viewport * viewport)
{
	if (!this->gc) {
		return;
	}

	if (viewport->get_coord_mode() != VIK_COORD_UTM) {
		VikCoord left, right, left2, right2;
		double l, r, i, j;
		int x1, y1, x2, y2, smod = 1, mmod = 1;
		bool mins = false, secs = false;
		GdkGC *dgc = viewport->new_gc_from_color(&(this->color), this->line_thickness);
		GdkGC *mgc = viewport->new_gc_from_color(&(this->color), MAX(this->line_thickness/2, 1));
		GdkGC *sgc = viewport->new_gc_from_color(&(this->color), MAX(this->line_thickness/5, 1));

		viewport->screen_to_coord(0, 0, &left);
		viewport->screen_to_coord(viewport->get_width(), 0, &right);
		viewport->screen_to_coord(0, viewport->get_height(), &left2);
		viewport->screen_to_coord(viewport->get_width(), viewport->get_height(), &right2);

#define CLINE(gc, c1, c2) {			      \
			viewport->coord_to_screen((c1), &x1, &y1);	\
			viewport->coord_to_screen((c2), &x2, &y2);	\
			viewport->draw_line((gc), x1, y1, x2, y2);	\
		}

		l = left.east_west;
		r = right.east_west;
		if (60*fabs(l-r) < 4) {
			secs = true;
			smod = MIN(6, (int)ceil(3600*fabs(l-r)/30.0));
		}
		if (fabs(l-r) < 4) {
			mins = true;
			mmod = MIN(6, (int)ceil(60*fabs(l-r)/30.0));
		}
		for (i = floor(l*60); i < ceil(r*60); i += 1.0) {
			if (secs) {
				for (j = i*60+1; j < (i+1)*60; j += 1.0) {
					left.east_west = j/3600.0;
					left2.east_west = j/3600.0;
					if ((int)j % smod == 0) CLINE(sgc, &left, &left2);
				}
			}
			if (mins) {
				left.east_west = i/60.0;
				left2.east_west = i/60.0;
				if ((int)i % mmod == 0) CLINE(mgc, &left, &left2);
			}
			if ((int)i % 60 == 0) {
				left.east_west = i/60.0;
				left2.east_west = i/60.0;
				CLINE(dgc, &left, &left2);
			}
		}

		viewport->screen_to_coord(0, 0, &left);
		l = left2.north_south;
		r = left.north_south;
		for (i = floor(l*60); i < ceil(r*60); i += 1.0) {
			if (secs) {
				for (j = i*60+1; j < (i+1)*60; j += 1.0) {
					left.north_south = j/3600.0;
					right.north_south = j/3600.0;
					if ((int)j % smod == 0) CLINE(sgc, &left, &right);
				}
			}
			if (mins) {
				left.north_south = i/60.0;
				right.north_south = i/60.0;
				if ((int)i % mmod == 0) CLINE(mgc, &left, &right);
			}
			if ((int)i % 60 == 0) {
				left.north_south = i/60.0;
				right.north_south = i/60.0;
				CLINE(dgc, &left, &right);
			}
		}
#undef CLINE
		g_object_unref(dgc);
		g_object_unref(sgc);
		g_object_unref(mgc);
		return;
	}

	if (viewport->get_coord_mode() == VIK_COORD_UTM) {
		const struct UTM *center = (const struct UTM *) viewport->get_center();
		double xmpp = viewport->get_xmpp(), ympp = viewport->get_ympp();
		uint16_t width = viewport->get_width(), height = viewport->get_height();
		struct LatLon ll, ll2, min, max;
		double lon;
		int x1, x2;
		struct UTM utm;

		utm = *center;
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

		lon = ((double) ((long) ((min.lon)/ this->deg_inc))) * this->deg_inc;
		ll.lon = ll2.lon = lon;

		for (; ll.lon <= max.lon; ll.lon += this->deg_inc, ll2.lon += this->deg_inc) {
			a_coords_latlon_to_utm(&ll, &utm);
			x1 = ((utm.easting - center->easting) / xmpp) + (width / 2);
			a_coords_latlon_to_utm(&ll2, &utm);
			x2 = ((utm.easting - center->easting) / xmpp) + (width / 2);
			viewport->draw_line(this->gc, x1, height, x2, 0);
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
			x1 = (height / 2) - ((utm.northing - center->northing) / ympp);
			a_coords_latlon_to_utm (&ll2, &utm);
			x2 = (height / 2) - ((utm.northing - center->northing) / ympp);
			viewport->draw_line(this->gc, width, x2, 0, x1);
		}
	}
}

static void coord_layer_free(VikCoordLayer *vcl)
{
	LayerCoord * layer = (LayerCoord *) ((VikLayer * ) vcl)->layer;
	if (layer->gc != NULL) {
		g_object_unref(G_OBJECT(layer->gc));
	}
}

void LayerCoord::update_gc(Viewport * viewport)
{
	if (this->gc) {
		g_object_unref(G_OBJECT(this->gc));
	}

	this->gc = viewport->new_gc_from_color(&(this->color), this->line_thickness);
}

static VikCoordLayer *coord_layer_create(VikViewport *vp)
{
	VikCoordLayer *vcl = coord_layer_new(vp);
	if (vp) {
		LayerCoord * layer = (LayerCoord *) ((VikLayer * ) vcl)->layer;
		layer->update_gc(&vp->port);
	}

	return vcl;
}
