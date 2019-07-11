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

#ifndef _SG_WINDOW_H_
#define _SG_WINDOW_H_




#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QTreeView>
#include <QMouseEvent>
#include <QCursor>
#include <QCloseEvent>
#include <QUrl>
#include <QComboBox>




#include "measurements.h"
#include "viewport.h"
#include "layer_trw_track.h"
#include "layer_trw_waypoints.h"
#include "layer_trw_tracks.h"
#include "file.h"




#define VIK_SETTINGS_WIN_MAX                     "window_maximized"
#define VIK_SETTINGS_WIN_FULLSCREEN              "window_fullscreen"
#define VIK_SETTINGS_WIN_WIDTH                   "window_width"
#define VIK_SETTINGS_WIN_HEIGHT                  "window_height"
#define VIK_SETTINGS_WIN_MAIN_DOCK_SIZE          "window_horizontal_pane_position"
#define VIK_SETTINGS_WIN_COPY_CENTRE_FULL_FORMAT "window_copy_centre_full_format"




namespace SlavGPS {




	QComboBox * create_zoom_combo_all_levels(QWidget * parent);




	enum class SGFileType;
	class VikingScale;
	class Layer;
	class LayerTRW;
	class Toolbox;
	class Viewport2D;
	class LayersPanel;
	class ScreenPos;
	class DataSource;
	class StatusBar;
	enum class LayerType;
	enum class StatusBarField;




	class Window : public QMainWindow {
		Q_OBJECT

	friend class Toolbox;

	public:
		Window();
		~Window();
		static Window * new_window();

		void draw_tree_items();
		void draw_sync();

		void update_status_bar_on_redraw();

		void handle_selection_of_tree_item(const TreeItem & tree_item);

		GisViewport * get_viewport(void);
		LayersPanel * get_items_tree(void);
		QMenu * get_layer_menu(QMenu * menu);
		QMenu * new_layers_submenu_add_actions(QMenu * menu);
		Toolbox * get_toolbox(void);
		StatusBar * get_statusbar(void);

		LayersPanel * items_tree = NULL;
		Viewport2D * viewport = NULL;


		void statusbar_update(StatusBarField field, QString const & message);

		bool save_on_dirty_flag(void);
		void set_dirty_flag(bool dirty);


		void pan_click(QMouseEvent * event);
		void pan_move(QMouseEvent * event);
		void pan_release(QMouseEvent * event);
		void pan_off(void);
		bool get_pan_move_in_progress(void) const;
		sg_ret pan_move_update_viewport(const QMouseEvent * ev);

		char type_string[30];

		void closeEvent(QCloseEvent * event);
		void keyPressEvent(QKeyEvent * event);


		QAction * get_drawmode_action(GisViewportDrawMode mode);


		void simple_map_update(bool only_new);

		bool export_to(const std::list<const Layer *> & layers, SGFileType file_type, const QString & full_dir_path, char const *extension);
		void export_to_common(SGFileType file_type, char const * extension);


		/* Return indicates if a redraw is necessary. */
		bool clear_highlight();

		void finish_new(void);


		/* Set full path to current document. */
		void set_current_document_full_path(const QString & document_full_path);
		/* Get full path to current document. */
		QString get_current_document_full_path(void);
		/* Get last part of path to current document, i.e. only file name. */
		QString get_current_document_file_name(void);


		void set_redraw_trigger(TreeItem * tree_item);

		void activate_tool_by_id(const QString & tool_id);

		void open_file(const QString & new_document_full_path, bool set_as_current_document);

		void set_busy_cursor(void);
		void clear_busy_cursor(void);


		bool save_current_document();
		void apply_new_preferences(void);

		bool get_side_panel_visibility(void) const;

		void emit_center_coord_or_zoom_changed(const QString & trigger_name);


		QAction * qa_tree_item_properties = NULL;

	public slots:
		Window * new_window_cb(void);

		void menu_layer_new_cb(void);
		void draw_layer_cb(sg_uid_t uid);
		void draw_tree_items_cb(void);


		void goto_default_location_cb(void);
		void goto_location_cb(void);
		void goto_latlon_cb(void);
		void goto_utm_cb(void);
		void goto_previous_location_cb(void);
		void goto_next_location_cb(void);


		void set_full_screen_state_cb(bool new_state);

		void set_side_panel_visibility_cb(bool new_state);
		void set_status_bar_visibility_cb(bool new_state);
		void set_main_menu_visibility_cb(bool new_state);

		void set_highlight_usage_cb(bool new_state);
		void set_scale_visibility_cb(bool new_state);
		void set_center_mark_visibility_cb(bool new_state);


		void zoom_cb(void);
		void zoom_to_cb(void);
		void zoom_level_selected_cb(QAction * qa);


		void menu_view_refresh_cb(void);
		void menu_view_set_highlight_color_cb(void);
		void menu_view_set_bg_color_cb(void);
		void menu_view_pan_cb(void);


		void show_background_jobs_window_cb(void);

		void show_layer_defaults_cb(void);

		void show_centers_cb();



		/* General handler of acquire requests.
		   Called by all acquire_from_X_cb() with specific data source. */
		void acquire_handler(DataSource * data_source);

