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
 */

#ifndef _SG_TRACK_INTERNAL_H_
#define _SG_TRACK_INTERNAL_H_




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <list>
#include <cstdint>
#include <cmath>
#include <time.h>

#include <QColor>
#include <QMenu>

#include "coord.h"
#include "bbox.h"
#include "globals.h"
#include "tree_view.h"
#include "layer.h"
#include "layer_trw_track.h"
#include "file.h"




namespace SlavGPS {




	class Trackpoint2;
	class Trackpoint;
	typedef bool (* compare_trackpoints_t)(const Trackpoint * a, const Trackpoint * b);


	class TrackPropertiesDialog;
	class TrackProfileDialog;




	class Trackpoint {
	public:

		Trackpoint() {};
		Trackpoint(const Trackpoint& tp);
		Trackpoint(Trackpoint const& tp_a, Trackpoint const& tp_b, CoordMode coord_mode);
		~Trackpoint() {};


		void set_name(const QString & new_name);
		static bool compare_timestamps(const Trackpoint * a, const Trackpoint * b); /* compare_trackpoints_t */



		QString name;
		Coord coord;
		bool newsegment = false;
		bool has_timestamp = false;
		time_t timestamp = 0;

		double altitude = VIK_DEFAULT_ALTITUDE; /* VIK_DEFAULT_ALTITUDE if data unavailable. */
		double speed = NAN;  	                /* NAN if data unavailable. */
		double course = NAN;                    /* NAN if data unavailable. */

		unsigned int nsats = 0;     /* Number of satellites used. 0 if data unavailable. */

		GPSFixMode fix_mode = GPSFixMode::NOT_SEEN; /* GPSFixMode::NOT_SEEN if data unavailable. */
		double hdop = VIK_DEFAULT_DOP;                 /* VIK_DEFAULT_DOP if data unavailable. */
		double vdop = VIK_DEFAULT_DOP;                 /* VIK_DEFAULT_DOP if data unavailable. */
		double pdop = VIK_DEFAULT_DOP;                 /* VIK_DEFAULT_DOP if data unavailable. */
	};




	/*
	  Convenience class for pair of vectors representing track's
	  Speed/Timestamp or Altitude/Timestamp information.

	  The two vectors are put in one data structure because
	  values in the vectors are calculated during one iteration
	  over the same trackpoint list. If I decided to split the
	  two vectors, I would have to collect data in the vectors
	  during two separate iterations over trackpoint list. This
	  would take twice as much time.
	*/
	class TrackData {
	public:
		TrackData();
		TrackData(int n_data_points);
		~TrackData();

		TrackData & operator=(const TrackData & other);

		void invalidate(void);
		void calculate_min_max(void);
		void allocate_vector(int n_data_points);

		double * x = NULL;
		double * y = NULL;

		double x_min = 0.0;
		double x_max = 0.0;

		double y_min = 0.0;
		double y_max = 0.0;

		bool valid = false;
		int n_points = 0;
	};




	/*
	  Instead of having a separate Route type, routes are
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

	class Track : public TreeItem {
		Q_OBJECT
	public:
		/* Track/Route differentiation is made either explicitly (through 'is_route' argument)
		   or implicitly (is copied from 'from' argument'). */
		Track(bool is_route);
		Track(const Track& from);
		Track(const Track& from, TrackPoints::iterator first, TrackPoints::iterator last);
		~Track();


		void set_defaults();
		void set_name(const QString & new_name);
		void set_comment(const QString & new_comment);
		void set_description(const QString & new_description);
		void set_source(const QString & new_source);
		void set_type(const QString & new_type);
		void ref();
		void free();

		QString get_tooltip(void);


		/* STL-like container interface. */
		TrackPoints::iterator begin();
		TrackPoints::iterator end();
		bool empty();
		TrackPoints::iterator erase(TrackPoints::iterator first, TrackPoints::iterator last);
		void push_front(Trackpoint * tp);


		void sort(compare_trackpoints_t compare_function);

		void add_trackpoint(Trackpoint * tp, bool recalculate);
		double get_length_to_trackpoint(const Trackpoint * tp) const;
		double get_length() const;
		double get_length_including_gaps() const;
		unsigned long get_tp_count() const;
		unsigned int get_segment_count() const;
		std::list<Track *> * split_into_segments();

		Track * split_at_trackpoint(const Trackpoint2 & tp);

		void smooth_it(bool flat);

		unsigned int merge_segments(void);
		void reverse(void);
		time_t get_duration(bool include_segments) const;
		double get_duration() const; /* private. */

		unsigned long get_dup_point_count() const;
		unsigned long remove_dup_points();
		unsigned long get_same_time_point_count() const;
		unsigned long remove_same_time_points();

		void to_routepoints();

		double get_max_speed() const;
		double get_average_speed() const;
		double get_average_speed_moving(int track_min_stop_length_seconds) const;

		void convert(CoordMode dest_mode);

		bool get_total_elevation_gain(double * up, double * down) const;
		Trackpoint * get_tp_by_dist(double meters_from_start, bool get_next_point, double *tp_metres_from_start);
		Trackpoint * get_closest_tp_by_percentage_dist(double reldist, double *meters_from_start);
		Trackpoint * get_closest_tp_by_percentage_time(double reldist, time_t *seconds_from_start);
		Trackpoint * get_tp_by_max_speed() const;
		Trackpoint * get_tp_by_max_alt() const;
		Trackpoint * get_tp_by_min_alt() const;
		Trackpoint * get_tp_first() const;
		Trackpoint * get_tp_last() const;
		Trackpoint * get_tp_prev(Trackpoint * tp) const;
		bool get_minmax_alt(double * min_alt, double * max_alt) const;

