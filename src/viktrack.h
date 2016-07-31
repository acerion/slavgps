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

#ifndef _SG_TRACK_H
#define _SG_TRACK_H




#include <list>

#include <time.h>
#include <glib.h>
#include <gtk/gtk.h>
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




	enum TrackDrawnameType {
		TRACK_DRAWNAME_NO=0,
		TRACK_DRAWNAME_CENTRE,
		TRACK_DRAWNAME_START,
		TRACK_DRAWNAME_END,
		TRACK_DRAWNAME_START_END,
		TRACK_DRAWNAME_START_END_CENTRE,
		NUM_TRACK_DRAWNAMES
	};





	// Instead of having a separate VikRoute type, routes are considered tracks
	//  Thus all track operations must cope with a 'route' version
	//  [track functions handle having no timestamps anyway - so there is no practical difference in most cases]
	//  This is simpler than having to rewrite particularly every track function for route version
	//   given that they do the same things
	//  Mostly this matters in the display in deciding where and how they are shown
	class Track {
	public:

		Track();
		Track(const Track & from);
		Track(const Track & from, GList * new_trackpoints);

		void set_defaults();
		void set_name(const char *name);
		void set_comment(const char *comment);
		void set_description(const char *description);
		void set_source(const char *source);
		void set_type(const char *type);
		void ref();
		void free();

		bool empty();
		void sort(GCompareFunc compare_func);

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
		void get_total_elevation_gain(double *up, double *down);  // const
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

		static void delete_track(Track *);
		std::list<Trackpoint *>::iterator erase_trackpoint(std::list<Trackpoint *>::iterator iter);
		std::list<Trackpoint *>::iterator delete_trackpoint(std::list<Trackpoint *>::iterator iter);
		void insert(Trackpoint * tp_at, Trackpoint * tp_new, bool before);

		std::list<Trackpoint *>::iterator get_last();
		std::list<Rect *> * get_rectangles(LatLon * wh);
		VikCoordMode get_coord_mode();


		GList *trackpoints;
		std::list<Trackpoint *> * trackpointsB;
		bool visible;
		bool is_route;
		TrackDrawnameType draw_name_mode;
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
	private:
		static void smoothie(std::list<Trackpoint *>::iterator start, std::list<Trackpoint *>::iterator stop, double elev1, double elev2, unsigned int points);
		void recalculate_bounds_last_tp();


	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TRACK_H */
