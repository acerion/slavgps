/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2012-2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

/**
 * SECTION:vikroutingengine
 * @short_description: the base class to describe routing engine
 *
 * The #VikRoutingEngine class is both the interface and the base class
 * for the hierarchie of routing engines.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdlib>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "babel.h"
#include "vikroutingengine.h"




using namespace SlavGPS;




static void vik_routing_engine_finalize(GObject * gob);
static GObjectClass * parent_class;

typedef struct _VikRoutingPrivate VikRoutingPrivate;
struct _VikRoutingPrivate {
	char * id;
	char * label;
	char * format;
};




#define VIK_ROUTING_ENGINE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), VIK_ROUTING_ENGINE_TYPE, VikRoutingPrivate))
G_DEFINE_ABSTRACT_TYPE (VikRoutingEngine, vik_routing_engine, G_TYPE_OBJECT)




/* Properties. */
enum {
	PROP_0,

	PROP_ID,
	PROP_LABEL,
	PROP_FORMAT,
};




static void vik_routing_engine_set_property(GObject      * object,
					    unsigned int   property_id,
					    const GValue * value,
					    GParamSpec   * pspec)
{
	VikRoutingPrivate * priv = VIK_ROUTING_ENGINE_PRIVATE (object);

	switch (property_id) {
	case PROP_ID:
		free(priv->id);
		priv->id = g_strdup(g_value_get_string(value));
		break;

	case PROP_LABEL:
		free(priv->label);
		priv->label = g_strdup(g_value_get_string(value));
		break;

	case PROP_FORMAT:
		free(priv->format);
		priv->format = g_strdup(g_value_get_string(value));
		break;

	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}




static void vik_routing_engine_get_property(GObject      * object,
					    unsigned int   property_id,
					    GValue       * value,
					    GParamSpec   * pspec)
{
	VikRoutingPrivate * priv = VIK_ROUTING_ENGINE_PRIVATE (object);

	switch (property_id) {
	case PROP_ID:
		g_value_set_string(value, priv->id);
		break;

	case PROP_LABEL:
		g_value_set_string(value, priv->label);
		break;

	case PROP_FORMAT:
		g_value_set_string(value, priv->format);
		break;

	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}




static void vik_routing_engine_class_init(VikRoutingEngineClass * klass)
{
	GObjectClass *object_class;
	VikRoutingEngineClass *routing_class;
	GParamSpec *pspec = NULL;

	object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = vik_routing_engine_set_property;
	object_class->get_property = vik_routing_engine_get_property;
	object_class->finalize = vik_routing_engine_finalize;

	parent_class = (GObjectClass *) g_type_class_peek_parent(klass);

	routing_class = VIK_ROUTING_ENGINE_CLASS (klass);
	routing_class->find = NULL;

	routing_class->supports_direction = NULL;
	routing_class->get_url_from_directions = NULL;

	routing_class->refine = NULL;
	routing_class->supports_refine = NULL;

	pspec = g_param_spec_string("id",
				    "Identifier",
				    "The identifier of the routing engine",
				    "<no-set>", /* Default value. */
				    (GParamFlags) (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_ID, pspec);

	pspec = g_param_spec_string("label",
				    "Label",
				    "The label of the routing engine",
				    "<no-set>", /* Default value. */
				    (GParamFlags) (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_LABEL, pspec);

	pspec = g_param_spec_string("format",
				    "Format",
				    "The format of the output (see gpsbabel)",
				    "<no-set>", /* Default value. */
				    (GParamFlags) (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_FORMAT, pspec);

	g_type_class_add_private(klass, sizeof (VikRoutingPrivate));
}




static void vik_routing_engine_init(VikRoutingEngine * self)
{
	VikRoutingPrivate * priv = VIK_ROUTING_ENGINE_PRIVATE (self);

	priv->id = NULL;
	priv->label = NULL;
	priv->format = NULL;
}




static void vik_routing_engine_finalize(GObject * self)
{
	VikRoutingPrivate *priv = VIK_ROUTING_ENGINE_PRIVATE (self);

	free(priv->id);
	priv->id = NULL;

	free(priv->label);
	priv->label = NULL;

	free(priv->format);
	priv->format = NULL;

	G_OBJECT_CLASS(parent_class)->finalize(self);
}




/**
 * @self: self object
 * @vtl:
 * @start: starting point
 * @end: ending point
 *
 * Retrieve a route between two coordinates.
 *
 * Returns: indicates success or not.
 */
bool vik_routing_engine_find(VikRoutingEngine * self, VikLayer * vtl, struct LatLon start, struct LatLon end)
{
	VikRoutingEngineClass * klass;

	if (!VIK_IS_ROUTING_ENGINE (self)) {
		return 0;
	}
	klass = VIK_ROUTING_ENGINE_GET_CLASS(self);
	if (!klass->find) {
		return 0;
	}

	return klass->find(self, vtl, start, end);
}




/**
 * Returns: the id of self.
 */
char * vik_routing_engine_get_id(VikRoutingEngine * self)
{
	VikRoutingPrivate * priv = VIK_ROUTING_ENGINE_PRIVATE (self);

	return priv->id;
}




/**
 * Returns: the label of self.
 */
char * vik_routing_engine_get_label(VikRoutingEngine * self)
{
	VikRoutingPrivate * priv = VIK_ROUTING_ENGINE_PRIVATE (self);

	return priv->label;
}




/**
 * GPSbabel's Format of result.
 *
 * Returns: the format of self.
 */
char * vik_routing_engine_get_format(VikRoutingEngine * self)
{
	VikRoutingPrivate * priv = VIK_ROUTING_ENGINE_PRIVATE (self);

	return priv->format;
}

/**
 * Returns: %true if this engine supports the route finding based on directions.
 */
bool vik_routing_engine_supports_direction(VikRoutingEngine * self)
{
	VikRoutingEngineClass * klass;

	if (!VIK_IS_ROUTING_ENGINE (self)) {
		return false;
	}

	klass = VIK_ROUTING_ENGINE_GET_CLASS(self);
	if (!klass->supports_direction) {
		return false;
	}

	return klass->supports_direction(self);
}




/**
 * @self: routing engine
 * @start: the start direction
 * @end: the end direction
 *
 * Compute the URL used with the acquire framework.
 *
 * Returns: the computed URL.
 */
char * vik_routing_engine_get_url_from_directions(VikRoutingEngine * self, const char * start, const char * end)
{
	VikRoutingEngineClass * klass;

	if (!VIK_IS_ROUTING_ENGINE (self)) {
		return NULL;
	}
	klass = VIK_ROUTING_ENGINE_GET_CLASS(self);
	if (!klass->get_url_from_directions) {
		return NULL;
	}

	return klass->get_url_from_directions(self, start, end);
}




/**
 * @self: self object
 * @vtl: layer where to create new track
 * @vt: the simple route to refine
 *
 * Retrieve a route refining the @vt track/route.
 *
 * A refined route is computed from @vt.
 * The route is computed from first trackpoint to last trackpoint,
 * and going via all intermediate trackpoints.
 *
 * Returns: indicates success or not.
 */
bool vik_routing_engine_refine(VikRoutingEngine * self, VikLayer * vtl, Track * trk)
{
	VikRoutingEngineClass * klass;

	if (!VIK_IS_ROUTING_ENGINE (self)) {
		return 0;
	}

	klass = VIK_ROUTING_ENGINE_GET_CLASS (self);
	if (!klass->refine) {
		return 0;
	}

	return klass->refine(self, vtl, trk);
}




/**
 * @self: routing engine
 *
 * Returns: %true if this engine supports the refine of track.
 */
bool vik_routing_engine_supports_refine(VikRoutingEngine * self)
{
	VikRoutingEngineClass * klass;

	if (!VIK_IS_ROUTING_ENGINE (self)) {
		return false;
	}
	klass = VIK_ROUTING_ENGINE_GET_CLASS (self);
	if (!klass->supports_refine) {
		return false;
	}

	return klass->supports_refine(self);
}
