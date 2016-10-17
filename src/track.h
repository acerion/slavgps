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

#ifndef _SG_TRACK_H_
#define _SG_TRACK_H_




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <list>
#include <cstdint>

#include <time.h>
//#include <gtk/gtk.h>

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include "coord.h"
#include "bbox.h"
#include "globals.h"
#include "slav_qt.h"




/* cf with vik_track_get_minmax_alt internals. */
#define VIK_VAL_MIN_ALT 25000.0
#define VIK_VAL_MAX_ALT -5000.0




namespace SlavGPS {




	enum FixMode {
		VIK_GPS_MODE_NOT_SEEN = 0,     /* Mode update not seen yet. */
		VIK_GPS_MODE_NO_FIX   = 1,     /* None. */
		VIK_GPS_MODE_2D       = 2,     /* Good for latitude/longitude. */
		VIK_GPS_MODE_3D       = 3,     /* Good for altitude/climb too. */
		VIK_GPS_MODE_DGPS     = 4,
		VIK_GPS_MODE_PPS      = 5      /* Military signal used. */
	};




	class Trackpoint;
	typedef bool (* compare_trackpoints_t)(const Trackpoint * a, const Trackpoint * b);




	class Trackpoint {
	public:

		Trackpoint();
		Trackpoint(const Trackpoint& tp);
		Trackpoint(Trackpoint const& tp_a, Trackpoint const& tp_b, VikCoordMode coord_mode);
		~Trackpoint();


		void set_name(char const * name);
		static bool compare_timestamps(const Trackpoint * a, const Trackpoint * b); /* compare_trackpoints_t */



		char * name = NULL;
		VikCoord coord;
		bool newsegment = false;
		bool has_timestamp = false;
		time_t timestamp = 0;

		double altitude = VIK_DEFAULT_ALTITUDE; /* VIK_DEFAULT_ALTITUDE if data unavailable. */
		double speed = NAN;  	                /* NAN if data unavailable. */
		double course = NAN;                    /* NAN if data unavailable. */

		unsigned int nsats = 0;     /* Number of satellites used. 0 if data unavailable. */

		enum FixMode fix_mode = VIK_GPS_MODE_NOT_SEEN; /* VIK_GPS_MODE_NOT_SEEN if data unavailable. */
		double hdop = VIK_DEFAULT_DOP;                 /* VIK_DEFAULT_DOP if data unavailable. */
		double vdop = VIK_DEFAULT_DOP;                 /* VIK_DEFAULT_DOP if data unavailable. */
		double pdop = VIK_DEFAULT_DOP;                 /* VIK_DEFAULT_DOP if data unavailable. */
	};




	enum TrackDrawnameType {
		TRACK_DRAWNAME_NO=0,
		TRACK_DRAWNAME_CENTRE,
		TRACK_DRAWNAME_START,
		TRACK_DRAWNAME_END,
		TRACK_DRAWNAME_START_END,
		TRACK_DRAWNAME_START_END_CENTRE,
		NUM_TRACK_DRAWNAMES
	};




	typedef std::list<Trackpoint *> TrackPoints;




	/*
	  Instead of having a separate VikRoute type, routes are
	  considered tracks.  Thus all track operations must cope with
	  a 'route' version.

	  Track functions handle having no timestamps anyway - so
	  there is no practical difference in most cases.

	  This is simpler than having to rewrite particularly every
	  track function for route version, given that they do the
	  same things.

	  Mostly this matters in the display in deciding where and how
	  they are shown.
	*/

	class Track {
	public:

		Track();
		Track(const Track& from);
		Track(const Track& from, TrackPoints::iterator first, TrackPoints::iterator last);
		~Track();


		void set_defaults();
		void set_name(const char *name);
		void set_comment(const char *comment);
		void set_description(const char *description);
		void set_source(const char *source);
		void set_type(const char *type);
		void ref();
		void free();


		/* STL-like container interface. */
		TrackPoints::iterator begin();
		TrackPoints::iterator end();
		bool empty();
		TrackPoints::iterator erase(TrackPoints::iterator first, TrackPoints::iterator last);
		void push_front(Trackpoint * tp);