		TrackData make_values_vector_altitude_distance(int compressed_n_points) const;
		TrackData make_values_vector_gradient_distance(int compressed_n_points) const;
		TrackData make_values_vector_speed_time(int compressed_n_points) const;
		TrackData make_values_vector_distance_time(int compressed_n_points) const;
		TrackData make_values_vector_altitude_time(int compressed_n_points) const;
		TrackData make_values_vector_speed_distance(int compressed_n_points) const;
		TrackData make_values_distance_time(void) const;
		TrackData make_values_altitude_time(void) const;


		void marshall(uint8_t ** data, size_t * data_len);
		static Track * unmarshall(uint8_t * data, size_t data_len);

		void calculate_bounds();
		void find_maxmin(LatLonMinMax & min_max);

		void anonymize_times();
		void interpolate_times();
		void apply_dem_data_common(bool skip_existing_elevations);
		unsigned long apply_dem_data(bool skip_existing);
		void apply_dem_data_last_trackpoint();
		unsigned long smooth_missing_elevation_data(bool flat);

		void steal_and_append_trackpoints(Track * from);

		Coord * cut_back_to_double_point();

		static int compare_timestamp(const void * x, const void * y);

		void set_properties_dialog(TrackPropertiesDialog * dialog);
		void update_properties_dialog(void);
		void clear_properties_dialog();

		void set_profile_dialog(TrackProfileDialog * dialog);
		void update_profile_dialog(void);
		void clear_profile_dialog();

		void export_track(const QString & title, const QString & default_file_name, SGFileType file_type);


		TrackPoints::iterator erase_trackpoint(TrackPoints::iterator iter);
		TrackPoints::iterator delete_trackpoint(TrackPoints::iterator iter);
		void insert(Trackpoint * tp_at, Trackpoint * tp_new, bool before);

		TrackPoints::iterator get_last();
		std::list<Rect *> * get_rectangles(LatLon * wh);
		CoordMode get_coord_mode(void) const;

		bool add_context_menu_items(QMenu & menu, bool tree_view_context_menu);
		void sublayer_menu_track_route_misc(LayerTRW * parent_layer_, QMenu & menu, QMenu * upload_submenu);
		void sublayer_menu_track_misc(LayerTRW * parent_layer_, QMenu & menu, QMenu * upload_submenu);

		bool handle_selection_in_tree(void);

		void draw_tree_item(Viewport * viewport, bool hl_is_allowed, bool hl_is_required);

		QString sublayer_rename_request(const QString & new_name);

		std::list<Rect *> * get_map_rectangles(double zoom_level);

		void create_tp_next_to_reference_tp(Trackpoint2 * reference_tp, bool before);

		void delete_sublayer(bool confirm);

		void remove_last_trackpoint(void);

#ifdef VIK_CONFIG_GOOGLE
		bool is_valid_google_route();
#endif

		bool properties_dialog();

		LayerTRW * get_parent_layer_trw() const;

		/* Track editing tool. */
		Trackpoint2 selected_tp;

		/* QString name; */ /* Inherited from TreeItem. */
		QString comment;
		QString description;
		QString source;
		QString type;

		TrackPoints trackpoints;
		//bool visible = false;
		TrackDrawNameMode draw_name_mode = TrackDrawNameMode::NONE;
		uint8_t max_number_dist_labels = 0;

		uint8_t ref_count = 0;
		bool has_color = false;
		QColor color;
		LatLonBBox bbox;

		TrackPropertiesDialog * props_dialog = NULL;
		TrackProfileDialog * profile_dialog = NULL;

	private:
		static void smoothie(TrackPoints::iterator start, TrackPoints::iterator stop, double elev1, double elev2, unsigned int points);
		void recalculate_bounds_last_tp();

	public slots:
		void goto_startpoint_cb(void);
		void goto_center_cb(void);
		void goto_endpoint_cb(void);
		void goto_max_speed_cb(void);
		void goto_max_alt_cb(void);
		void goto_min_alt_cb(void);

		void anonymize_times_cb(void);
		void interpolate_times_cb(void);

		void properties_dialog_cb(void);
		void statistics_dialog_cb(void);
		void profile_dialog_cb(void);

		void missing_elevation_data_interp_cb(void);
		void missing_elevation_data_flat_cb(void);

		void rezoom_to_show_full_cb(void);

		void apply_dem_data_all_cb(void);
		void apply_dem_data_only_missing_cb(void);

		void export_track_as_gpx_cb(void);

		void open_diary_cb(void);
		void open_astro_cb(void);

		void reverse_cb(void);

		void upload_to_gps_cb(void);
		void upload_to_osm_traces_cb(void);

		void convert_track_route_cb(void);

#ifdef VIK_CONFIG_GEOTAG
		void geotagging_track_cb(void);
#endif

#ifdef VIK_CONFIG_GOOGLE
		void google_route_webpage_cb(void);
#endif

#ifndef WINDOWS
		void track_use_with_filter_cb(void);
#endif

		void split_by_timestamp_cb(void);
		void split_by_n_points_cb(void);
		void split_by_segments_cb(void);

		void refine_route_cb(void);

		void cut_sublayer_cb(void);
		void copy_sublayer_cb(void);
		void delete_sublayer_cb(void);
	};




} /* namespace SlavGPS */




Q_DECLARE_METATYPE(SlavGPS::Track*)




#endif /* #ifndef _SG_TRACK_INTERNAL_H_ */
