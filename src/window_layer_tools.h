#ifndef _H_WINDOW_LAYER_TOOLS_H_
#define _H_WINDOW_LAYER_TOOLS_H_



#include <vector>

#include <QAction>
#include <QMouseEvent>
#include <QCursor>

#include "layer.h"
#include "slav_qt.h"




namespace SlavGPS {




	class Window;
	class Viewport;




	class LayerToolBox {

	public:
		LayerToolBox(Window * win) : window(win) {};
		~LayerToolBox();

		QAction * add_tool(LayerTool * layer_tool);
		void add_group(QActionGroup * group);
		LayerTool * get_tool(QString const & tool_name);;

		void activate_tool(QAction * qa);
		bool deactivate_tool(QAction * qa);
		//void activate_layer_tools(QString const & layer_type);


		void selected_layer(QString const & group_name);

		QAction * set_group_enabled(QString const & group_name);
		//QAction * set_group_disabled(QString const & group_name);
		//void set_other_groups_disabled(QString const & group_name);
		QActionGroup * get_group(QString const & group_name);
		QAction * get_active_tool_action(void);
		LayerTool * get_active_tool(void);


		QCursor const * get_cursor_click(QString const & tool_name);
		QCursor const * get_cursor_release(QString const & tool_name);

		void click(QMouseEvent * event);
		void double_click(QMouseEvent * event);
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




	class LayerToolZoom : public LayerTool {
	public:
		LayerToolZoom(Window * window, Viewport * viewport);
		~LayerToolZoom();
	};



	class LayerToolRuler : public LayerTool {
	public:
		LayerToolRuler(Window * window, Viewport * viewport);
		~LayerToolRuler();
	};


	class LayerToolPan : public LayerTool {
	public:
		LayerToolPan(Window * window, Viewport * viewport);
		~LayerToolPan();
	};


	class LayerToolSelect : public LayerTool {
	public:
		LayerToolSelect(Window * window, Viewport * viewport);
		~LayerToolSelect();
	};




} /* namespace SlavGPS */




#endif /* #ifndef _H_WINDOW_LAYER_TOOLS_H_ */
