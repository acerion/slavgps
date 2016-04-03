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

#ifndef _VIKING_TRACK_H
#define _VIKING_TRACK_H

#include <time.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <stdbool.h>
#include <stdint.h>


#include "vikcoord.h"
#include "bbox.h"

G_BEGIN_DECLS

/* todo important: put these in their own header file, maybe.probably also rename */

#define VIK_TRACK(x) ((VikTrack *)(x))
#define VIK_TRACKPOINT(x) ((VikTrackpoint *)(x))

typedef struct _VikTrackpoint VikTrackpoint;
struct _VikTrackpoint {
  char* name;
  VikCoord coord;
  bool newsegment;
  bool has_timestamp;
  time_t timestamp;
  double altitude;	/* VIK_DEFAULT_ALTITUDE if data unavailable */
  double speed;  	/* NAN if data unavailable */
  double course;   /* NAN if data unavailable */
  unsigned int nsats;      /* number of satellites used. 0 if data unavailable */
#define VIK_GPS_MODE_NOT_SEEN	0	/* mode update not seen yet */
#define VIK_GPS_MODE_NO_FIX	1	/* none */
#define VIK_GPS_MODE_2D  	2	/* good for latitude/longitude */
#define VIK_GPS_MODE_3D  	3	/* good for altitude/climb too */
#define VIK_GPS_MODE_DGPS	4
#define VIK_GPS_MODE_PPS 	5	/* military signal used */
  int fix_mode;    /* VIK_GPS_MODE_NOT_SEEN if data unavailable */
  double hdop;     /* VIK_DEFAULT_DOP if data unavailable */
  double vdop;     /* VIK_DEFAULT_DOP if data unavailable */
  double pdop;     /* VIK_DEFAULT_DOP if data unavailable */
};

typedef enum {
  TRACK_DRAWNAME_NO=0,
  TRACK_DRAWNAME_CENTRE,
  TRACK_DRAWNAME_START,
  TRACK_DRAWNAME_END,
  TRACK_DRAWNAME_START_END,
  TRACK_DRAWNAME_START_END_CENTRE,
  NUM_TRACK_DRAWNAMES
} VikTrackDrawnameType;

// Instead of having a separate VikRoute type, routes are considered tracks
//  Thus all track operations must cope with a 'route' version
//  [track functions handle having no timestamps anyway - so there is no practical difference in most cases]
//  This is simpler than having to rewrite particularly every track function for route version
//   given that they do the same things
//  Mostly this matters in the display in deciding where and how they are shown
typedef struct _VikTrack VikTrack;
struct _VikTrack {
  GList *trackpoints;
  bool visible;
  bool is_route;
  VikTrackDrawnameType draw_name_mode;
  uint8_t max_number_dist_labels;
  char *comment;
  char *description;
  char *source;
  char *type;
  uint8_t ref_count;
  char *name;
  GtkWidget *property_dialog;
  bool has_color;
  GdkColor color;
  LatLonBBox bbox;
};

VikTrack *vik_track_new();
void vik_track_set_defaults(VikTrack *tr);
void vik_track_set_name(VikTrack *tr, const char *name);
void vik_track_set_comment(VikTrack *tr, const char *comment);
void vik_track_set_description(VikTrack *tr, const char *description);
void vik_track_set_source(VikTrack *tr, const char *source);
void vik_track_set_type(VikTrack *tr, const char *type);
void vik_track_ref(VikTrack *tr);
void vik_track_free(VikTrack *tr);
VikTrack *vik_track_copy ( const VikTrack *tr, bool copy_points );
void vik_track_set_comment_no_copy(VikTrack *tr, char *comment);
VikTrackpoint *vik_trackpoint_new();
void vik_trackpoint_free(VikTrackpoint *tp);
VikTrackpoint *vik_trackpoint_copy(VikTrackpoint *tp);
void vik_trackpoint_set_name(VikTrackpoint *tp, const char *name);

