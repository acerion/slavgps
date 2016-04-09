/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2008, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#include "vikwebtoolcenter.h"

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "util.h"
#include "globals.h"
#include "maputils.h"

static GObjectClass *parent_class;

static void webtool_center_finalize ( GObject *gob );

static uint8_t webtool_center_mpp_to_zoom ( VikWebtool *self, double mpp );
static char *webtool_center_get_url ( VikWebtool *vw, VikWindow *vwindow );
static char *webtool_center_get_url_at_position ( VikWebtool *vw, VikWindow *vwindow, VikCoord *vc );

typedef struct _VikWebtoolCenterPrivate VikWebtoolCenterPrivate;

struct _VikWebtoolCenterPrivate
{
  char *url;
};

#define WEBTOOL_CENTER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                       VIK_WEBTOOL_CENTER_TYPE,          \
                                       VikWebtoolCenterPrivate))

G_DEFINE_TYPE (VikWebtoolCenter, vik_webtool_center, VIK_WEBTOOL_TYPE)

enum
{
  PROP_0,

  PROP_URL,
};

static void
webtool_center_set_property (GObject      *object,
                             unsigned int         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  VikWebtoolCenter *self = VIK_WEBTOOL_CENTER (object);
  VikWebtoolCenterPrivate *priv = WEBTOOL_CENTER_GET_PRIVATE (self);

  switch (property_id)
    {
    case PROP_URL:
      free(priv->url);
      priv->url = g_value_dup_string (value);
      fprintf(stderr, "DEBUG: VikWebtoolCenter.url: %s\n", priv->url);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
webtool_center_get_property (GObject    *object,
                             unsigned int       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  VikWebtoolCenter *self = VIK_WEBTOOL_CENTER (object);
  VikWebtoolCenterPrivate *priv = WEBTOOL_CENTER_GET_PRIVATE (self);

  switch (property_id)
    {
    case PROP_URL:
      g_value_set_string (value, priv->url);
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
vik_webtool_center_class_init ( VikWebtoolCenterClass *klass )
{
  GObjectClass *gobject_class;
  VikWebtoolClass *base_class;
  GParamSpec *pspec;

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = webtool_center_finalize;
  gobject_class->set_property = webtool_center_set_property;
  gobject_class->get_property = webtool_center_get_property;

  pspec = g_param_spec_string ("url",
                               "Template Url",
                               "Set the template url",
                               VIKING_URL /* default value */,
                               (GParamFlags) (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_URL,
                                   pspec);

  parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

  base_class = VIK_WEBTOOL_CLASS ( klass );
  base_class->get_url = webtool_center_get_url;
  base_class->get_url_at_position = webtool_center_get_url_at_position;

  klass->mpp_to_zoom = webtool_center_mpp_to_zoom;

  g_type_class_add_private (klass, sizeof (VikWebtoolCenterPrivate));
}

VikWebtoolCenter *vik_webtool_center_new ()
{
  return VIK_WEBTOOL_CENTER ( g_object_new ( VIK_WEBTOOL_CENTER_TYPE, NULL ) );
}

VikWebtoolCenter *vik_webtool_center_new_with_members ( const char *label, const char *url )
{
  VikWebtoolCenter *result = VIK_WEBTOOL_CENTER ( g_object_new ( VIK_WEBTOOL_CENTER_TYPE,
                                                                 "label", label,
                                                                 "url", url,
                                                                 NULL ) );

  return result;
}

static void
vik_webtool_center_init ( VikWebtoolCenter *self )
{
  VikWebtoolCenterPrivate *priv = WEBTOOL_CENTER_GET_PRIVATE (self);
  priv->url = NULL;
}

static void webtool_center_finalize ( GObject *gob )
{
  VikWebtoolCenterPrivate *priv = WEBTOOL_CENTER_GET_PRIVATE ( gob );
  free( priv->url ); priv->url = NULL;
  G_OBJECT_CLASS(parent_class)->finalize(gob);
}

static uint8_t webtool_center_mpp_to_zoom ( VikWebtool *self, double mpp ) {
  return map_utils_mpp_to_zoom_level ( mpp );
}

static char *webtool_center_get_url_at_position ( VikWebtool *self, VikWindow *vwindow, VikCoord *vc )
{
  VikWebtoolCenterPrivate *priv = NULL;
  VikViewport *viewport = NULL;
  uint8_t zoom = 17;
  struct LatLon ll;
  char strlat[G_ASCII_DTOSTR_BUF_SIZE], strlon[G_ASCII_DTOSTR_BUF_SIZE];

  priv = WEBTOOL_CENTER_GET_PRIVATE (self);
  viewport = vik_window_viewport ( vwindow );
  // Coords
  // Use the provided position otherwise use center of the viewport
  if ( vc )
    vik_coord_to_latlon ( vc, &ll );
  else {
    const VikCoord *coord = NULL;
    coord = vik_viewport_get_center ( viewport );
    vik_coord_to_latlon ( coord, &ll );
  }

  // zoom - ideally x & y factors need to be the same otherwise use the default
  if ( vik_viewport_get_xmpp ( viewport ) == vik_viewport_get_ympp ( viewport ) )
    zoom = vik_webtool_center_mpp_to_zoom ( self, vik_viewport_get_zoom ( viewport ) );

  // Cannot simply use g_strdup_printf and double due to locale.
  // As we compute an URL, we have to think in C locale.
  g_ascii_dtostr (strlat, G_ASCII_DTOSTR_BUF_SIZE, ll.lat);
  g_ascii_dtostr (strlon, G_ASCII_DTOSTR_BUF_SIZE, ll.lon);

  return g_strdup_printf ( priv->url, strlat, strlon, zoom );
}

static char *webtool_center_get_url ( VikWebtool *self, VikWindow *vwindow )
{
  return webtool_center_get_url_at_position ( self, vwindow, NULL );
}

uint8_t vik_webtool_center_mpp_to_zoom (VikWebtool *self, double mpp)
{
  return VIK_WEBTOOL_CENTER_GET_CLASS( self )->mpp_to_zoom( self, mpp );
}
