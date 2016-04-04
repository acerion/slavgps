/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking
 * Copyright (C) 2010, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * 
 * viking is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 2 of the License, or
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
 
 /**
  * SECTION:vikwmscmapsource
  * @short_description: the class for WMS/WMS-C oriented map sources
  * 
  * The #VikWmscMapSource class handles WMS/WMS-C oriented map sources.
  * 
  * http://wiki.osgeo.org/wiki/WMS_Tile_Caching
  */
 
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include <stdlib.h>
#include <assert.h>

#include "globals.h"
#include "vikwmscmapsource.h"
#include "maputils.h"

static bool _coord_to_mapcoord ( VikMapSource *self, const VikCoord *src, double xzoom, double yzoom, MapCoord *dest );
static void _mapcoord_to_center_coord ( VikMapSource *self, MapCoord *src, VikCoord *dest );
static bool _supports_download_only_new ( VikMapSource *self );
static bool _is_direct_file_access ( VikMapSource *self );
static bool _is_mbtiles ( VikMapSource *self );
static bool _is_osm_meta_tiles (VikMapSource *self );
static uint8_t _get_zoom_min(VikMapSource *self );
static uint8_t _get_zoom_max(VikMapSource *self );
static double _get_lat_min(VikMapSource *self );
static double _get_lat_max(VikMapSource *self );
static double _get_lon_min(VikMapSource *self );
static double _get_lon_max(VikMapSource *self );

static char *_get_uri( VikMapSourceDefault *self, MapCoord *src );
static char *_get_hostname( VikMapSourceDefault *self );
static DownloadFileOptions *_get_download_options( VikMapSourceDefault *self );

typedef struct _VikWmscMapSourcePrivate VikWmscMapSourcePrivate;
struct _VikWmscMapSourcePrivate
{
  char *hostname;
  char *url;
  DownloadFileOptions options;
  unsigned int zoom_min; // TMS Zoom level: 0 = Whole World // http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
  unsigned int zoom_max; // TMS Zoom level: Often 18 for zoomed in.
  double lat_min; // Degrees
  double lat_max; // Degrees
  double lon_min; // Degrees
  double lon_max; // Degrees
};

#define VIK_WMSC_MAP_SOURCE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), VIK_TYPE_WMSC_MAP_SOURCE, VikWmscMapSourcePrivate))

/* properties */
enum
{
  PROP_0,

  PROP_HOSTNAME,
  PROP_URL,
  PROP_REFERER,
  PROP_FOLLOW_LOCATION,
  PROP_CHECK_FILE_SERVER_TIME,
  PROP_ZOOM_MIN,
  PROP_ZOOM_MAX,
  PROP_LAT_MIN,
  PROP_LAT_MAX,
  PROP_LON_MIN,
  PROP_LON_MAX,
};

G_DEFINE_TYPE (VikWmscMapSource, vik_wmsc_map_source, VIK_TYPE_MAP_SOURCE_DEFAULT);

static void
vik_wmsc_map_source_init (VikWmscMapSource *self)
{
  /* initialize the object here */
  VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE (self);

  priv->hostname = NULL;
  priv->url = NULL;
  priv->options.referer = NULL;
  priv->options.follow_location = 0;
  priv->options.check_file = a_check_map_file;
  priv->options.check_file_server_time = false;
  priv->zoom_min = 0;
  priv->zoom_max = 18;
  priv->lat_min = -90.0;
  priv->lat_max = 90.0;
  priv->lon_min = -180.0;
  priv->lon_max = 180.0;

  g_object_set (G_OBJECT (self),
                "tilesize-x", 256,
                "tilesize-y", 256,
                "drawmode", VIK_VIEWPORT_DRAWMODE_LATLON,
                NULL);
}

static void
vik_wmsc_map_source_finalize (GObject *object)
{
  VikWmscMapSource *self = VIK_WMSC_MAP_SOURCE (object);
  VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE (self);

  free(priv->hostname);
  priv->hostname = NULL;
  free(priv->url);
  priv->url = NULL;
  free(priv->options.referer);
  priv->options.referer = NULL;

  G_OBJECT_CLASS (vik_wmsc_map_source_parent_class)->finalize (object);
}

