#ifndef _H_TOOLBOX_H_
#define _H_TOOLBOX_H_




#include <vector>




#include <QAction>
#include <QMouseEvent>
#include <QCursor>
#include <QString>




#include "layer_interface.h"




namespace SlavGPS {




	class Window;




	class Toolbox {

	public:
		Toolbox(Window * win) : window(win) {};
		~Toolbox();

		/**
		   @brief Add given tools to toolbox

		   The toolbox becomes owner of the tools in the container.
		   The container is always owned by caller.

		   @param tools - container with tools that should be added to toolbox
		   @return group of freshly created QActions that correspond to the tools
		*/
		QActionGroup * add_tools(const LayerToolContainer & tools);

		/**
		   @brief Get tool by its globally-unique ID

		   Look up a tool in a toolbox using tool's globally-unique ID.

		   @param tool_id - globally unique tool's ID
		   @return pointer to tool on success
		   @return NULL on failure
		*/
		LayerTool * get_tool(const SGObjectTypeID & tool_id) const;

		const LayerTool * get_current_tool(void) const;


		/**
		   @brief Activate a tool specified by its globally unique ID

		   @param tool_id Tool's globally unique ID
		*/
		void activate_tool_by_id(const SGObjectTypeID & tool_id);

		/**
		   @brief Deactivate a tool specified by its globally unique ID

		   @param tool_id Tool's globally unique ID
		*/
		void deactivate_tool_by_id(const SGObjectTypeID & tool_id);

		void deactivate_current_tool(void);


		void activate_tools_group(const QString & group_name);

		QAction * set_group_enabled(const QString & group_name);
		QActionGroup * get_group(const QString & group_name);
		QAction * get_active_tool_action(void);
		LayerTool * get_active_tool(void);

		void handle_mouse_click(QMouseEvent * event);
		void handle_mouse_double_click(QMouseEvent * event);
		void handle_mouse_move(QMouseEvent * event);
		void handle_mouse_release(QMouseEvent * event);

	private:
		Layer * handle_mouse_event_common(void);

		LayerToolContainer tools; /* A map: tool's globally-unique ID -> pointer to the tool. */
		std::vector<QActionGroup *> action_groups;

		LayerTool * active_tool = NULL;
		QAction * active_tool_qa = NULL;
		Window * window = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _H_TOOLBOX_H_ */