		void sort(compare_trackpoints_t compare_function);

		void set_comment_no_copy(char * comment);

		void add_trackpoint(Trackpoint * tp, bool recalculate);
		double get_length_to_trackpoint(const Trackpoint * tp); // const
		double get_length();  // const
		double get_length_including_gaps();  // const
		unsigned long get_tp_count();  // const
		unsigned int get_segment_count();  // const
		std::list<Track *> * split_into_segments();

		unsigned int merge_segments(void);
		void reverse(void);
		time_t get_duration(bool include_segments); // const
		double get_duration(); /* private. */

		unsigned long get_dup_point_count();  // const
		unsigned long remove_dup_points();
		unsigned long get_same_time_point_count(); // const
		unsigned long remove_same_time_points();

		void to_routepoints();

		double get_max_speed();  // const
		double get_average_speed();  // const
		double get_average_speed_moving(int stop_length_seconds);  // const

		void convert(VikCoordMode dest_mode);
		double * make_elevation_map(uint16_t num_chunks);  // const
		bool get_total_elevation_gain(double *up, double *down);  // const
		Trackpoint * get_tp_by_dist(double meters_from_start, bool get_next_point, double *tp_metres_from_start);
		Trackpoint * get_closest_tp_by_percentage_dist(double reldist, double *meters_from_start);
		Trackpoint * get_closest_tp_by_percentage_time(double reldist, time_t *seconds_from_start);
		Trackpoint * get_tp_by_max_speed();  // const
		Trackpoint * get_tp_by_max_alt();  // const
		Trackpoint * get_tp_by_min_alt();  // const
		Trackpoint * get_tp_first();  // const
		Trackpoint * get_tp_last();  // const
		Trackpoint * get_tp_prev(Trackpoint * tp);  // const
		double * make_gradient_map(uint16_t num_chunks);  // const
		double * make_speed_map(uint16_t num_chunks);  // const
		double * make_distance_map(uint16_t num_chunks);  // const
		double * make_elevation_time_map(uint16_t num_chunks);  // const
		double * make_speed_dist_map(uint16_t num_chunks);  // const
		bool get_minmax_alt(double *min_alt, double *max_alt);  // const
		void marshall(uint8_t **data, size_t * len);
		static Track * unmarshall(uint8_t *data, size_t datalen);

		void calculate_bounds();

		void anonymize_times();
		void interpolate_times();
		unsigned long apply_dem_data(bool skip_existing);
		void apply_dem_data_last_trackpoint();
		unsigned long smooth_missing_elevation_data(bool flat);

		void steal_and_append_trackpoints(Track * from);

		VikCoord * cut_back_to_double_point();

		static int compare_timestamp(const void * x, const void * y);

		void set_property_dialog(GtkWidget *dialog);
		void clear_property_dialog();

		TrackPoints::iterator erase_trackpoint(TrackPoints::iterator iter);
		TrackPoints::iterator delete_trackpoint(TrackPoints::iterator iter);
		void insert(Trackpoint * tp_at, Trackpoint * tp_new, bool before);

		TrackPoints::iterator get_last();
		std::list<Rect *> * get_rectangles(LatLon * wh);
		VikCoordMode get_coord_mode();


		TrackPoints * trackpointsB = NULL;
		bool visible = false;
		bool is_route = false;
		TrackDrawnameType draw_name_mode = TRACK_DRAWNAME_NO;
		uint8_t max_number_dist_labels = 0;
		char * comment = NULL;
		char * description = NULL;
		char * source = NULL;
		char * type = NULL;
		uint8_t ref_count = 0;
		char * name = NULL;
		GtkWidget * property_dialog = NULL;
		bool has_color = false;
		GdkColor color;
		LatLonBBox bbox;
	private:
		static void smoothie(TrackPoints::iterator start, TrackPoints::iterator stop, double elev1, double elev2, unsigned int points);
		void recalculate_bounds_last_tp();
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TRACK_H_ */
