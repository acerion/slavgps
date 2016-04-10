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





namespace SlavGPS {




	enum FixMode {
		VIK_GPS_MODE_NOT_SEEN = 0,     /* mode update not seen yet */
		VIK_GPS_MODE_NO_FIX   = 1,     /* none */
		VIK_GPS_MODE_2D       = 2,     /* good for latitude/longitude */
		VIK_GPS_MODE_3D       = 3,     /* good for altitude/climb too */
		VIK_GPS_MODE_DGPS     = 4,
		VIK_GPS_MODE_PPS      = 5      /* military signal used */
	};





	class Trackpoint {
	public:

		Trackpoint();
		~Trackpoint();
		Trackpoint(const Trackpoint & tp);
		void set_name(char const * name);

		static void vik_trackpoint_free(Trackpoint *tp);

		char * name;
		VikCoord coord;
		bool newsegment;
		bool has_timestamp;
		time_t timestamp;

		double altitude;	/* VIK_DEFAULT_ALTITUDE if data unavailable */
		double speed;  	        /* NAN if data unavailable */
		double course;          /* NAN if data unavailable */

		unsigned int nsats;     /* number of satellites used. 0 if data unavailable */

		enum FixMode fix_mode;  /* VIK_GPS_MODE_NOT_SEEN if data unavailable */
		double hdop;            /* VIK_DEFAULT_DOP if data unavailable */
		double vdop;            /* VIK_DEFAULT_DOP if data unavailable */
		double pdop;            /* VIK_DEFAULT_DOP if data unavailable */
	};





} /* namespace */





/* todo important: put these in their own header file, maybe.probably also rename */


using namespace SlavGPS;



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
typedef struct _Track Track;
struct _Track {
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

Track * vik_track_new();
void vik_track_set_defaults(Track * trk);
void vik_track_set_name(Track * trk, const char *name);
void vik_track_set_comment(Track * trk, const char *comment);
void vik_track_set_description(Track * trk, const char *description);
void vik_track_set_source(Track * trk, const char *source);
void vik_track_set_type(Track * trk, const char *type);
void vik_track_ref(Track * trk);
void vik_track_free(Track * trk);
Track * vik_track_copy(const Track * trk, bool copy_points);
void vik_track_set_comment_no_copy(Track * trk, char *comment);

void vik_track_add_trackpoint(Track * trk, Trackpoint * tp, bool recalculate);
double vik_track_get_length_to_trackpoint(const Track * trk, const Trackpoint * tp);
double vik_track_get_length(const Track * trk);
double vik_track_get_length_including_gaps(const Track * trk);
unsigned long vik_track_get_tp_count(const Track * trk);
unsigned int vik_track_get_segment_count(const Track * trk);
Track **vik_track_split_into_segments(Track * trk, unsigned int *ret_len);
unsigned int vik_track_merge_segments(Track * trk);
void vik_track_reverse(Track * trk);
time_t vik_track_get_duration(const Track * trk, bool include_segments);

unsigned long vik_track_get_dup_point_count(const Track * trk);
unsigned long vik_track_remove_dup_points(Track * trk);
unsigned long vik_track_get_same_time_point_count(const Track * trk);
unsigned long vik_track_remove_same_time_points(Track * trk);

void vik_track_to_routepoints(Track * trk);

double vik_track_get_max_speed(const Track * trk);
double vik_track_get_average_speed(const Track * trk);
double vik_track_get_average_speed_moving(const Track * trk, int stop_length_seconds);

void vik_track_convert(Track * trk, VikCoordMode dest_mode);
double *vik_track_make_elevation_map(const Track * trk, uint16_t num_chunks);
void vik_track_get_total_elevation_gain(const Track * trk, double *up, double *down);
Trackpoint * vik_track_get_tp_by_dist(Track *trk, double meters_from_start, bool get_next_point, double *tp_metres_from_start);
Trackpoint * vik_track_get_closest_tp_by_percentage_dist(Track * trk, double reldist, double *meters_from_start);
Trackpoint * vik_track_get_closest_tp_by_percentage_time(Track * trk, double reldist, time_t *seconds_from_start);
Trackpoint * vik_track_get_tp_by_max_speed(const Track * trk);
Trackpoint * vik_track_get_tp_by_max_alt(const Track * trk);
Trackpoint * vik_track_get_tp_by_min_alt(const Track * trk);
Trackpoint * vik_track_get_tp_first(const Track * trk);
Trackpoint * vik_track_get_tp_last(const Track * trk);
Trackpoint * vik_track_get_tp_prev(const Track * trk, Trackpoint * tp);
double *vik_track_make_gradient_map(const Track * trk, uint16_t num_chunks);
double *vik_track_make_speed_map(const Track * trk, uint16_t num_chunks);
double *vik_track_make_distance_map(const Track * trk, uint16_t num_chunks);
double *vik_track_make_elevation_time_map(const Track * trk, uint16_t num_chunks);
double *vik_track_make_speed_dist_map(const Track * trk, uint16_t num_chunks);
bool vik_track_get_minmax_alt(const Track * trk, double *min_alt, double *max_alt);
void vik_track_marshall(Track * trk, uint8_t **data, size_t * len);
Track *vik_track_unmarshall(uint8_t *data, size_t datalen);

void vik_track_calculate_bounds(Track *trk);

void vik_track_anonymize_times(Track * trk);
void vik_track_interpolate_times(Track * trk);
unsigned long vik_track_apply_dem_data(Track * trk, bool skip_existing);
void vik_track_apply_dem_data_last_trackpoint(Track * trk);
unsigned long vik_track_smooth_missing_elevation_data(Track * trk, bool flat);

void vik_track_steal_and_append_trackpoints(Track *trk1, Track *trk2);

VikCoord *vik_track_cut_back_to_double_point(Track * trk);

int vik_track_compare_timestamp(const void *x, const void *y);

void vik_track_set_property_dialog(Track * trk, GtkWidget *dialog);
void vik_track_clear_property_dialog(Track * trk);





#endif /* #ifndef _VIKING_TRACK_H */
