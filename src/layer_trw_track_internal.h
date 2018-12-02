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




#include <list>
#include <cstdint>
#include <cmath>
#include <time.h>




#include <QColor>
#include <QMenu>




#include "coord.h"
#include "bbox.h"
#include "tree_view.h"
#include "layer_trw_track.h"
#include "dialog.h"
#include "measurements.h"




namespace SlavGPS {




	class TrackpointIter;
	class Trackpoint;
	typedef bool (* compare_trackpoints_t)(const Trackpoint * a, const Trackpoint * b);


	class TrackPropertiesDialog;
	class TrackProfileDialog;

	enum class SGFileType;

	class LayerTRW;




	class Trackpoint {
	public:

		Trackpoint() {};
		Trackpoint(const Trackpoint& tp);
		Trackpoint(Trackpoint const& tp_a, Trackpoint const& tp_b, CoordMode coord_mode);
		~Trackpoint() {};


		void set_name(const QString & new_name);
		static bool compare_timestamps(const Trackpoint * a, const Trackpoint * b); /* compare_trackpoints_t */

		void set_timestamp(const Time & value);
		void set_timestamp(time_t value);

		QString name;
		Coord coord;
		bool newsegment = false;
		Time timestamp; /* Invalid by default (trackpoint doesn't have a timestamp). */

		double altitude = VIK_DEFAULT_ALTITUDE; /* VIK_DEFAULT_ALTITUDE if data unavailable. */
		double speed = NAN;  	                /* NAN if data unavailable. */
		double course = NAN;                    /* NAN if data unavailable. */

		unsigned int nsats = 0;     /* Number of satellites used. 0 if data unavailable. */

		GPSFixMode fix_mode = GPSFixMode::NotSeen;     /* GPSFixMode::NotSeen if data unavailable. */
		double hdop = VIK_DEFAULT_DOP;                 /* VIK_DEFAULT_DOP if data unavailable. */
		double vdop = VIK_DEFAULT_DOP;                 /* VIK_DEFAULT_DOP if data unavailable. */
		double pdop = VIK_DEFAULT_DOP;                 /* VIK_DEFAULT_DOP if data unavailable. */
	};




	/*
	  Convenience class for pair of vectors representing track's
	  Speed/Time or Altitude/Timestamp information.

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

		TrackData compress(int compressed_size) const;

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
		Track(const Track & from);
		Track(const Track & from, const TrackPoints::iterator & begin, const TrackPoints::iterator & end);
		~Track();


		void set_defaults();
		void set_name(const QString & new_name);
		void set_comment(const QString & new_comment);
		void set_description(const QString & new_description);
		void set_source(const QString & new_source);
		void set_type(const QString & new_type);
		void ref();
		void free();


		/* Generate an icon for itself. */
		void self_assign_icon(void);
		/* Generate a timestamp for itself. */
		void self_assign_timestamp(void);


		QString get_tooltip(void) const;


		/* STL-like container interface. */
		TrackPoints::iterator begin();
		TrackPoints::iterator end();
		bool empty(void) const;
		TrackPoints::iterator erase(TrackPoints::iterator first, TrackPoints::iterator last);
		void push_front(Trackpoint * tp);

		/* May return ::end(). */
	        Trackpoint * get_current_tp(void) const;


		void sort(compare_trackpoints_t compare_function);

		void add_trackpoint(Trackpoint * tp, bool recalculate);


		/* Get total length along a track in meters. */
		double get_length_value(void) const;
		Distance get_length(void) const;
		double get_length_value_including_gaps(void) const;
		Distance get_length_including_gaps(void) const;

		/* Get a length of a track up to a specified trackpoint (in meters). */
		double get_length_value_to_trackpoint(const Trackpoint * tp) const;
		Distance get_length_to_trackpoint(const Trackpoint * tp) const;


		/* Update the tree view's item of the track - primarily to update the icon. */
		sg_ret update_tree_item_properties(void) override;


		unsigned long get_tp_count() const;
		unsigned int get_segment_count() const;
		std::list<Track *> split_into_segments(void);

		Track * split_at_trackpoint(const TrackpointIter & tp);

		void smooth_it(bool flat);

		unsigned int merge_segments(void);
		void reverse(void);
		Time get_duration(bool include_segments) const;
		Time get_duration(void) const; /* private. */

		unsigned long get_dup_point_count(void) const;
		unsigned long remove_dup_points(void);
		unsigned long get_same_time_point_count(void) const;
		unsigned long remove_same_time_points(void);

		void to_routepoints();

		sg_ret calculate_max_speed(void);
		const Speed & get_max_speed(void) const;

		Speed get_average_speed(void) const;
		Speed get_average_speed_moving(int track_min_stop_length_seconds) const;

		void convert(CoordMode dest_mode);

		/**
		   @brief Get timestamps of first and last trackpoint

		   @return sg_ret::err if track has less than two
		   trackpoints or if first or last trackpoint doesn't
		   have a timestamp.
		*/
		sg_ret get_timestamps(Time & ts_first, Time & ts_last) const;

