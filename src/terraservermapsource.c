/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking
 * Copyright (C) 2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * 
 * viking is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * viking is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include "globals.h"
#include "terraservermapsource.h"

static bool _coord_to_mapcoord ( VikMapSource *self, const VikCoord *src, double xzoom, double yzoom, MapCoord *dest );
static void _mapcoord_to_center_coord ( VikMapSource *self, MapCoord *src, VikCoord *dest );
static bool _is_direct_file_access ( VikMapSource *self );
static bool _is_mbtiles ( VikMapSource *self );

static char *_get_uri( VikMapSourceDefault *self, MapCoord *src );
static char *_get_hostname( VikMapSourceDefault *self );
static DownloadFileOptions *_get_download_options( VikMapSourceDefault *self );

/* FIXME Huge gruik */
static DownloadFileOptions terraserver_options = { false, false, NULL, 0, a_check_map_file, NULL, NULL };

typedef struct _TerraserverMapSourcePrivate TerraserverMapSourcePrivate;
struct _TerraserverMapSourcePrivate
{
  uint8_t type;
};

#define TERRASERVER_MAP_SOURCE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), TERRASERVER_TYPE_MAP_SOURCE, TerraserverMapSourcePrivate))

/* properties */
enum
{
  PROP_0,

  PROP_TYPE,
};

G_DEFINE_TYPE (TerraserverMapSource, terraserver_map_source, VIK_TYPE_MAP_SOURCE_DEFAULT);

static void
terraserver_map_source_init (TerraserverMapSource *self)
{
	/* initialize the object here */
	g_object_set (G_OBJECT (self),
	              "tilesize-x", 200,
	              "tilesize-y", 200,
	              "drawmode", VIK_VIEWPORT_DRAWMODE_UTM,
	              NULL);
}

static void
terraserver_map_source_finalize (GObject *object)
{
	/* TODO: Add deinitalization code here */

	G_OBJECT_CLASS (terraserver_map_source_parent_class)->finalize (object);
}

