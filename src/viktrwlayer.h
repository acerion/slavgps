/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2011-2013, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _VIKING_TRWLAYER_H
#define _VIKING_TRWLAYER_H

#include <stdbool.h>
#include <stdint.h>

#include <unordered_map>

#include "viklayer.h"
#include "vikviewport.h"
#include "vikwaypoint.h"
#include "viktrack.h"
#include "viklayerspanel.h"


using namespace SlavGPS;


typedef GtkTreeIter TreeIndex;

#ifdef __cplusplus
extern "C" {
#endif


#define VIK_TRW_LAYER_TYPE            (vik_trw_layer_get_type ())
#define VIK_TRW_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_TRW_LAYER_TYPE, VikTrwLayer))
#define VIK_TRW_LAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_TRW_LAYER_TYPE, VikTrwLayerClass))
#define IS_VIK_TRW_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_TRW_LAYER_TYPE))
#define IS_VIK_TRW_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_TRW_LAYER_TYPE))

enum {
  VIK_TRW_LAYER_SUBLAYER_TRACKS,
  VIK_TRW_LAYER_SUBLAYER_WAYPOINTS,
  VIK_TRW_LAYER_SUBLAYER_TRACK,
  VIK_TRW_LAYER_SUBLAYER_WAYPOINT,
  VIK_TRW_LAYER_SUBLAYER_ROUTES,
  VIK_TRW_LAYER_SUBLAYER_ROUTE
};

typedef struct _VikTrwLayerClass VikTrwLayerClass;
struct _VikTrwLayerClass
{
  VikLayerClass object_class;
};


GType vik_trw_layer_get_type ();

typedef struct _VikTrwLayer VikTrwLayer;

typedef struct {
  char *description;
  char *author;
  //bool has_time;
  char *timestamp; // TODO: Consider storing as proper time_t.
  char *keywords; // TODO: handling/storing a GList of individual tags?
} VikTRWMetadata;

VikTRWMetadata *vik_trw_metadata_new();
void vik_trw_metadata_free ( VikTRWMetadata *metadata);
VikTRWMetadata *vik_trw_layer_get_metadata ( VikTrwLayer *vtl );
void vik_trw_layer_set_metadata ( VikTrwLayer *vtl, VikTRWMetadata *metadata);

bool vik_trw_layer_find_date ( VikTrwLayer *vtl, const char *date_str, VikCoord *position, VikViewport *vvp, bool do_tracks, bool select );

/* These are meant for use in file loaders (gpspoint.c, gpx.c, etc).
 * These copy the name, so you should free it if necessary. */
void vik_trw_layer_filein_add_waypoint ( VikTrwLayer *vtl, char *name, Waypoint * wp);
void vik_trw_layer_filein_add_track ( VikTrwLayer *vtl, char *name, Track * trk);

int vik_trw_layer_get_property_tracks_line_thickness ( VikTrwLayer *vtl );

void vik_trw_layer_add_waypoint ( VikTrwLayer *vtl, char *name, Waypoint * wp);
void vik_trw_layer_add_track ( VikTrwLayer *vtl, char *name, Track * trk);
void vik_trw_layer_add_route ( VikTrwLayer *vtl, char *name, Track * trk);

// Waypoint returned is the first one
Waypoint * vik_trw_layer_get_waypoint ( VikTrwLayer *vtl, const char *name );

// Track returned is the first one
Track * vik_trw_layer_get_track ( VikTrwLayer *vtl, const char *name );
bool vik_trw_layer_delete_track ( VikTrwLayer *vtl, Track * trk);
bool vik_trw_layer_delete_route ( VikTrwLayer *vtl, Track * trk);

bool vik_trw_layer_auto_set_view ( VikTrwLayer *vtl, VikViewport *vvp );
bool vik_trw_layer_find_center ( VikTrwLayer *vtl, VikCoord *dest );
std::unordered_map<sg_uid_t, Track *> & vik_trw_layer_get_tracks(VikTrwLayer * l);
std::unordered_map<sg_uid_t, Track *> & vik_trw_layer_get_routes(VikTrwLayer * l);
std::unordered_map<sg_uid_t, Waypoint *> & vik_trw_layer_get_waypoints(VikTrwLayer * l);
bool vik_trw_layer_is_empty ( VikTrwLayer *vtl );

bool vik_trw_layer_new_waypoint ( VikTrwLayer *vtl, GtkWindow *w, const VikCoord *def_coord );

VikCoordMode vik_trw_layer_get_coord_mode ( VikTrwLayer *vtl );

bool vik_trw_layer_uniquify ( VikTrwLayer *vtl, VikLayersPanel *vlp );

void vik_trw_layer_delete_all_waypoints ( VikTrwLayer *vtl );
void vik_trw_layer_delete_all_tracks ( VikTrwLayer *vtl );
void vik_trw_layer_delete_all_routes ( VikTrwLayer *vtl );
void trw_layer_cancel_tps_of_track ( VikTrwLayer *vtl, Track * trk);

void vik_trw_layer_reset_waypoints ( VikTrwLayer *vtl );

void vik_trw_layer_draw_highlight ( VikTrwLayer *vtl, VikViewport *vvp );
void vik_trw_layer_draw_highlight_item ( VikTrwLayer *vtl, Track * trk, Waypoint * wp, VikViewport *vvp );
void vik_trw_layer_draw_highlight_items ( VikTrwLayer *vtl, std::unordered_map<sg_uid_t, Track *> * tracks, std::unordered_map<sg_uid_t, Waypoint *> * waypoints, VikViewport *vvp );

// For creating a list of tracks with the corresponding layer it is in
//  (thus a selection of tracks may be from differing layers)
typedef struct {
  Track * trk;
  VikTrwLayer *vtl;
} vik_trw_track_list_t;

typedef GList* (*VikTrwlayerGetTracksAndLayersFunc) (VikLayer*, void *);
GList *vik_trw_layer_build_track_list_t ( VikTrwLayer *vtl, GList *tracks );

// For creating a list of waypoints with the corresponding layer it is in
//  (thus a selection of waypoints may be from differing layers)
typedef struct {
  Waypoint * wp;
  VikTrwLayer *vtl;
} vik_trw_waypoint_list_t;

typedef GList* (*VikTrwlayerGetWaypointsAndLayersFunc) (VikLayer*, void *);
GList *vik_trw_layer_build_waypoint_list_t ( VikTrwLayer *vtl, GList *waypoints );

GdkPixbuf* get_wp_sym_small ( char *symbol );

/* Exposed Layer Interface function definitions */
// Intended only for use by other trw_layer subwindows
void trw_layer_verify_thumbnails ( VikTrwLayer *vtl, GtkWidget *vp );
// Other functions only for use by other trw_layer subwindows
char *trw_layer_new_unique_sublayer_name ( VikTrwLayer *vtl, int sublayer_type, const char *name );
void trw_layer_waypoint_rename ( VikTrwLayer *vtl, Waypoint * wp, const char *new_name );
void trw_layer_waypoint_reset_icon ( VikTrwLayer *vtl, Waypoint * wp);
void trw_layer_calculate_bounds_waypoints ( VikTrwLayer *vtl );

bool vik_trw_layer_get_tracks_visibility ( VikTrwLayer *vtl );
bool vik_trw_layer_get_routes_visibility ( VikTrwLayer *vtl );
bool vik_trw_layer_get_waypoints_visibility ( VikTrwLayer *vtl );

void trw_layer_update_treeview ( VikTrwLayer *vtl, Track * trk);

void trw_layer_dialog_shift ( VikTrwLayer *vtl, GtkWindow *dialog, VikCoord *coord, bool vertical );

sg_uid_t trw_layer_track_find_uuid(std::unordered_map<sg_uid_t, Track *> & tracks, Track * trk);

sg_uid_t trw_layer_waypoint_find_uuid(std::unordered_map<sg_uid_t, Waypoint *> & waypoints, Waypoint * wp);

void trw_layer_zoom_to_show_latlons ( VikTrwLayer *vtl, VikViewport *vvp, struct LatLon maxmin[2] );

std::unordered_map<sg_uid_t, TreeIndex *> & vik_trw_layer_get_tracks_iters(VikTrwLayer * vtl);
std::unordered_map<sg_uid_t, TreeIndex *> & vik_trw_layer_get_routes_iters(VikTrwLayer * vtl);
std::unordered_map<sg_uid_t, TreeIndex *> & vik_trw_layer_get_waypoints_iters(VikTrwLayer * vtl);

#define VIK_SETTINGS_LIST_DATE_FORMAT "list_date_format"

#ifdef __cplusplus
}
#endif




GList * vik_trw_layer_get_track_values(GList ** list, std::unordered_map<sg_uid_t, Track *> & tracks);

namespace SlavGPS {





	class LayerTRW {



	};





}





#endif
