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
 */

#ifndef _SG_LAYER_TRW_H_
#define _SG_LAYER_TRW_H_




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdint>
#include <list>
#include <unordered_map>

#include <QStandardItem>
#include <QDialog>

#include "layer.h"
#include "layer_tool.h"
#include "layer_interface.h"
#include "layer_trw_dialogs.h"
#include "layer_trw_tracks.h"
#include "layer_trw_waypoints.h"
#include "viewport.h"
#include "waypoint.h"
#include "trackpoint_properties.h"
#include "file.h"
#include "track.h"




namespace SlavGPS {




	class LayerTRW;
	class LayersPanel;
	class Track;
	class TrackpointSearch;
	class WaypointSearch;




	struct _VikDataSourceInterface;
	typedef struct _VikDataSourceInterface VikDataSourceInterface;


	/* To be removed. */
	typedef int GdkFunction;
	typedef int PangoLayout;


	class TRWMetadata {

	public:
		void set_author(const QString & new_author);
		void set_description(const QString & new_description);
		void set_keywords(const QString & new_keywords);
		void set_timestamp(const QString & new_timestamp);

		QString description;
		QString author;
		QString keywords;  /* TODO: handling/storing a GList of individual tags? */
		/* TODO: verify how the timestamp should work: when and how it should be set. */
		bool has_timestamp;
		QString timestamp; /* TODO: Consider storing as proper time_t. */
	};





	/* See http://developer.gnome.org/pango/stable/PangoMarkupFormat.html */
	typedef enum {
		FS_XX_SMALL = 0,
		FS_X_SMALL,
		FS_SMALL,
		FS_MEDIUM, // DEFAULT
		FS_LARGE,
		FS_X_LARGE,
		FS_XX_LARGE,
		FS_NUM_SIZES
	} font_size_t;





	/* For creating a list of tracks with the corresponding layer it is in
	   (thus a selection of tracks may be from differing layers). */
	class track_layer_t {
	public:
		Track * trk = NULL;
		LayerTRW * trw = NULL;
	};

	/* For creating a list of waypoints with the corresponding layer it is in
	   (thus a selection of waypoints may be from differing layers). */
	class waypoint_layer_t {
	public:
		Waypoint * wp = NULL;
		LayerTRW * trw = NULL;
	};




	class LayerTRWInterface : public LayerInterface {
	public:
		LayerTRWInterface();
		Layer * unmarshall(uint8_t * data, size_t data_len, Viewport * viewport);
		void change_param(GtkWidget * widget, ui_change_values * values);
		LayerToolContainer * create_tools(Window * window, Viewport * viewport);
	};




	class LayerTRW : public Layer {
		Q_OBJECT
	public:
		LayerTRW();
		~LayerTRW();


		/* Layer interface methods. */
		void post_read(Viewport * viewport, bool from_file);
		QString get_tooltip();


