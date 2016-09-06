#ifndef _SG_WINDOW_QT_H_
#define _SG_WINDOW_QT_H_




#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QTreeView>

#include "viklayerspanel.h"
#include "vikviewport.h"
#include "viklayer.h"




namespace SlavGPS {




	class Window : public QMainWindow {
		Q_OBJECT

	public:
		Window();

		void draw_update();
		void draw_sync();
		void draw_status();
		void draw_redraw();

		void selected_layer(Layer * layer);



		Viewport * get_viewport(void);
		LayersPanel * get_layers_panel(void);


	public slots:
		void menu_layer_new_cb(void);

	protected:

	private slots:

	private:

		void create_layout(void);
		void create_actions();

		QMenuBar * menu_bar = NULL;
		QToolBar * tool_bar = NULL;
		QStatusBar * status_bar = NULL;

		SlavGPS::LayersPanel * layers_panel = NULL;
		SlavGPS::Viewport * viewport = NULL;

		SlavGPS::Layer * trigger = NULL;
		VikCoord trigger_center;

		char * filename = NULL;
		bool modified = false;
		//VikLoadType_t loaded_type = LOAD_TYPE_READ_FAILURE; /* AKA none. */
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WINDOW_QT_H_ */