		void acquire_from_gps_cb(void);
		void acquire_from_file_cb(void);
		void acquire_from_geojson_cb(void);
		void acquire_from_routing_cb(void);
		void acquire_from_osm_cb(void);
		void acquire_from_my_osm_cb(void);

#ifdef VIK_CONFIG_GEOCACHES
		void acquire_from_gc_cb(void);
#endif

#ifdef VIK_CONFIG_GEOTAG
		void acquire_from_geotag_cb(void);
#endif

#ifdef VIK_CONFIG_GEONAMES
		void acquire_from_wikipedia_cb(void);
#endif

		void acquire_from_url_cb(void);


		void draw_viewport_to_image_file_cb(void);
		void draw_viewport_to_image_dir_cb(void);
#ifdef HAVE_ZIP_H
		void draw_viewport_to_kmz_file_cb(void);
#endif
		void import_kmz_file_cb(void);

		void print_cb(void);


		void change_coord_mode_cb(QAction * qa);


		void draw_click_cb(QMouseEvent * ev);
		void draw_release_cb(QMouseEvent * ev);


		void export_to_gpx_cb(void);
		void export_to_kml_cb(void);


		void menu_view_cache_info_cb(void);


	protected:

	private slots:
		void center_changed_cb(void);
		void layer_tool_cb(QAction * a);

		void destroy_window_cb(void);


		void menu_edit_cut_cb(void);
		void menu_edit_copy_cb(void);
		void menu_edit_paste_cb(void);
		void menu_edit_delete_cb(void);
		void menu_edit_delete_all_cb(void);
		void menu_copy_centre_cb(void);
		void map_cache_flush_cb(void);
		void set_default_location_cb(void);
		void preferences_cb(void);
		void open_file_cb(void);
		void open_recent_file_cb(void);

		void menu_file_properties_cb(void);
		bool menu_file_save_cb(void);
		bool menu_file_save_as_cb(void);
		bool menu_file_save_and_exit_cb(void);

		void help_help_cb(void);
		void help_about_cb(void);

	private:

		void create_layout(void);
		void create_actions(void);
		void create_ui(void);

		void display_tool_name();

		/**
		   Function used to update "recent files" entry in
		   File menu.  Use only for files that Viking knows
		   how to handle without additional context.  .vik and
		   .kml files are such files. Files with other types
		   (e.g. generic xml file) probably are not.
		*/
		void update_recent_files(QString const & path, const QString & mime_type);

		void open_window(const QStringList & file_full_paths);

		bool pan_move_in_progress = false;
		bool single_click_pending = false;
		/* Coordinates of these screen positions should be in
		   Qt's coordinate system, where beginning (pixel 0,0)
		   is in upper-left corner. */
		ScreenPos pan_pos; /* Last recorded position of cursor while panning. */
		ScreenPos delayed_pan_pos; /* Temporary storage. */


		QMenuBar * menu_bar = NULL;
		QToolBar * toolbar = NULL;
		QDockWidget * panel_dock = NULL;
		StatusBar * status_bar = NULL;
		Toolbox * toolbox = NULL;

		QMenu * menu_file = NULL;
		QMenu * menu_edit = NULL;
		QMenu * menu_view = NULL;
		QMenu * menu_layers = NULL;
		QMenu * menu_tools = NULL;
		QMenu * menu_help = NULL;

		QMenu * submenu_recent_files = NULL;
		QMenu * submenu_file_acquire = NULL;

		/* Half-drawn update. */
		TreeItem * redraw_trigger = NULL;
		Coord trigger_center;

		QString current_document_full_path;

		LoadStatus file_load_status = LoadStatus::Code::ReadFailure; /* AKA none. */

		/* Tool management state. */
		LayerType tool_layer_type;
		uint16_t tool_tool_id;

		/* Cursor of window's central widget. */
		QCursor viewport_cursor;


		bool only_updating_coord_mode_ui = false; /* Hack for a bug in GTK. */

		QAction * qa_view_full_screen_state = NULL;
		QAction * qa_view_scale_visibility = NULL;
		QAction * qa_view_center_mark_visibility = NULL;
		QAction * qa_view_highlight_usage = NULL;

		QAction * qa_drawmode_expedia = NULL;
		QAction * qa_drawmode_mercator = NULL;
		QAction * qa_drawmode_latlon = NULL;
		QAction * qa_drawmode_utm = NULL;

		QAction * qa_previous_location = NULL;
		QAction * qa_next_location = NULL;

		std::list<QString> recent_files;

		/* Flag set when contents of project is modified. This
		   flag is set only in a handful of situations. Adding
		   new waypoint or track doesn't set this flag, so its
		   usefulness is questionable. */
		bool dirty_flag = false;

	signals:
		void center_coord_or_zoom_changed(void);
	};




	class ThisApp {
	public:
		void set(Window * new_window, LayersPanel * new_layers_panel, GisViewport * new_gisview)
		{
			this->window = new_window;
			this->layers_panel = new_layers_panel;
			this->gisview = new_gisview;
		}

		static Window * get_main_window(void);
		static LayersPanel * get_layers_panel(void);
		static GisViewport * get_main_viewport(void);

	private:
		Window * window = NULL;
		LayersPanel * layers_panel = NULL;
		GisViewport * gisview = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WINDOW_H_ */
