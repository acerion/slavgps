#ifndef _H_LAYER_TOOLBOX_H_
#define _H_LAYER_TOOLBOX_H_




#include <vector>

#include <QAction>
#include <QMouseEvent>
#include <QCursor>

#include "layer.h"




namespace SlavGPS {




	class Window;




	class LayerToolbox {

	public:
		LayerToolbox(Window * win) : window(win) {};
		~LayerToolbox();

		QAction * add_tool(LayerTool * layer_tool);
		void add_group(QActionGroup * group);
		LayerTool * get_tool(QString const & tool_name);


		void activate_tool(QAction * qa);
		bool deactivate_tool(QAction * qa);

		void activate_tool(LayerType layer_type, int tool_id);
		void deactivate_tool(LayerType layer_type, int tool_id);

		void deactivate_tool(LayerTool * tool);
		void deactivate_current_tool(void);


		void selected_layer(QString const & group_name);

		QAction * set_group_enabled(QString const & group_name);
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




} /* namespace SlavGPS */




#endif /* #ifndef _H_LAYER_TOOLBOX_H_ */