		/* Methods for generic "Select" tool. */
		bool select_click(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		bool select_move(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		bool select_release(QMouseEvent * event, Viewport * viewport, LayerTool * tool);
		bool select_tool_context_menu(QMouseEvent * event, Viewport * viewport);

		void select_click_do_track_selection(QMouseEvent * ev, LayerTool * tool, Track * track, TrackPoints::iterator & tp_iter);
		void select_click_do_waypoint_selection(QMouseEvent * ev, LayerTool * tool, Waypoint * wp);

		void set_menu_selection(LayerMenuItem selection);
		LayerMenuItem get_menu_selection();

		void marshall(uint8_t ** data, size_t * data_len);

		void cut_sublayer(TreeItem * item);
		void copy_sublayer(TreeItem * item, uint8_t ** data, unsigned int * len);
		bool paste_sublayer(TreeItem * item, uint8_t * data, size_t len);
		void delete_sublayer(TreeItem * item);

		void change_coord_mode(CoordMode dest_mode);

		time_t get_timestamp();

		void drag_drop_request(Layer * src, TreeIndex & src_item_index, void * GtkTreePath_dest_path);

		int read_file(FILE * f, char const * dirpath);
		void write_file(FILE * f) const;

		void add_menu_items(QMenu & menu);

		void add_children_to_tree(void);
		bool set_param_value(uint16_t id, const SGVariant & param_value, bool is_file_operation);
		SGVariant get_param_value(param_id_t id, bool is_file_operation) const;





		bool create_new_tracks(Track * orig, std::list<TrackPoints *> * tracks_data);

		void add_track(Track * trk);
		void add_route(Track * trk);
		void add_waypoint(Waypoint * wp);

		Tracks & get_track_items();
		Tracks & get_route_items();
		Waypoints & get_waypoint_items();

		LayerTRWTracks & get_tracks_node(void);
		LayerTRWTracks & get_routes_node(void);
		LayerTRWWaypoints & get_waypoints_node(void);

		LayerTRWTracks * tracks = NULL; /* Sub-node, under which all layer's tracks are shown. */
		LayerTRWTracks * routes = NULL; /* Sub-node, under which all layer's routes are shown. */
		LayerTRWWaypoints * waypoints = NULL; /* Sub-node, under which all layer's waypoints are shown. */


		bool get_tracks_visibility();
		bool get_routes_visibility();
		bool get_waypoints_visibility();

		bool is_empty();


		bool find_track_by_date(char const * date, Viewport * viewport, bool select);
		bool find_waypoint_by_date(char const * date, Viewport * viewport, bool select);

		/* Draw all items of the layer, without highlight. */
		void draw(Viewport * viewport);

		/* Draw all items of the layer, with highlight. */
		void draw_tree_item(Viewport * viewport, bool hl_is_allowed, bool hl_is_required);


		void find_maxmin(struct LatLon maxmin[2]);
		bool find_center(Coord * dest);

		void set_statusbar_msg_info_tp(TrackPoints::iterator & tp_iter, Track * track);
		void set_statusbar_msg_info_wpt(Waypoint * wp);

		void zoom_to_show_latlons(Viewport * viewport, struct LatLon maxmin[2]);
		bool auto_set_view(Viewport * viewport);



		bool new_waypoint(Window * parent, const Coord * def_coord);
		Track * new_track_create_common(const QString & new_name);
		Track * new_route_create_common(const QString & new_name);




		void cancel_tps_of_track(Track * trk);

		void reset_waypoints();

		QString new_unique_element_name(const QString & type_id, const QString& old_name);


		/* These are meant for use in file loaders (gpspoint.c, gpx.c, etc).
		 * These copy the name, so you should free it if necessary. */
		void filein_add_waypoint(Waypoint * wp, const QString & wp_name);
		void filein_add_track(Track * trk, const QString & trk_name);




		void move_item(LayerTRW * vtl_dest, sg_uid_t sublayer_uid, const QString & type_id);



		bool delete_track(Track * trk);
		bool delete_track_by_name(const QString & trk_name, bool is_route);
		bool delete_route(Track * trk);
		bool delete_waypoint(Waypoint * wp);
		bool delete_waypoint_by_name(const QString & wp_name);


		void delete_all_routes();
		void delete_all_tracks();
		void delete_all_waypoints();


		void smooth_it(Track * trk, bool flat);
		void wp_changed_message(int changed);



		void delete_selected_tp(Track * trk);

		void diary_open(char const * date_str);
		void astro_open(char const * date_str,  char const * time_str, char const * lat_str, char const * lon_str, char const * alt_str);



		std::list<waypoint_layer_t *> * create_waypoints_and_layers_list();
		std::list<waypoint_layer_t *> * create_waypoints_and_layers_list_helper(std::list<Waypoint *> * waypoints);


		std::list<track_layer_t *> * create_tracks_and_layers_list();
		std::list<track_layer_t *> * create_tracks_and_layers_list(const QString & type_id);
		std::list<track_layer_t *> * create_tracks_and_layers_list_helper(std::list<Track *> * tracks);


		void trackpoint_properties_show();



		Trackpoint * closest_tp_in_five_pixel_interval(Viewport * viewport, int x, int y);
		Waypoint * closest_wp_in_five_pixel_interval(Viewport * viewport, int x, int y);


		void sort_all();



		void set_coord_mode(CoordMode mode);
		CoordMode get_coord_mode();

		bool uniquify(LayersPanel * panel);

		void goto_coord(Viewport * viewport, const Coord & coord);

		void highest_wp_number_reset();
		void highest_wp_number_add_wp(const QString & new_wp_name);
		void highest_wp_number_remove_wp(const QString & old_wp_name);
		QString highest_wp_number_get();


		void tpwin_update_dialog_data();


		void dialog_shift(QDialog * dialog, Coord * coord, bool vertical);


		int32_t get_property_track_thickness();

		static TRWMetadata * metadata_new();
		static void metadata_free(TRWMetadata * metadata);
		TRWMetadata * get_metadata();
		void set_metadata(TRWMetadata * metadata);

		/* Intended only for use by other trw_layer subwindows. */
		void verify_thumbnails(void);



		/* This should be private. */
		void image_cache_free();
		void new_track_pens(void);
		void cancel_current_tp(bool destroy);
		void tpwin_response(int response);
		void update_statusbar();
		ToolStatus tool_new_track_or_route_click(QMouseEvent * event, Viewport * viewport);


		Track * get_edited_track(void);
		void set_edited_track(Track * track, const TrackPoints::iterator & tp_iter);
		void set_edited_track(Track * track);
		void reset_edited_track(void);

		Waypoint * get_edited_wp(void);
		void set_edited_wp(Waypoint * wp);
		void reset_edited_wp(void);


		void delete_sublayer_common(TreeItem * item, bool confirm);
		void copy_sublayer_common(TreeItem * item);
		void cut_sublayer_common(TreeItem * item, bool confirm);

		bool handle_selection_in_tree();

		void reset_internal_selections(void);

		/* Export. */
		void export_layer(const QString & title, const QString & default_name, Track * trk, SGFileType file_type);
		void open_layer_with_external_program(const QString & external_program);
		int export_layer_with_gpsbabel(const QString & title, const QString & default_name);


		/* Common for LayerTRW and for Tracks. */
		void upload_to_gps(TreeItem * sublayer);


		/* Track or Route that user currently operates.
		   Reference to an object already existing in ::tracks or ::routes. */
		Track * current_track = NULL;

		/* Waypoint that user currently operates on.
		   Reference to an object already existing in ::waypoints. */
		Waypoint * current_wp = NULL;

		bool moving_wp = false;
		bool waypoint_rightclick = false;


		PropertiesDialogTP * tpwin = NULL;

		/* Track editing tool -- more specifically, moving tps. */
		bool moving_tp = false;

		CoordMode coord_mode;

		int highest_wp_number = 0;



		bool track_draw_labels;

		int track_drawing_mode; /* Mostly about how a color(s) for track is/are selected, but in future perhaps other attributes will be variable as well. */
	        bool draw_trackpoints;
		int32_t trackpoint_size;
		uint8_t drawelevation;
		int32_t elevation_factor;
		bool draw_track_stops;
		int32_t stop_length;
		bool draw_track_lines;
		uint8_t drawdirections;
		int32_t drawdirections_size;
		int32_t track_thickness;
		int32_t track_bg_thickness; /* Thickness of a line drawn in background of main line representing track. */
		sort_order_t track_sort_order;

		TRWMetadata * metadata = NULL;

		font_size_t trk_label_font_size; /* Font size of track's label, in Pango's "absolute size" units. */

		int wp_marker_type;
		int32_t wp_marker_size; /* In Variant data type this field is stored as uint8_t. */
		bool wp_draw_symbols;
		font_size_t wp_label_font_size; /* Font size of waypoint's label, in Pango's "absolute size" units. */
		sort_order_t wp_sort_order;


		double track_draw_speed_factor;
		std::vector<QPen> track_pens;
		QColor track_color_common; /* Used when layer's properties indicate that all tracks are drawn with the same color. */
		QPen current_trk_pen;
		/* Separate pen for a track's potential new point as drawn via separate method
		   (compared to the actual track points drawn in the main trw_layer_draw_track function). */
		QPen current_trk_new_point_pen;

		QPen track_bg_pen;
		QColor track_bg_color;

		QPen wp_marker_pen;
		QColor wp_marker_color;

		QPen wp_label_fg_pen;
		QColor wp_label_fg_color;

		QPen wp_label_bg_pen;
		QColor wp_label_bg_color;

		GdkFunction wpbgand;


		uint16_t ct_x1;
		uint16_t ct_y1;
		uint16_t ct_x2;
		uint16_t ct_y2;
		bool draw_sync_done;
		bool draw_sync_do;

		/* Route finder tool. */
		bool route_finder_started;
		bool route_finder_check_added_track;
		Track * route_finder_added_track = NULL;
		bool route_finder_append;


		bool drawlabels;
		bool drawimages;
		int32_t wp_image_alpha;
		GQueue * image_cache = NULL;
		int32_t wp_image_size;
		int32_t wp_image_cache_size;


		bool has_verified_thumbnails;

		/* Menu. */
		LayerMenuItem menu_selection;


		bool clear_highlight();


	public slots:
		void trackpoint_properties_cb(int response);



		/* Context menu callbacks. */

		void finish_track_cb(void);

		void full_view_cb(void);

		void centerize_cb(void);
		void find_waypoint_dialog_cb(void);    /* In context menu for "Layer TRW" or "Waypoints" node in tree view. */

		void export_as_gpspoint_cb(void);
		void export_as_gpsmapper_cb(void);
		void export_as_gpx_cb(void);
		void export_as_kml_cb(void);
		void export_as_geojson_cb(void);
		void export_via_babel_cb(void);
		void open_with_external_gpx_1_cb(void);
		void open_with_external_gpx_2_cb(void);

		void new_waypoint_cb(void);
		void new_track_cb(void);
		void new_route_cb(void);

#ifdef VIK_CONFIG_GEOTAG
		void geotag_images_cb(void);
#endif

		void acquire_from_gps_cb(void);
		void acquire_from_routing_cb(void);
#ifdef VIK_CONFIG_OPENSTREETMAP
		void acquire_from_osm_cb(void);
		void acquire_from_osm_my_traces_cb(void);
#endif
		void acquire_from_url_cb(void);

		void acquire_from_wikipedia_waypoints_viewport_cb(void);
		void acquire_from_wikipedia_waypoints_layer_cb(void);

#ifdef VIK_CONFIG_GEOCACHES
		void acquire_from_geocache_cb(void);
#endif
#ifdef VIK_CONFIG_GEOTAG
		void acquire_from_geotagged_images_cb(void);
#endif
		void acquire_from_file_cb();

		void upload_to_gps_cb(void);
		void upload_to_osm_traces_cb(void);

		void delete_all_tracks_cb(void);
		void delete_selected_tracks_cb(void);
		void delete_all_routes_cb();
		void delete_selected_routes_cb();
		void delete_all_waypoints_cb(void);
		void delete_selected_waypoints_cb(void);

		void waypoint_list_dialog_cb(void);


		void tracks_stats_cb(void);

		void show_picture_cb(void);

		void merge_by_segment_cb(void);
		void merge_by_timestamp_cb(void);
		void merge_with_other_cb(void);
		void append_track_cb(void);
		void split_at_trackpoint_cb(void);
		void delete_point_selected_cb(void);
		void delete_points_same_position_cb(void);
		void delete_points_same_time_cb(void);
		void download_map_along_track_cb(void);
		void edit_trackpoint_cb(void);
		void track_list_dialog_cb(void);
		void extend_track_end_cb(void);
		void extend_track_end_route_finder_cb(void);

		void append_other_cb(void);
		void insert_point_after_cb(void);
		void insert_point_before_cb(void);
		void routes_stats_cb();

	public:
		void acquire(VikDataSourceInterface *datasource);
	};




	void layer_trw_init(void);
	char * convert_to_dms(double dec);




} /* namespace SlavGPS */




int sort_alphabetically(gconstpointer a, gconstpointer b, void * user_data);
int check_tracks_for_same_name(gconstpointer aa, gconstpointer bb, void * udata);
bool is_valid_geocache_name(const char *str);




#define VIK_SETTINGS_LIST_DATE_FORMAT "list_date_format"




#endif /* #ifndef _SG_LAYER_TRW_H_ */
