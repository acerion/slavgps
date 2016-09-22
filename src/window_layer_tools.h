#ifndef _H_WINDOW_LAYER_TOOLS_H_
#define _H_WINDOW_LAYER_TOOLS_H_



#include <vector>

#include <QAction>
#include <QMouseEvent>
#include <QCursor>

#include "viklayer.h"
#include "slav_qt.h"




namespace SlavGPS {




	class Window;
	class Viewport;




	class LayerToolsBox {

	public:
		LayerToolsBox(Window * win) : window(win) {};
		~LayerToolsBox();

		QAction * add_tool(LayerTool * layer_tool);
		void add_layer_tools(QActionGroup * group);
		LayerTool * get_tool(QString & tool_name);;

		void activate_tool(QAction * qa);
		void activate_layer_tools(QAction * qa);
		void activate_layer_tools(QString & layer_type);


		QCursor const * get_cursor_click(QString & tool_name);
		QCursor const * get_cursor_release(QString & tool_name);

		void click(QMouseEvent * event);
		void move(QMouseEvent * event);
		void release(QMouseEvent * event);



		LayerTool * active_tool = NULL;
		QAction * active_tool_qa = NULL;
		unsigned int n_tools = 0;
		std::vector<LayerTool *> tools;
		Window * window = NULL;

	private:
		std::vector<QActionGroup *> action_groups;
	};




	LayerTool * ruler_create(Window * window, Viewport * viewport);
	LayerTool * zoomtool_create(Window * window, Viewport * viewport);
	LayerTool * pantool_create(Window * window, Viewport * viewport);
	LayerTool * selecttool_create(Window * window, Viewport * viewport);




} /* namespace SlavGPS */




#endif /* #ifndef _H_WINDOW_LAYER_TOOLS_H_ */
