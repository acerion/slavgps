#ifndef _SG_WINDOW_QT_H_
#define _SG_WINDOW_QT_H_




#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QTreeView>
#include <QMouseEvent>
#include <QCursor>
#include <QCloseEvent>

#include "layers_panel.h"
#include "statusbar.h"
#include "file.h"




#define VIK_SETTINGS_WIN_MAX                     "window_maximized"
#define VIK_SETTINGS_WIN_FULLSCREEN              "window_fullscreen"
#define VIK_SETTINGS_WIN_WIDTH                   "window_width"
#define VIK_SETTINGS_WIN_HEIGHT                  "window_height"
#define VIK_SETTINGS_WIN_PANE_POSITION           "window_horizontal_pane_position"
#define VIK_SETTINGS_WIN_SAVE_IMAGE_WIDTH        "window_save_image_width"
#define VIK_SETTINGS_WIN_SAVE_IMAGE_HEIGHT       "window_save_image_height"
#define VIK_SETTINGS_WIN_SAVE_IMAGE_PNG          "window_save_image_as_png"
#define VIK_SETTINGS_WIN_COPY_CENTRE_FULL_FORMAT "window_copy_centre_full_format"



namespace SlavGPS {




	class Layer;
	class LayerTRW;
	class LayerToolbox;
	class Viewport;




	class Window : public QMainWindow {
		Q_OBJECT

	friend class LayerToolbox;

	public:
		Window();

		void draw_sync();
		void draw_status();
		void draw_redraw();
		void draw_update(void);

		void selected_layer(Layer * layer);



		Viewport * get_viewport(void);
		LayersPanel * get_layers_panel(void);
		QMenu * get_layer_menu(QMenu * menu);
		QMenu * new_layers_submenu_add_actions(QMenu * menu);
		LayerToolbox * get_layer_tools_box(void);
		StatusBar * get_statusbar(void);

		SlavGPS::LayersPanel * layers_panel = NULL;
		SlavGPS::Viewport * viewport = NULL;
		bool modified = false;

		void statusbar_update(StatusBarField field, QString const & message);


		void pan_click(QMouseEvent * event);
		void pan_move(QMouseEvent * event);
		void pan_release(QMouseEvent * event);

		char type_string[30];

		void closeEvent(QCloseEvent * event);



		void toggle_full_screen();

		void toggle_side_panel();
		void toggle_statusbar();
		void toggle_main_menu();
		//void simple_map_update(bool only_new);





		LayerTRW * get_selected_trw_layer();
		void set_selected_trw_layer(LayerTRW * trw);

		Tracks * get_selected_tracks();
		void set_selected_tracks(Tracks * tracks, LayerTRW * trw);

		Track * get_selected_track();
		void set_selected_track(Track * track, LayerTRW * trw);

		Waypoints * get_selected_waypoints();
		void set_selected_waypoints(Waypoints * waypoints, LayerTRW * trw);

		Waypoint * get_selected_waypoint();
		void set_selected_waypoint(Waypoint * wp, LayerTRW * trw);

		/* Return the VikLayer of the selected track(s) or waypoint(s) are in (maybe NULL). */
		//void * vik_window_get_containing_trw_layer(VikWindow *vw);

		/* Return indicates if a redraw is necessary. */
		bool clear_highlight();

		void finish_new(void);


		static void set_redraw_trigger(Layer * layer);

		/* Display controls. */
		bool select_move = false;


		/* Store at this level for highlighted selection drawing since it applies to the viewport and the layers panel. */
		/* Only one of these items can be selected at the same time. */
		LayerTRW * selected_trw = NULL;
		Tracks * selected_tracks = NULL;
		Track * selected_track = NULL;
		Waypoints * selected_waypoints = NULL;
		Waypoint * selected_waypoint = NULL;
		/* Only use for individual track or waypoint. */
		/* For track(s) & waypoint(s) it is the layer they are in - this helps refering to the individual item easier. */
		LayerTRW * containing_trw = NULL;




	public slots:
		void menu_layer_new_cb(void);
		void draw_layer_cb(sg_uid_t uid);
		void draw_update_cb(void);


		void view_full_screen_cb(bool new_state);

		void view_side_panel_cb(bool new_state);
		void view_statusbar_cb(bool new_state);
		void view_main_menu_cb(bool new_state);

		void draw_highlight_cb(bool new_state);
		void draw_scale_cb(bool new_state);
		void draw_centermark_cb(bool new_state);


		void zoom_cb(void);
		void zoom_to_cb(void);


		void show_background_jobs_window_cb(void);

		void show_layer_defaults_cb(void);

		void show_centers_cb();


	protected:

	private slots:
		void center_changed_cb(void);
		void layer_tool_cb(QAction * a);
		void preferences_cb(void);
		void open_file_cb(void);

		void help_help_cb(void);
		void help_about_cb(void);

	private:

		void create_layout(void);
		void create_actions(void);
		void create_ui(void);

		void display_tool_name();
		void open_file(char const * filename, bool change_filename);
		void set_busy_cursor(void);
		void clear_busy_cursor(void);
		void set_filename(char const * filename);
		char const * get_filename(void);
		GtkWidget * get_drawmode_button(ViewportDrawMode mode);
		void update_recently_used_document(char const * filename);
		void update_recent_files(QString const & path);
		void open_window(void);


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

		QMenu * menu_file = NULL;
		QMenu * menu_edit = NULL;
		QMenu * menu_view = NULL;
		QMenu * menu_layers = NULL;
		QMenu * menu_tools = NULL;
		QMenu * menu_help = NULL;

		QMenu * submenu_recent_files = NULL;

		SlavGPS::Layer * trigger = NULL;
		VikCoord trigger_center;

		char * filename = NULL;

		VikLoadType_t loaded_type = LOAD_TYPE_READ_FAILURE; /* AKA none. */

		QAction * qa_layer_properties = NULL;

		/* Tool management state. */
		LayerTool * current_tool = NULL;
		LayerType tool_layer_type;
		uint16_t tool_tool_id;

		LayerToolbox * layer_toolbox = NULL;

		QCursor * busy_cursor = NULL;
		QCursor * viewport_cursor = NULL; /* Only a reference. */



		/* Display various window items. */
		bool view_full_screen = false;

		bool draw_scale = true;
		bool draw_centermark = true;
		bool draw_highlight = true;

		bool view_side_panel = true;
		bool view_statusbar = true;
		bool view_toolbar = true;
		bool view_main_menu = true;

		bool only_updating_coord_mode_ui = false; /* Hack for a bug in GTK. */

		QAction * qa_view_full_screen = NULL;

		QAction * qa_view_show_draw_scale = NULL;
		QAction * qa_view_show_draw_centermark = NULL;
		QAction * qa_view_show_draw_highlight = NULL;
		QAction * qa_view_show_side_panel = NULL;
		QAction * qa_view_show_statusbar = NULL;
		QAction * qa_view_show_toolbar = NULL;
		QAction * qa_view_show_main_menu = NULL;

		std::list<QString> recent_files;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WINDOW_QT_H_ */
