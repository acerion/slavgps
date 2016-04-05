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

#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include "coords.h"
#include "vikcoord.h"
#include "vikwaypoint.h"
#include "globals.h"
#include "garminsymbols.h"
#include "dems.h"
#include <glib/gi18n.h>

VikWaypoint *vik_waypoint_new()
{
  VikWaypoint *wp = (VikWaypoint *) malloc(sizeof (VikWaypoint));
  memset(wp, 0, sizeof (VikWaypoint));

  wp->altitude = VIK_DEFAULT_ALTITUDE;
  wp->name = g_strdup(_("Waypoint"));
  return wp;
}

// Hmmm tempted to put in new constructor
void vik_waypoint_set_name(VikWaypoint *wp, const char *name)
{
  if ( wp->name )
    free( wp->name );

  wp->name = g_strdup(name);
}

void vik_waypoint_set_comment_no_copy(VikWaypoint *wp, char *comment)
{
  if ( wp->comment )
    free( wp->comment );
  wp->comment = comment;
}

void vik_waypoint_set_comment(VikWaypoint *wp, const char *comment)
{
  if ( wp->comment )
    free( wp->comment );

  if ( comment && comment[0] != '\0' )
    wp->comment = g_strdup(comment);
  else
    wp->comment = NULL;
}

void vik_waypoint_set_description(VikWaypoint *wp, const char *description)
{
  if ( wp->description )
    free( wp->description );

  if ( description && description[0] != '\0' )
    wp->description = g_strdup(description);
  else
    wp->description = NULL;
}

void vik_waypoint_set_source(VikWaypoint *wp, const char *source)
{
  if ( wp->source )
    free( wp->source );

  if ( source && source[0] != '\0' )
    wp->source = g_strdup(source);
  else
    wp->source = NULL;
}

void vik_waypoint_set_type(VikWaypoint *wp, const char *type)
{
  if ( wp->type )
    free( wp->type );

  if ( type && type[0] != '\0' )
    wp->type = g_strdup(type);
  else
    wp->type = NULL;
}

void vik_waypoint_set_url(VikWaypoint *wp, const char *url)
{
  if ( wp->url )
    free( wp->url );

  if ( url && url[0] != '\0' )
    wp->url = g_strdup(url);
  else
    wp->url = NULL;
}

void vik_waypoint_set_image(VikWaypoint *wp, const char *image)
{
  if ( wp->image )
    free( wp->image );

  if ( image && image[0] != '\0' )
    wp->image = g_strdup(image);
  else
    wp->image = NULL;
  // NOTE - ATM the image (thumbnail) size is calculated on demand when needed to be first drawn
}

void vik_waypoint_set_symbol(VikWaypoint *wp, const char *symname)
{
  const char *hashed_symname;

  if ( wp->symbol )
    free( wp->symbol );

  // NB symbol_pixbuf is just a reference, so no need to free it

  if ( symname && symname[0] != '\0' ) {
    hashed_symname = a_get_hashed_sym ( symname );
    if ( hashed_symname )
      symname = hashed_symname;
    wp->symbol = g_strdup( symname );
    wp->symbol_pixbuf = a_get_wp_sym ( wp->symbol );
  }
  else {
    wp->symbol = NULL;
    wp->symbol_pixbuf = NULL;
  }
}

void vik_waypoint_free(VikWaypoint *wp)
{
  if ( wp->comment )
    free( wp->comment );
  if ( wp->description )
    free( wp->description );
  if ( wp->source )
    free( wp->source );
  if ( wp->type )
    free( wp->type );
  if ( wp->url )
    free( wp->url );
  if ( wp->image )
    free( wp->image );
  if ( wp->symbol )
    free( wp->symbol );
  free( wp );
}

VikWaypoint *vik_waypoint_copy(const VikWaypoint *wp)
{
  VikWaypoint *new_wp = vik_waypoint_new();
  new_wp->coord = wp->coord;
  new_wp->visible = wp->visible;
  new_wp->altitude = wp->altitude;
  new_wp->has_timestamp = wp->has_timestamp;
  new_wp->timestamp = wp->timestamp;
  vik_waypoint_set_name(new_wp,wp->name);
  vik_waypoint_set_comment(new_wp,wp->comment);
  vik_waypoint_set_description(new_wp,wp->description);
  vik_waypoint_set_source(new_wp,wp->source);
  vik_waypoint_set_type(new_wp,wp->type);
  vik_waypoint_set_url(new_wp,wp->url);
  vik_waypoint_set_image(new_wp,wp->image);
  vik_waypoint_set_symbol(new_wp,wp->symbol);
  return new_wp;
}

/**
 * vik_waypoint_apply_dem_data:
 * @wp:            The Waypoint to operate on
 * @skip_existing: When true, don't change the elevation if the waypoint already has a value
 *
 * Set elevation data for a waypoint using available DEM information
 *
 * Returns: true if the waypoint was updated
 */
bool vik_waypoint_apply_dem_data ( VikWaypoint *wp, bool skip_existing )
{
  bool updated = false;
  if ( !(skip_existing && wp->altitude != VIK_DEFAULT_ALTITUDE) ) {
    int16_t elev = a_dems_get_elev_by_coord ( &(wp->coord), VIK_DEM_INTERPOL_BEST );
    if ( elev != VIK_DEM_INVALID_ELEVATION ) {
      wp->altitude = (double)elev;
      updated = true;
    }
  }
  return updated;
}

/*
 * Take a Waypoint and convert it into a byte array
 */
void vik_waypoint_marshall ( VikWaypoint *wp, uint8_t **data, unsigned int *datalen)
{
  GByteArray *b = g_byte_array_new();
  unsigned int len;

  // This creates space for fixed sized members like ints and whatnot
  //  and copies that amount of data from the waypoint to byte array
  g_byte_array_append(b, (uint8_t *)wp, sizeof(*wp));

  // This allocates space for variant sized strings
  //  and copies that amount of data from the waypoint to byte array
#define vwm_append(s) \
  len = (s) ? strlen(s)+1 : 0; \
  g_byte_array_append(b, (uint8_t *)&len, sizeof(len)); \
  if (s) g_byte_array_append(b, (uint8_t *)s, len);

  vwm_append(wp->name);
  vwm_append(wp->comment);
  vwm_append(wp->description);
  vwm_append(wp->source);
  vwm_append(wp->type);
  vwm_append(wp->url);
  vwm_append(wp->image);
  vwm_append(wp->symbol);

  *data = b->data;
  *datalen = b->len;
  g_byte_array_free(b, false);
#undef vwm_append
}

/*
 * Take a byte array and convert it into a Waypoint
 */
VikWaypoint *vik_waypoint_unmarshall (uint8_t *data, unsigned int datalen)
{
  unsigned int len;
  VikWaypoint *new_wp = vik_waypoint_new();
  // This copies the fixed sized elements (i.e. visibility, altitude, image_width, etc...)
  memcpy(new_wp, data, sizeof(*new_wp));
  data += sizeof(*new_wp);

  // Now the variant sized strings...
#define vwu_get(s) \
  len = *(unsigned int *)data; \
  data += sizeof(len); \
  if (len) { \
    (s) = g_strdup((char *)data); \
  } else { \
    (s) = NULL; \
  } \
  data += len;

  vwu_get(new_wp->name);
  vwu_get(new_wp->comment);
  vwu_get(new_wp->description);
  vwu_get(new_wp->source);
  vwu_get(new_wp->type);
  vwu_get(new_wp->url);
  vwu_get(new_wp->image); 
  vwu_get(new_wp->symbol);
  
  return new_wp;
#undef vwu_get
}