void vik_track_add_trackpoint(VikTrack *tr, VikTrackpoint *tp, bool recalculate);
double vik_track_get_length_to_trackpoint (const VikTrack *tr, const VikTrackpoint *tp);
double vik_track_get_length(const VikTrack *tr);
double vik_track_get_length_including_gaps(const VikTrack *tr);
unsigned long vik_track_get_tp_count(const VikTrack *tr);
unsigned int vik_track_get_segment_count(const VikTrack *tr);
VikTrack **vik_track_split_into_segments(VikTrack *tr, unsigned int *ret_len);
unsigned int vik_track_merge_segments(VikTrack *tr);
void vik_track_reverse(VikTrack *tr);
time_t vik_track_get_duration(const VikTrack *trk, bool include_segments);

unsigned long vik_track_get_dup_point_count ( const VikTrack *vt );
unsigned long vik_track_remove_dup_points ( VikTrack *vt );
unsigned long vik_track_get_same_time_point_count ( const VikTrack *vt );
unsigned long vik_track_remove_same_time_points ( VikTrack *vt );

void vik_track_to_routepoints ( VikTrack *tr );

double vik_track_get_max_speed(const VikTrack *tr);
double vik_track_get_average_speed(const VikTrack *tr);
double vik_track_get_average_speed_moving ( const VikTrack *tr, int stop_length_seconds );

void vik_track_convert ( VikTrack *tr, VikCoordMode dest_mode );
double *vik_track_make_elevation_map ( const VikTrack *tr, uint16_t num_chunks );
void vik_track_get_total_elevation_gain(const VikTrack *tr, double *up, double *down);
VikTrackpoint *vik_track_get_tp_by_dist ( VikTrack *trk, double meters_from_start, bool get_next_point, double *tp_metres_from_start );
VikTrackpoint *vik_track_get_closest_tp_by_percentage_dist ( VikTrack *tr, double reldist, double *meters_from_start );
VikTrackpoint *vik_track_get_closest_tp_by_percentage_time ( VikTrack *tr, double reldist, time_t *seconds_from_start );
VikTrackpoint *vik_track_get_tp_by_max_speed ( const VikTrack *tr );
VikTrackpoint *vik_track_get_tp_by_max_alt ( const VikTrack *tr );
VikTrackpoint *vik_track_get_tp_by_min_alt ( const VikTrack *tr );
VikTrackpoint *vik_track_get_tp_first ( const VikTrack *tr );
VikTrackpoint *vik_track_get_tp_last ( const VikTrack *tr );
VikTrackpoint *vik_track_get_tp_prev ( const VikTrack *tr, VikTrackpoint *tp );
double *vik_track_make_gradient_map ( const VikTrack *tr, uint16_t num_chunks );
double *vik_track_make_speed_map ( const VikTrack *tr, uint16_t num_chunks );
double *vik_track_make_distance_map ( const VikTrack *tr, uint16_t num_chunks );
double *vik_track_make_elevation_time_map ( const VikTrack *tr, uint16_t num_chunks );
double *vik_track_make_speed_dist_map ( const VikTrack *tr, uint16_t num_chunks );
bool vik_track_get_minmax_alt ( const VikTrack *tr, double *min_alt, double *max_alt );
void vik_track_marshall ( VikTrack *tr, uint8_t **data, unsigned int *len);
VikTrack *vik_track_unmarshall (uint8_t *data, unsigned int datalen);

void vik_track_calculate_bounds ( VikTrack *trk );

void vik_track_anonymize_times ( VikTrack *tr );
void vik_track_interpolate_times ( VikTrack *tr );
unsigned long vik_track_apply_dem_data ( VikTrack *tr, bool skip_existing );
void vik_track_apply_dem_data_last_trackpoint ( VikTrack *tr );
unsigned long vik_track_smooth_missing_elevation_data ( VikTrack *tr, bool flat );

void vik_track_steal_and_append_trackpoints ( VikTrack *t1, VikTrack *t2 );

VikCoord *vik_track_cut_back_to_double_point ( VikTrack *tr );

int vik_track_compare_timestamp (const void *x, const void *y);

void vik_track_set_property_dialog(VikTrack *tr, GtkWidget *dialog);
void vik_track_clear_property_dialog(VikTrack *tr);

G_END_DECLS

#endif
