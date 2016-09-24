#ifndef _SG_WINDOW_QT_H_
#define _SG_WINDOW_QT_H_




#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QTreeView>
#include <QMouseEvent>
#include <QCursor>

#include "layers_panel.h"
#include "viewport.h"
#include "layer.h"
#include "window_layer_tools.h"




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



	public slots:
		void menu_layer_new_cb(void);

	protected:

	private slots:
		void center_changed_cb(void);
		void layer_tools_cb(QAction * a);

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
		QToolBar * tool_bar = NULL;
		QStatusBar * status_bar = NULL;

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

	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WINDOW_QT_H_ */
