#ifndef _H_WINDOW_LAYER_TOOLS_H_
#define _H_WINDOW_LAYER_TOOLS_H_



#include <vector>

#include <QAction>
#include <QMouseEvent>

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
		LayerTool * get_tool(QString & tool_name);;
		void activate(QString & tool_name);
		const GdkCursor * get_cursor(QString & tool_name);

		void click(QMouseEvent * event);
		void move(QMouseEvent * event);
		void release(QMouseEvent * event);



		LayerTool * active_tool = NULL;
		unsigned int n_tools = 0;
		std::vector<LayerTool *> tools;
		Window * window = NULL;
	};




	LayerTool * ruler_create(Window * window, Viewport * viewport);
	LayerTool * zoomtool_create(Window * window, Viewport * viewport);
	LayerTool * pantool_create(Window * window, Viewport * viewport);
	LayerTool * selecttool_create(Window * window, Viewport * viewport);




} /* namespace SlavGPS */




#endif /* #ifndef _H_WINDOW_LAYER_TOOLS_H_ */
