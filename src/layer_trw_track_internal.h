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




	class Track;
	class TrackpointReference;
	class Trackpoint;
	typedef bool (* compare_trackpoints_t)(const Trackpoint * a, const Trackpoint * b);


	class TrackPropertiesDialog;
	class TrackProfileDialog;
	class Graph2D;

	enum class SGFileType;
	enum class GisViewportDomain;

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

		Altitude altitude;      /* Invalid/unavailable by default. */
		double gps_speed = NAN; /* NAN if data unavailable. */
		Angle course;           /* Invalid if data unavailable. Invalid by default. */

		unsigned int nsats = 0;     /* Number of satellites used. 0 if data unavailable. */

		GPSFixMode fix_mode = GPSFixMode::NotSeen;     /* GPSFixMode::NotSeen if data unavailable. */
		double hdop = VIK_DEFAULT_DOP;                 /* VIK_DEFAULT_DOP if data unavailable. */
		double vdop = VIK_DEFAULT_DOP;                 /* VIK_DEFAULT_DOP if data unavailable. */
		double pdop = VIK_DEFAULT_DOP;                 /* VIK_DEFAULT_DOP if data unavailable. */
	};




	class TrackSelectedChildren {
		friend class Track;
	public:
		TrackSelectedChildren();

		/**
		   @return number of trackpoints in the track/route that are selected (may be zero)
		*/
		size_t get_count(void) const;

		/**
		   Is this @param tp selected (possibly as one of many
		   other trackpoints belonging to this track)?

		   @return false if number of selected children (trackpoints) is zero
		   @return true if the number is non-zero, and given @param tp is among selected items.
		*/
		bool is_member(const Trackpoint * tp) const;

		TrackpointReference front(void) const;

	private:
		/* For now it's only single-item container. There will
		   always be one item, but the item may be invalid if
		   no selections are made. */
		std::list<TrackpointReference> references;
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
		Track(bool is_route);
		Track(const Track & from); /* Only copy properties, don't move or copy trackpoints from source track. */
		~Track();

		void set_defaults();
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


		SGObjectTypeID get_type_id(void) const override;
		static SGObjectTypeID type_id(void);


		/* For now we only support no more than one selected
		   tp, so these two methods set or reset a single
		   trackpoint. */
		void selected_tp_set(const TrackpointReference & tp_ref);
		/**
		   @return true if a selected trackpoint was set before this function call
		   @return false otherwise
		*/
		bool selected_tp_reset(void);

		/**
		   @brief Deselect current trackpoint, select next trackpoint

		   If it's impossible to change selection, return
		   value other than sg_ret::ok.
		*/
		sg_ret move_selection_to_next_tp(void);

		/**
		   @brief Deselect current trackpoint, select previous trackpoint

		   If it's impossible to change selection, return
		   value other than sg_ret::ok.
		*/
		sg_ret move_selection_to_previous_tp(void);

		sg_ret delete_all_selected_tp(void);

		const TrackSelectedChildren & get_selected_children(void) const;




		bool is_selected(void) const;

		void smooth_it(bool flat);

		unsigned int merge_segments(void);
		void reverse(void);
		Duration get_duration(bool include_segments) const;
		Duration get_duration(void) const; /* private. */

		unsigned long get_dup_point_count(void) const;
		unsigned long remove_dup_points(void);
		unsigned long get_same_time_point_count(void) const;
		unsigned long remove_same_time_points(void);

		void to_routepoints();

		sg_ret calculate_max_speed(void);
		const Speed & get_max_speed(void) const;

		Speed get_average_speed(void) const;
		Speed get_average_speed_moving(const Duration & track_min_stop_duration) const;

		void change_coord_mode(CoordMode dest_mode);

		/**
		   @brief Get timestamps of first and last trackpoint

		   @return sg_ret::err if track has less than two
		   trackpoints or if first or last trackpoint doesn't
		   have a timestamp.
		*/
		sg_ret get_timestamps(Time & ts_first, Time & ts_last) const;

		bool get_total_elevation_gain(Altitude & delta_up, Altitude & down) const;
		Trackpoint * get_tp_by_dist(double meters_from_start, bool get_next_point, double *tp_metres_from_start);

		sg_ret select_tp(const Trackpoint * tp);

		Trackpoint * get_tp_by_max_speed() const;
		Trackpoint * get_tp_with_highest_altitude(void) const;
		Trackpoint * get_tp_with_lowest_altitude(void) const;
		Trackpoint * get_tp_first() const;
		Trackpoint * get_tp_last() const;
		Trackpoint * get_tp_prev(Trackpoint * tp) const;

		bool get_minmax_alt(Altitude & min_alt, Altitude & max_alt) const;

		void marshall(Pickle & pickle);
		static Track * unmarshall(Pickle & pickle);

		/**
		   @brief Common method for showing a list of tracks with extended information

		   @title: the title for the dialog
		   @layer: The layer, from which a list of tracks/routes should be extracted (may be Aggregate layer)
		   @wanted_types: type IDs of items to be shown in list (tracks and/or routes)

		   @param layer can be also Aggregate layer - the
		   function then goes through all child layers of the
		   Aggregate layer in search of @param wanted_types
		*/
		static void list_dialog(QString const & title, Layer * layer, const std::list<SGObjectTypeID> & wanted_types);

		QList<QStandardItem *> get_list_representation(const TreeItemViewFormat & view_format) override;


		void recalculate_bbox(void);
		LatLonBBox get_bbox(void) const;

		sg_ret anonymize_times(void);
		void interpolate_times();
		void apply_dem_data_common(bool skip_existing_elevations);
		unsigned long apply_dem_data(bool skip_existing);
		void apply_dem_data_last_trackpoint();
		unsigned long smooth_missing_elevation_data(bool flat);


		/* Move a subset (range) of trackpoints from track
		   @from, append them at the end of list of
		   trackpoints in this track. Recalculate bbox of
		   source and target tracks. */
		sg_ret move_trackpoints_from(Track & from, const TrackPoints::iterator & from_begin, const TrackPoints::iterator & from_end);

		/* Make a deep copy of a subset (range) of trackpoints
		   from other track, append them at the end of list of
		   trackpoints in this track. Recalculate bbox of this
		   tracks. */
		sg_ret copy_trackpoints_from(const TrackPoints::iterator & from_begin, const TrackPoints::iterator & from_end);


		Coord * cut_back_to_double_point();

		static bool compare_timestamp(const Track & a, const Track & b);

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

		std::list<CoordRectangle> get_coordinate_rectangles(const LatLon & single_rectangle_span);
		//CoordMode get_coord_mode(void) const;

		sg_ret menu_add_type_specific_operations(QMenu & menu, bool in_tree_view) override;
		sg_ret menu_add_standard_operations(QMenu & menu, const StandardMenuOperations & ops, bool in_tree_view) override;

		void sublayer_menu_track_route_misc(LayerTRW * parent_layer_, QMenu & menu, QMenu * upload_submenu);
		void sublayer_menu_track_misc(LayerTRW * parent_layer_, QMenu & menu, QMenu * upload_submenu);

		bool handle_selection_in_tree(void);

		void draw_tree_item(GisViewport * gisview, bool highlight_selected, bool parent_is_selected);

		QString sublayer_rename_request(const QString & new_name);

		std::list<CoordRectangle> get_coord_rectangles(const VikingScale & viking_scale);

		sg_ret create_tp_next_to_selected_tp(bool before);

		void remove_last_trackpoint(void);

		void prepare_for_profile(void);

#ifdef VIK_CONFIG_GOOGLE
		bool is_valid_google_route();
#endif

		bool is_track(void) const;
		bool is_route(void) const;

		bool show_properties_dialog(void) override;


		/**
		   @brief Get GPS Speed from this specific trackpoint

		   Returns invalid Speed object if given trackpoint
		   doesn't have GPS Speed set.
		 */
		static Speed get_gps_speed(const Trackpoint * tp);

		/**
		   @brief Get speed calculated using distance to
		   previous tp and time difference to previous tp

		   Returns invalid Speed object if given trackpoint
		   @param tp is first in track (there is no previous
		   trackpoint that could be used in calculations) or
		   if other data necessary for calculations is not
		   present.
		*/
		static Speed get_diff_speed(const Trackpoint * tp, const Trackpoint * tp_prev);

		/**
		   @brief Get time difference between two trackpoints

		   Returns invalid Duration object if given trackpoint
		   @param tp is first in track (there is no previous
		   trackpoint that could be used in calculations) or
		   if other data necessary for calculations is not
		   present.
		*/
		static Duration get_diff_time(const Trackpoint * tp, const Trackpoint * tp_prev);


		sg_ret tp_properties_dialog_set(void);
		static sg_ret tp_properties_dialog_reset(void);

		/**
		   @return sg_ret::ok if there is one and only one selected tp, and its coordinate has been set
		   @return sg_ret::err otherwise
		*/
		sg_ret selected_tp_set_coord(const Coord & new_coord, bool do_recalculate_bbox);

		/**
		   @brief Simple accessor
		*/
		LayerTRW * get_parent_layer_trw(void) const;

		static TreeItemViewFormat get_view_format_header(bool include_parent_layer);


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
		static void smoothie(TrackPoints::iterator start, TrackPoints::iterator stop, const Altitude & elev1, const Altitude & elev2, unsigned int points);
		void recalculate_bbox_last_tp();

		/* This method is private to make sure that only this
		   track can pass a trackpoint iter. This gives us
		   more certainty that given iter belongs to the
		   track. */
		sg_ret create_tp_next_to_specified_tp(const TrackpointReference & other_tp_ref, bool before);

		void copy_properties(const Track & from);



		/**
		   @brief Split track at given trackpoint

		   @param tp_ref - trackpoint reference

		   @return sg_ret::ok if split has been performed
		   @return other value on errors or if split can't be performed
		*/
		sg_ret split_at_trackpoint(const TrackpointReference & tp_ref);

		/* Split track at iterators. @iterators contains list
		   of iterators to track's trackpoints container. Each
		   range of iterators (with exception of first one)
		   will be used to fill trackpoints container of new
		   tracks.

		   First element on @iterators should be
		   trackpoints::begin().  Last element on @iterators
		   should be trackpoints::end().

		   The split will be performed only when there are
		   more than two (trackpoints::begin() and
		   trackpoints::end()) iterators. */
		sg_ret split_at_iterators(std::list<TrackPoints::iterator> & iterators, LayerTRW * parent_layer);

		Speed max_speed;

		TrackSelectedChildren selected_children;

	public slots:
		void goto_startpoint_cb(void);
		void goto_center_cb(void);
		void goto_endpoint_cb(void);
		void goto_max_speed_cb(void);
		void goto_max_alt_cb(void);
		void goto_min_alt_cb(void);

		void anonymize_times_cb(void);
		void interpolate_times_cb(void);

		bool show_properties_dialog_cb(void);
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

		/* Select a track to be used with gpsbabel filter. */
		void use_with_babel_filter_cb(void);

		void split_by_timestamp_cb(void);
		void split_by_n_points_cb(void);
		void split_by_segments_cb(void);
		sg_ret split_at_selected_trackpoint_cb(void);

		void refine_route_cb(void);

		sg_ret cut_tree_item_cb(void) override;
		sg_ret copy_tree_item_cb(void) override;
		sg_ret delete_tree_item_cb(void) override;

		void insert_point_after_cb(void);
		void insert_point_before_cb(void);

		void delete_all_selected_tp_cb(void);
		void delete_points_same_position_cb(void);
		void delete_points_same_time_cb(void);

		void extend_track_end_cb(void);
		void extend_track_end_route_finder_cb(void);
	};




	class Route {
	public:
		//SGObjectTypeID get_type_id(void) const override;
		static SGObjectTypeID type_id(void);
	};




} /* namespace SlavGPS */




Q_DECLARE_METATYPE(SlavGPS::Track*)




#endif /* #ifndef _SG_TRACK_INTERNAL_H_ */
