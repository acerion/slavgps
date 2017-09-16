#ifndef _H_TOOLBOX_H_
#define _H_TOOLBOX_H_




#include <vector>

#include <QAction>
#include <QMouseEvent>
#include <QCursor>

#include "layer.h"




namespace SlavGPS {




	class Window;




	class Toolbox {

	public:
		Toolbox(Window * win) : window(win) {};
		~Toolbox();

		QAction * add_tool(LayerTool * layer_tool);
		void add_group(QActionGroup * group);
		LayerTool * get_tool(const QString & tool_name);
		const LayerTool * get_current_tool(void) const;


		void activate_tool(QAction * qa);
		bool deactivate_tool(QAction * qa);

		void activate_tool(LayerType layer_type, int tool_id);
		void deactivate_tool(LayerType layer_type, int tool_id);

		void deactivate_tool(LayerTool * tool);
		void deactivate_current_tool(void);


		void handle_selection_of_layer(const QString & group_name);

		QAction * set_group_enabled(const QString & group_name);
		QActionGroup * get_group(const QString & group_name);
		QAction * get_active_tool_action(void);
		LayerTool * get_active_tool(void);

		const QCursor * get_cursor_click(const QString & tool_name);
		const QCursor * get_cursor_release(const QString & tool_name);

		void handle_mouse_click(QMouseEvent * event);
		void handle_mouse_double_click(QMouseEvent * event);
		void handle_mouse_move(QMouseEvent * event);
		void handle_mouse_release(QMouseEvent * event);

	private:
		std::vector<LayerTool *> tools;
		std::vector<QActionGroup *> action_groups;

		LayerTool * active_tool = NULL;
		QAction * active_tool_qa = NULL;
		Window * window = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _H_TOOLBOX_H_ */
