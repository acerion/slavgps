#ifndef _H_WINDOW_LAYER_TOOLS_H_
#define _H_WINDOW_LAYER_TOOLS_H_



#include <vector>

#include <gtk/gtk.h>

#include "viklayer.h"




namespace SlavGPS {




	class Window;
	class Viewport;




	class LayerToolsBox {

	public:
		LayerToolsBox(Window * win) : window(win) {};
		~LayerToolsBox();

		int add_tool(LayerTool * layer_tool);
		LayerTool * get_tool(char const * tool_name);;
		void activate(char const * tool_name);
		const GdkCursor * get_cursor(char const * tool_name);

		void click(GdkEventButton * event);
		void move(GdkEventMotion * event);
		void release(GdkEventButton * event);



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