static void
terraserver_map_source_set_property (GObject      *object,
                                     unsigned int         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  TerraserverMapSource *self = TERRASERVER_MAP_SOURCE (object);
  TerraserverMapSourcePrivate *priv = TERRASERVER_MAP_SOURCE_PRIVATE (self);

  switch (property_id)
    {
    case PROP_TYPE:
      priv->type = g_value_get_uint (value);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
terraserver_map_source_get_property (GObject    *object,
                                     unsigned int       property_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  TerraserverMapSource *self = TERRASERVER_MAP_SOURCE (object);
  TerraserverMapSourcePrivate *priv = TERRASERVER_MAP_SOURCE_PRIVATE (self);

  switch (property_id)
    {
    case PROP_TYPE:
      g_value_set_uint (value, priv->type);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
terraserver_map_source_class_init (TerraserverMapSourceClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	VikMapSourceClass* grandparent_class = VIK_MAP_SOURCE_CLASS (klass);
	VikMapSourceDefaultClass* parent_class = VIK_MAP_SOURCE_DEFAULT_CLASS (klass);
    GParamSpec *pspec = NULL;
	
	object_class->set_property = terraserver_map_source_set_property;
    object_class->get_property = terraserver_map_source_get_property;
	
	/* Overiding methods */
	grandparent_class->coord_to_mapcoord =        _coord_to_mapcoord;
	grandparent_class->mapcoord_to_center_coord = _mapcoord_to_center_coord;
	grandparent_class->is_direct_file_access = _is_direct_file_access;
	grandparent_class->is_mbtiles = _is_mbtiles;
	
	parent_class->get_uri = _get_uri;
	parent_class->get_hostname = _get_hostname;
	parent_class->get_download_options = _get_download_options;

	pspec = g_param_spec_uint ("type",
	                           "Type",
                               "Type of Terraserver map",
                               0  /* minimum value */,
                               G_MAXUINT8 /* maximum value */,
                               0  /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_TYPE, pspec);

	g_type_class_add_private (klass, sizeof (TerraserverMapSourcePrivate));
	
	object_class->finalize = terraserver_map_source_finalize;
}

#define TERRASERVER_SITE "msrmaps.com"
#define MARGIN_OF_ERROR 0.001

static int mpp_to_scale ( double mpp, uint8_t type )
{
  mpp *= 4;
  int t = (int) mpp;
  if ( ABS(mpp - t) > MARGIN_OF_ERROR )
    return false;

  switch ( t ) {
    case 1: return (type == 4) ? 8 : 0;
    case 2: return (type == 4) ? 9 : 0;
    case 4: return (type != 2) ? 10 : 0;
    case 8: return 11;
    case 16: return 12;
    case 32: return 13;
    case 64: return 14;
    case 128: return 15;
    case 256: return 16;
    case 512: return 17;
    case 1024: return 18;
    case 2048: return 19;
    default: return 0;
  }
}

static double scale_to_mpp ( int scale )
{
  return pow(2,scale - 10);
}

static bool
_coord_to_mapcoord ( VikMapSource *self, const VikCoord *src, double xmpp, double ympp, MapCoord *dest )
{
	g_return_val_if_fail(TERRASERVER_IS_MAP_SOURCE(self), false);
	
	TerraserverMapSourcePrivate *priv = TERRASERVER_MAP_SOURCE_PRIVATE(self);
	int type = priv->type;
	if ( src->mode != VIK_COORD_UTM )
		return false;

	if ( xmpp != ympp )
		return false;

	dest->scale = mpp_to_scale ( xmpp, type );
	if ( ! dest->scale )
		return false;

	dest->x = (int)(((int)(src->east_west))/(200*xmpp));
	dest->y = (int)(((int)(src->north_south))/(200*xmpp));
	dest->z = src->utm_zone;
	return true;
}

static bool
_is_direct_file_access ( VikMapSource *self )
{
	return false;
}

static bool
_is_mbtiles ( VikMapSource *self )
{
	return false;
}

static void
_mapcoord_to_center_coord ( VikMapSource *self, MapCoord *src, VikCoord *dest )
{
	// FIXME: slowdown here!
	double mpp = scale_to_mpp ( src->scale );
	dest->mode = VIK_COORD_UTM;
	dest->utm_zone = src->z;
	dest->east_west = ((src->x * 200) + 100) * mpp;
	dest->north_south = ((src->y * 200) + 100) * mpp;
}

static char *
_get_uri( VikMapSourceDefault *self, MapCoord *src )
{
	g_return_val_if_fail (TERRASERVER_IS_MAP_SOURCE(self), NULL);
	
	TerraserverMapSourcePrivate *priv = TERRASERVER_MAP_SOURCE_PRIVATE(self);
	int type = priv->type;
	char *uri = g_strdup_printf ( "/tile.ashx?T=%d&S=%d&X=%d&Y=%d&Z=%d", type,
                                  src->scale, src->x, src->y, src->z );
	return uri;
} 

static char *
_get_hostname( VikMapSourceDefault *self )
{
	g_return_val_if_fail (TERRASERVER_IS_MAP_SOURCE(self), NULL);
	
	return g_strdup( TERRASERVER_SITE );
}

static DownloadFileOptions *
_get_download_options( VikMapSourceDefault *self )
{
	g_return_val_if_fail (TERRASERVER_IS_MAP_SOURCE(self), NULL);
	
	return &terraserver_options;
}

TerraserverMapSource *
terraserver_map_source_new_with_id (uint16_t id, const char *label, int type)
{
	char *copyright = NULL;
	switch (type)
	{
	case 1:
		copyright = "© DigitalGlobe";
		break;
	case 2:
		copyright = "© LandVoyage";
		break;
	case 4:
		copyright = "© DigitalGlobe";
		break;
	default:
		g_critical("Houston, we've had a problem. type=%d", type);
	}

	return g_object_new(TERRASERVER_TYPE_MAP_SOURCE,
	                    "id", id, "label", label, "type", type,
	                    "copyright", copyright,
	                    NULL);
}