static void
vik_wmsc_map_source_set_property (GObject      *object,
                                    unsigned int         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  VikWmscMapSource *self = VIK_WMSC_MAP_SOURCE (object);
  VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE (self);

  switch (property_id)
    {
    case PROP_HOSTNAME:
      free(priv->hostname);
      priv->hostname = g_value_dup_string (value);
      break;

    case PROP_URL:
      free(priv->url);
      priv->url = g_value_dup_string (value);
      break;

    case PROP_REFERER:
      free(priv->options.referer);
      priv->options.referer = g_value_dup_string (value);
      break;

    case PROP_FOLLOW_LOCATION:
      priv->options.follow_location = g_value_get_long (value);
      break;

    case PROP_CHECK_FILE_SERVER_TIME:
      priv->options.check_file_server_time = g_value_get_boolean (value);
      break;

    case PROP_ZOOM_MIN:
      priv->zoom_min = g_value_get_uint (value);
      break;

    case PROP_ZOOM_MAX:
      priv->zoom_max = g_value_get_uint (value);
      break;

    case PROP_LAT_MIN:
      priv->lat_min = g_value_get_double (value);
      break;

    case PROP_LAT_MAX:
      priv->lat_max = g_value_get_double (value);
      break;

    case PROP_LON_MIN:
      priv->lon_min = g_value_get_double (value);
      break;

    case PROP_LON_MAX:
      priv->lon_max = g_value_get_double (value);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
vik_wmsc_map_source_get_property (GObject    *object,
                                    unsigned int       property_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  VikWmscMapSource *self = VIK_WMSC_MAP_SOURCE (object);
  VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE (self);

  switch (property_id)
    {
    case PROP_HOSTNAME:
      g_value_set_string (value, priv->hostname);
      break;

    case PROP_URL:
      g_value_set_string (value, priv->url);
      break;

    case PROP_REFERER:
      g_value_set_string (value, priv->options.referer);
      break;

    case PROP_FOLLOW_LOCATION:
      g_value_set_long (value, priv->options.follow_location);
      break;

    case PROP_CHECK_FILE_SERVER_TIME:
      g_value_set_boolean (value, priv->options.check_file_server_time);
      break;

    case PROP_ZOOM_MIN:
      g_value_set_uint (value, priv->zoom_min);
      break;

    case PROP_ZOOM_MAX:
      g_value_set_uint (value, priv->zoom_max);
      break;

    case PROP_LON_MIN:
      g_value_set_double (value, priv->lon_min);
      break;

    case PROP_LON_MAX:
      g_value_set_double (value, priv->lon_max);
      break;

    case PROP_LAT_MIN:
      g_value_set_double (value, priv->lat_min);
      break;

    case PROP_LAT_MAX:
      g_value_set_double (value, priv->lat_max);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
vik_wmsc_map_source_class_init (VikWmscMapSourceClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	VikMapSourceClass* grandparent_class = VIK_MAP_SOURCE_CLASS (klass);
	VikMapSourceDefaultClass* parent_class = VIK_MAP_SOURCE_DEFAULT_CLASS (klass);
	GParamSpec *pspec = NULL;
		
	object_class->set_property = vik_wmsc_map_source_set_property;
    object_class->get_property = vik_wmsc_map_source_get_property;

	/* Overiding methods */
	grandparent_class->coord_to_mapcoord =        _coord_to_mapcoord;
	grandparent_class->mapcoord_to_center_coord = _mapcoord_to_center_coord;
	grandparent_class->supports_download_only_new = _supports_download_only_new;
	grandparent_class->is_direct_file_access = _is_direct_file_access;
	grandparent_class->is_mbtiles = _is_mbtiles;
	grandparent_class->is_osm_meta_tiles = _is_osm_meta_tiles;
	grandparent_class->get_zoom_min = _get_zoom_min;
	grandparent_class->get_zoom_max = _get_zoom_max;
	grandparent_class->get_lat_min = _get_lat_min;
	grandparent_class->get_lat_max = _get_lat_max;
	grandparent_class->get_lon_min = _get_lon_min;
	grandparent_class->get_lon_max = _get_lon_max;

	parent_class->get_uri = _get_uri;
	parent_class->get_hostname = _get_hostname;
	parent_class->get_download_options = _get_download_options;

	pspec = g_param_spec_string ("hostname",
	                             "Hostname",
	                             "The hostname of the map server",
	                             "<no-set>" /* default value */,
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_HOSTNAME, pspec);

	pspec = g_param_spec_string ("url",
	                             "URL",
	                             "The template of the tiles' URL",
	                             "<no-set>" /* default value */,
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_URL, pspec);

	pspec = g_param_spec_string ("referer",
	                             "Referer",
	                             "The REFERER string to use in HTTP request",
	                             NULL /* default value */,
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_REFERER, pspec);
	
	pspec = g_param_spec_long ("follow-location",
	                           "Follow location",
                               "Specifies the number of retries to follow a redirect while downloading a page",
                               0  /* minimum value */,
                               G_MAXLONG /* maximum value */,
                               0  /* default value */,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FOLLOW_LOCATION, pspec);
	
	pspec = g_param_spec_boolean ("check-file-server-time",
	                              "Check file server time",
                                  "Age of current cache before redownloading tile",
                                  false  /* default value */,
                                  G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_CHECK_FILE_SERVER_TIME, pspec);

	pspec = g_param_spec_uint ("zoom-min",
	                           "Minimum zoom",
	                           "Minimum Zoom level supported by the map provider",
	                           0,  // minimum value,
	                           22, // maximum value
	                           0, // default value
	                           G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_ZOOM_MIN, pspec);

	pspec = g_param_spec_uint ("zoom-max",
	                           "Maximum zoom",
	                           "Maximum Zoom level supported by the map provider",
	                           0,  // minimum value,
	                           22, // maximum value
	                           18, // default value
	                           G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_ZOOM_MAX, pspec);

	pspec = g_param_spec_double ("lat-min",
	                             "Minimum latitude",
	                             "Minimum latitude in degrees supported by the map provider",
	                             -90.0,  // minimum value
	                             90.0, // maximum value
	                             -90.0, // default value
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_LAT_MIN, pspec);

	pspec = g_param_spec_double ("lat-max",
	                             "Maximum latitude",
	                             "Maximum latitude in degrees supported by the map provider",
	                             -90.0,  // minimum value
	                             90.0, // maximum value
	                             90.0, // default value
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_LAT_MAX, pspec);

	pspec = g_param_spec_double ("lon-min",
	                             "Minimum longitude",
	                             "Minimum longitude in degrees supported by the map provider",
	                             -180.0,  // minimum value
	                             180.0, // maximum value
	                             -180.0, // default value
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_LON_MIN, pspec);

	pspec = g_param_spec_double ("lon-max",
	                             "Maximum longitude",
	                             "Maximum longitude in degrees supported by the map provider",
	                             -180.0,  // minimum value
	                             180.0, // maximum value
	                             180.0, // default value
	                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_LON_MAX, pspec);

	g_type_class_add_private (klass, sizeof (VikWmscMapSourcePrivate));
	
	object_class->finalize = vik_wmsc_map_source_finalize;
}

static bool
_supports_download_only_new ( VikMapSource *self )
{
	g_return_val_if_fail (VIK_IS_WMSC_MAP_SOURCE(self), false);
	
    VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);
	
	return priv->options.check_file_server_time;
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

static bool
_is_osm_meta_tiles ( VikMapSource *self )
{
	return false;
}

static bool
_coord_to_mapcoord ( VikMapSource *self, const VikCoord *src, double xzoom, double yzoom, MapCoord *dest )
{
  assert ( src->mode == VIK_COORD_LATLON );

  if ( xzoom != yzoom )
    return false;

  dest->scale = map_utils_mpp_to_scale ( xzoom );
  if ( dest->scale == 255 )
    return false;

  /* Note : VIK_GZ(17) / xzoom / 2 = number of tile on Y axis */
  fprintf(stderr, "DEBUG: %s: xzoom=%f yzoom=%f -> %f\n", __FUNCTION__,
          xzoom, yzoom, VIK_GZ(17) / xzoom / 2);
  dest->x = floor((src->east_west + 180) / 180 * VIK_GZ(17) / xzoom / 2);
  /* We should restore logic of viking:
   * tile index on Y axis follow a screen logic (top -> down)
   */
  dest->y = floor((180 - (src->north_south + 90)) / 180 * VIK_GZ(17) / xzoom / 2);
  dest->z = 0;
  fprintf(stderr, "DEBUG: %s: %f,%f -> %d,%d\n", __FUNCTION__,
          src->east_west, src->north_south, dest->x, dest->y);
  return true;
}

static void
_mapcoord_to_center_coord ( VikMapSource *self, MapCoord *src, VikCoord *dest )
{
  double socalled_mpp;
  if (src->scale >= 0)
    socalled_mpp = VIK_GZ(src->scale);
  else
    socalled_mpp = 1.0/VIK_GZ(-src->scale);
  dest->mode = VIK_COORD_LATLON;
  dest->east_west = (src->x+0.5) * 180 / VIK_GZ(17) * socalled_mpp * 2 - 180;
  /* We should restore logic of viking:
   * tile index on Y axis follow a screen logic (top -> down)
   */
  dest->north_south = -((src->y+0.5) * 180 / VIK_GZ(17) * socalled_mpp * 2 - 90);
  fprintf(stderr, "DEBUG: %s: %d,%d -> %f,%f\n", __FUNCTION__,
          src->x, src->y, dest->east_west, dest->north_south);
}

static char *
_get_uri( VikMapSourceDefault *self, MapCoord *src )
{
	g_return_val_if_fail (VIK_IS_WMSC_MAP_SOURCE(self), NULL);
	
    VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);
	double socalled_mpp;
	if (src->scale >= 0)
		socalled_mpp = VIK_GZ(src->scale);
	else
		socalled_mpp = 1.0/VIK_GZ(-src->scale);
	double minx = (double)src->x * 180 / VIK_GZ(17) * socalled_mpp * 2 - 180;
	double maxx = (double)(src->x + 1) * 180 / VIK_GZ(17) * socalled_mpp * 2 - 180;
	/* We should restore logic of viking:
     * tile index on Y axis follow a screen logic (top -> down)
     */
	double miny = -((double)(src->y + 1) * 180 / VIK_GZ(17) * socalled_mpp * 2 - 90);
	double maxy = -((double)(src->y) * 180 / VIK_GZ(17) * socalled_mpp * 2 - 90);
	
	char sminx[G_ASCII_DTOSTR_BUF_SIZE];
	char smaxx[G_ASCII_DTOSTR_BUF_SIZE];
	char sminy[G_ASCII_DTOSTR_BUF_SIZE];
	char smaxy[G_ASCII_DTOSTR_BUF_SIZE];

	g_ascii_dtostr (sminx, G_ASCII_DTOSTR_BUF_SIZE, minx);
	g_ascii_dtostr (smaxx, G_ASCII_DTOSTR_BUF_SIZE, maxx);
	g_ascii_dtostr (sminy, G_ASCII_DTOSTR_BUF_SIZE, miny);
	g_ascii_dtostr (smaxy, G_ASCII_DTOSTR_BUF_SIZE, maxy);

	char *uri = g_strdup_printf (priv->url, sminx, sminy, smaxx, smaxy);
	
	return uri;
} 

static char *
_get_hostname( VikMapSourceDefault *self )
{
	g_return_val_if_fail (VIK_IS_WMSC_MAP_SOURCE(self), NULL);
	
    VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);
	return g_strdup( priv->hostname );
}

static DownloadFileOptions *
_get_download_options( VikMapSourceDefault *self )
{
	g_return_val_if_fail (VIK_IS_WMSC_MAP_SOURCE(self), NULL);
	
	VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);
	return &(priv->options);
}
/**
 *
 */
static uint8_t
_get_zoom_min (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_WMSC_MAP_SOURCE(self), false);
	VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);
	return priv->zoom_min;
}

/**
 *
 */
static uint8_t
_get_zoom_max (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_WMSC_MAP_SOURCE(self), false);
	VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);
	return priv->zoom_max;
}

/**
 *
 */
static double
_get_lat_min (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_WMSC_MAP_SOURCE(self), false);
	VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);
	return priv->lat_min;
}

/**
 *
 */
static double
_get_lat_max (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_WMSC_MAP_SOURCE(self), false);
	VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);
	return priv->lat_max;
}

/**
 *
 */
static double
_get_lon_min (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_WMSC_MAP_SOURCE(self), false);
	VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);
	return priv->lon_min;
}

/**
 *
 */
static double
_get_lon_max (VikMapSource *self)
{
	g_return_val_if_fail (VIK_IS_WMSC_MAP_SOURCE(self), false);
	VikWmscMapSourcePrivate *priv = VIK_WMSC_MAP_SOURCE_PRIVATE(self);
	return priv->lon_max;
}

VikWmscMapSource *
vik_wmsc_map_source_new_with_id (uint16_t id, const char *label, const char *hostname, const char *url)
{
	return g_object_new(VIK_TYPE_WMSC_MAP_SOURCE,
	                    "id", id, "label", label, "hostname", hostname, "url", url, NULL);
}
