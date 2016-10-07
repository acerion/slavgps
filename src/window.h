#ifndef _SG_WINDOW_QT_H_
#define _SG_WINDOW_QT_H_




#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QTreeView>
#include <QMouseEvent>
#include <QCursor>
#include <QCloseEvent>

#include "layers_panel.h"
#include "viewport.h"
#include "layer.h"
#include "window_layer_tools.h"



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




	class Window : public QMainWindow {
		Q_OBJECT

	friend class LayerToolsBox;

	public:
		Window();

		void draw_update();
		void draw_sync();
		void draw_status();
		void draw_redraw();

		void selected_layer(Layer * layer);



		Viewport * get_viewport(void);
		LayersPanel * get_layers_panel(void);
		QMenu * get_layer_menu(QMenu * menu);
		LayerToolsBox * get_layer_tools_box();

		SlavGPS::LayersPanel * layers_panel = NULL;
		SlavGPS::Viewport * viewport = NULL;
		bool modified = false;

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

	public slots:
		void menu_layer_new_cb(void);
		void draw_layer_cb(sg_uid_t uid);


		void view_full_screen_cb(bool new_state);

		void view_side_panel_cb(bool new_state);
		void view_statusbar_cb(bool new_state);
		void view_main_menu_cb(bool new_state);

		void draw_highlight_cb(bool new_state);
		void draw_scale_cb(bool new_state);
		void draw_centermark_cb(bool new_state);


		void zoom_cb(void);
		void zoom_to_cb(void);


	protected:

	private slots:
		void center_changed_cb(void);
		void layer_tools_cb(QAction * a);
		void preferences_cb(void);

	private:

		void create_layout(void);
		void create_actions(void);
		void create_ui(void);


		bool pan_move_flag = false;
		int pan_x = -1;
		int pan_y = -1;
		int delayed_pan_x; /* Temporary storage. */
		int delayed_pan_y; /* Temporary storage. */
		bool single_click_pending = false;


		QMenuBar * menu_bar = NULL;
		QToolBar * toolbar = NULL;
		QStatusBar * status_bar = NULL;
		QDockWidget * panel_dock = NULL;

		QMenu * menu_file = NULL;
		QMenu * menu_edit = NULL;
		QMenu * menu_view = NULL;
		QMenu * menu_layers = NULL;
		QMenu * menu_tools = NULL;
		QMenu * menu_help = NULL;

		SlavGPS::Layer * trigger = NULL;
		VikCoord trigger_center;

		char * filename = NULL;

		//VikLoadType_t loaded_type = LOAD_TYPE_READ_FAILURE; /* AKA none. */

		QAction * qa_layer_properties = NULL;

		/* Tool management state. */
		unsigned int current_tool;
		LayerType tool_layer_type;
		uint16_t tool_tool_id;

		LayerToolsBox * tb = NULL;

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


	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WINDOW_QT_H_ */
