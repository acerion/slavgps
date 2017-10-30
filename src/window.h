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

#include "statusbar.h"
#include "file.h"
#include "acquire.h"
#include "viewport.h"
#include "viewport_utils.h"
#include "layer_trw_track.h"
#include "layer_trw_waypoints.h"
#include "layer_trw_tracks.h"




#define VIK_SETTINGS_WIN_MAX                     "window_maximized"
#define VIK_SETTINGS_WIN_FULLSCREEN              "window_fullscreen"
#define VIK_SETTINGS_WIN_WIDTH                   "window_width"
#define VIK_SETTINGS_WIN_HEIGHT                  "window_height"
#define VIK_SETTINGS_WIN_PANE_POSITION           "window_horizontal_pane_position"
#define VIK_SETTINGS_WIN_SAVE_IMAGE_WIDTH        "window_save_image_width"
#define VIK_SETTINGS_WIN_SAVE_IMAGE_HEIGHT       "window_save_image_height"
#define VIK_SETTINGS_WIN_SAVE_IMAGE_PNG          "window_save_image_as_png"
#define VIK_SETTINGS_WIN_COPY_CENTRE_FULL_FORMAT "window_copy_centre_full_format"




typedef int GtkWidget;
typedef int GdkDragContext;
typedef int GtkSelectionData;




namespace SlavGPS {




	QComboBox * create_zoom_combo_all_levels(QWidget * parent);




	class Layer;
	class LayerTRW;
	class Toolbox;
	class Viewport;
	class LayersPanel;




	struct _VikDataSourceInterface;
	typedef struct _VikDataSourceInterface VikDataSourceInterface;



	class Window : public QMainWindow {
		Q_OBJECT

	friend class Toolbox;

	public:
		Window();
		~Window();
		static Window * new_window();

		void draw_sync();
		void draw_status();
		void draw_redraw();
		void draw_update(void);

		void handle_selection_of_layer(Layer * layer);

		Viewport * get_viewport(void);
		LayersPanel * get_items_tree(void);
		QMenu * get_layer_menu(QMenu * menu);
		QMenu * new_layers_submenu_add_actions(QMenu * menu);
		Toolbox * get_toolbox(void);
		StatusBar * get_statusbar(void);

		LayersPanel * items_tree = NULL;
		Viewport * viewport = NULL;

		/* Flag set when contents of project is modified. This
		   flag is set only in a handful of situations. Adding
		   new waypoint or track doesn't set this flag, so its
		   usefulness is questionable. */
		bool contents_modified = false;

		void statusbar_update(StatusBarField field, QString const & message);


		void pan_click(QMouseEvent * event);
		void pan_move(QMouseEvent * event);
		void pan_release(QMouseEvent * event);
		void pan_off(void);
		bool get_pan_move(void);

		char type_string[30];

		void closeEvent(QCloseEvent * event);


		QAction * get_drawmode_action(ViewportDrawMode mode);


		void simple_map_update(bool only_new);

		bool export_to(std::list<const Layer *> * layers, SGFileType file_type, const QString & full_dir_path, char const *extension);
		void export_to_common(SGFileType file_type, char const * extension);

		void import_kmz_file_cb(void);


		/* Return indicates if a redraw is necessary. */
		bool clear_highlight();

		void finish_new(void);

		QString save_viewport_get_full_path(ViewportSaveMode mode);
		void save_viewport_to_image(const QString & file_full_path, unsigned int w, unsigned int h, double zoom, bool save_as_png, bool save_kmz);
		bool save_viewport_to_dir(const QString & dir_full_path, unsigned int w, unsigned int h, double zoom, bool save_as_png, unsigned int tiles_w, unsigned int tiles_h);

		/* Set full path to current document. */
		void set_current_document_full_path(const QString & document_full_path);
		/* Get full path to current document. */
		QString get_current_document_full_path(void);
		/* Get last part of path to current document, i.e. only file name. */
		QString get_current_document_file_name(void);


		static void set_redraw_trigger(Layer * layer);

		void activate_tool(const QString & tool_id);

		void open_file(const QString & new_document_full_path, bool set_as_current_document);

		void set_busy_cursor(void);
		void clear_busy_cursor(void);


		bool save_current_document();
		void apply_new_preferences(void);


		QAction * qa_tree_item_properties = NULL;


		unsigned int draw_image_width;
		unsigned int draw_image_height;
		bool save_viewport_as_png = false;

	public slots:
		Window * new_window_cb(void);

		void menu_layer_new_cb(void);
		void draw_layer_cb(sg_uid_t uid);
		void draw_update_cb(void);


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

		void my_acquire(VikDataSourceInterface * datasource);
		void acquire_from_gps_cb(void);
		void acquire_from_file_cb(void);
		void acquire_from_geojson_cb(void);
		void acquire_from_routing_cb(void);
#ifdef VIK_CONFIG_OPENSTREETMAP
		void acquire_from_osm_cb(void);
		void acquire_from_my_osm_cb(void);
#endif

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
		void print_cb(void);


		void change_coord_mode_cb(QAction * qa);

		void configure_event_cb(void);


		void draw_click_cb(QMouseEvent * ev);
		void draw_release_cb(QMouseEvent * ev);


		void export_to_gpx_cb(void);
		void export_to_kml_cb(void);


		void menu_view_cache_info_cb(void);

		bool key_press_event_cb(QKeyEvent * event);
		void drag_data_received_cb(GtkWidget * widget, GdkDragContext *context, int x, int y, GtkSelectionData * selection_data, unsigned int target_type, unsigned int time);



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
		void update_recently_used_document(const QString & file_full_path);
		void update_recent_files(QString const & path);
		void open_window(const QStringList & file_full_paths);

		bool pan_move_flag = false;
		int pan_x = -1;
		int pan_y = -1;
		int delayed_pan_x; /* Temporary storage. */
		int delayed_pan_y; /* Temporary storage. */
		bool single_click_pending = false;


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
		Layer * trigger = NULL;
		Coord trigger_center;

		QString current_document_full_path;

		FileLoadResult loaded_type = FileLoadResult::READ_FAILURE; /* AKA none. */

		/* Tool management state. */
		LayerType tool_layer_type;
		uint16_t tool_tool_id;

		/* Cursor of window's central widget. */
		QCursor viewport_cursor;

		/* Display various window items. */
		bool full_screen_state = false;

		bool scale_visibility = true;
		bool center_mark_visibility = true;
		bool highlight_usage = true;

		bool side_panel_visibility = true;
		bool status_bar_visibility = true;
		bool tool_bar_visibility = true;

		bool only_updating_coord_mode_ui = false; /* Hack for a bug in GTK. */

		QAction * qa_view_full_screen_state = NULL;

		QAction * qa_view_scale_visibility = NULL;
		QAction * qa_view_center_mark_visibility = NULL;
		QAction * qa_view_highlight_usage = NULL;
		QAction * qa_view_side_panel_visibility = NULL;
		QAction * qa_view_status_bar_visibility = NULL;
		QAction * qa_view_tool_bar_visibility = NULL;
		QAction * qa_view_main_menu_visibility = NULL;

		QAction * qa_drawmode_expedia = NULL;
		QAction * qa_drawmode_mercator = NULL;
		QAction * qa_drawmode_latlon = NULL;
		QAction * qa_drawmode_utm = NULL;

		QAction * qa_previous_location = NULL;
		QAction * qa_next_location = NULL;

		std::list<QString> recent_files;

		/* The last used directory for saving viewport to image(s). */
		QUrl last_folder_images_url;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WINDOW_H_ */
