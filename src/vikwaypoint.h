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

#ifndef _VIKING_WAYPOINT_H
#define _VIKING_WAYPOINT_H

#include <stdbool.h>
#include <stdint.h>

#include "vikcoord.h"

#include <gdk-pixbuf/gdk-pixdata.h>

#ifdef __cplusplus
extern "C" {
#endif

/* todo important: put these in their own header file, maybe.probably also rename */

#define VIK_WAYPOINT(x) ((VikWaypoint *)(x))

typedef struct _VikWaypoint VikWaypoint;

struct _VikWaypoint {
  VikCoord coord;
  bool visible;
  bool has_timestamp;
  time_t timestamp;
  double altitude;
  char *name;
  char *comment;
  char *description;
  char *source;
  char *type;
  char *url;
  char *image;
  /* a rather misleading, ugly hack needed for trwlayer's click image.
   * these are the height at which the thumbnail is being drawn, not the 
   * dimensions of the original image. */
  uint8_t image_width;
  uint8_t image_height;
  char *symbol;
  // Only for GUI display
  GdkPixbuf *symbol_pixbuf;
};

VikWaypoint *vik_waypoint_new();
void vik_waypoint_set_name(VikWaypoint *wp, const char *name);
void vik_waypoint_set_comment(VikWaypoint *wp, const char *comment);
void vik_waypoint_set_description(VikWaypoint *wp, const char *description);
void vik_waypoint_set_source(VikWaypoint *wp, const char *source);
void vik_waypoint_set_type(VikWaypoint *wp, const char *type);
void vik_waypoint_set_url(VikWaypoint *wp, const char *url);
void vik_waypoint_set_image(VikWaypoint *wp, const char *image);
void vik_waypoint_set_symbol(VikWaypoint *wp, const char *symname);
void vik_waypoint_free(VikWaypoint * wp);
VikWaypoint *vik_waypoint_copy(const VikWaypoint *wp);
void vik_waypoint_set_comment_no_copy(VikWaypoint *wp, char *comment);
bool vik_waypoint_apply_dem_data ( VikWaypoint *wp, bool skip_existing );
void vik_waypoint_marshall ( VikWaypoint *wp, uint8_t **data, unsigned int *len);
VikWaypoint *vik_waypoint_unmarshall (uint8_t *data, unsigned int datalen);

#ifdef __cplusplus
}
#endif

#endif