		bool get_total_elevation_gain(Altitude & delta_up, Altitude & down) const;
		Trackpoint * get_tp_by_dist(double meters_from_start, bool get_next_point, double *tp_metres_from_start);
		bool set_tp_by_percentage_dist(double reldist, double *meters_from_start, int index);
		bool set_tp_by_percentage_time(double reldist, time_t *seconds_from_start, int index);
		Trackpoint * get_tp_by_max_speed() const;
		Trackpoint * get_tp_by_max_alt() const;
		Trackpoint * get_tp_by_min_alt() const;
		Trackpoint * get_tp_first() const;
		Trackpoint * get_tp_last() const;
		Trackpoint * get_tp_prev(Trackpoint * tp) const;
		Trackpoint * get_tp(int idx) const;
		bool get_minmax_alt(Altitude & min_alt, Altitude & max_alt) const;

		TrackData make_track_data_altitude_over_distance(int compressed_n_points) const;
		TrackData make_track_data_gradient_over_distance(int compressed_n_points) const;
		TrackData make_track_data_speed_over_time(void) const;
		TrackData make_track_data_distance_over_time(void) const;
		TrackData make_track_data_altitude_over_time(void) const;
		TrackData make_track_data_speed_over_distance(void) const;

		void marshall(Pickle & pickle);
		static Track * unmarshall(Pickle & pickle);

		virtual QList<QStandardItem *> get_list_representation(const TreeItemListFormat & list_format);


		void recalculate_bbox(void);
		LatLonBBox get_bbox(void) const;

		sg_ret anonymize_times(void);
		void interpolate_times();
		void apply_dem_data_common(bool skip_existing_elevations);
		unsigned long apply_dem_data(bool skip_existing);
		void apply_dem_data_last_trackpoint();
		unsigned long smooth_missing_elevation_data(bool flat);

		void steal_and_append_trackpoints(Track * from);

		Coord * cut_back_to_double_point();

		static bool compare_timestamp(const Track & a, const Track & b);

		void set_properties_dialog(TrackPropertiesDialog * dialog);
		void update_properties_dialog(void);
		void clear_properties_dialog();

		void set_profile_dialog(TrackProfileDialog * dialog);
		void update_profile_dialog(void);
		void clear_profile_dialog();

		void export_track(const QString & title, const QString & default_file_name, SGFileType file_type);


		/**
		   @brief Return the percentage of how far a trackpoint is a long a track by distance

		   @return NAN on problems
		*/
		double get_tp_distance_percent(int idx) const;
		/**
		   @brief Return the percentage of how far a current trackpoint is along a track by time

		   @return NAN on problems
		*/
		double get_tp_time_percent(int idx) const;


		TrackPoints::iterator erase_trackpoint(TrackPoints::iterator iter);
		TrackPoints::iterator delete_trackpoint(TrackPoints::iterator iter);
		void insert(Trackpoint * tp_at, Trackpoint * tp_new, bool before);

		std::list<Rect *> * get_rectangles(LatLon * wh);
		//CoordMode get_coord_mode(void) const;

		bool add_context_menu_items(QMenu & menu, bool tree_view_context_menu);
		void sublayer_menu_track_route_misc(LayerTRW * parent_layer_, QMenu & menu, QMenu * upload_submenu);
		void sublayer_menu_track_misc(LayerTRW * parent_layer_, QMenu & menu, QMenu * upload_submenu);

		bool handle_selection_in_tree(void);

		void draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected);

		QString sublayer_rename_request(const QString & new_name);

		std::list<Rect *> * get_map_rectangles(const VikingZoomLevel & viking_zoom_level);

		void create_tp_next_to_reference_tp(TrackpointIter * reference_tp, bool before);

		void remove_last_trackpoint(void);

#ifdef VIK_CONFIG_GOOGLE
		bool is_valid_google_route();
#endif

		bool properties_dialog();

		LayerTRW * get_parent_layer_trw() const;

		/* Track editing tool. */
		TrackpointIter selected_tp_iter;

		/* QString name; */ /* Inherited from TreeItem. */
		QString comment;
		QString description;
		QString source;
		QString type;

		TrackPoints trackpoints;
		/* bool visible = true; */ /* Inherited from TreeItem. */
		TrackDrawNameMode draw_name_mode = TrackDrawNameMode::None;
		int max_number_dist_labels = 0;

		uint8_t ref_count = 0;
		bool has_color = false;
		QColor color;
		LatLonBBox bbox;

		TrackPropertiesDialog * props_dialog = NULL;
		TrackProfileDialog * profile_dialog = NULL;
		double track_length_including_gaps = 0.0;

	private:
		static void smoothie(TrackPoints::iterator start, TrackPoints::iterator stop, double elev1, double elev2, unsigned int points);
		void recalculate_bbox_last_tp();
		TrackData make_values_distance_over_time_helper(void) const;
		TrackData make_values_altitude_over_time_helper(void) const;

		Trackpoint * tps[2] = { NULL, NULL };

		Speed max_speed;

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
		void track_use_with_bfilter_cb(void);
#endif

		void split_by_timestamp_cb(void);
		void split_by_n_points_cb(void);
		void split_by_segments_cb(void);

		void refine_route_cb(void);

		void cut_sublayer_cb(void);
		void copy_sublayer_cb(void);
	};




} /* namespace SlavGPS */




Q_DECLARE_METATYPE(SlavGPS::Track*)




#endif /* #ifndef _SG_TRACK_INTERNAL_H_ */
