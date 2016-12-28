#ifndef _H_GENERIC_LAYER_TOOLS_H_
#define _H_GENERIC_LAYER_TOOLS_H_



#include <vector>

#include <QAction>
#include <QMouseEvent>
#include <QCursor>

#include "layer.h"




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




	LayerTool * ruler_create(Window * window, Viewport * viewport);
	LayerTool * zoomtool_create(Window * window, Viewport * viewport);
	LayerTool * pantool_create(Window * window, Viewport * viewport);
	LayerTool * selecttool_create(Window * window, Viewport * viewport);



	typedef struct {
		QPixmap * pixmap = NULL;
		/* Track zoom bounds for zoom tool with shift modifier: */
		bool bounds_active = false;
		int start_x = 0;
		int start_y = 0;
	} zoom_tool_state_t;

	class LayerToolZoom : public LayerTool {
	public:
		LayerToolZoom(Window * window, Viewport * viewport);
		~LayerToolZoom();

		LayerToolFuncStatus click_(Layer * layer, QMouseEvent * event);
		LayerToolFuncStatus move_(Layer * layer, QMouseEvent * event);
		LayerToolFuncStatus release_(Layer * layer, QMouseEvent * event);

	private:
		void resize_pixmap(void);
		zoom_tool_state_t * zoom = NULL;
	};


	typedef struct {
		bool has_start_coord = false;
		VikCoord start_coord;
		bool invalidate_start_coord = false; /* Discard/invalidate ->start_coord on release of left mouse button? */
	} ruler_tool_state_t;

	class LayerToolRuler : public LayerTool {
	public:
		LayerToolRuler(Window * window, Viewport * viewport);
		~LayerToolRuler();

		LayerToolFuncStatus release_(Layer * layer, QMouseEvent * event);
		LayerToolFuncStatus move_(Layer * layer, QMouseEvent * event);
		LayerToolFuncStatus click_(Layer * layer, QMouseEvent * event);
		void deactivate_(Layer * layer);
		bool key_press_(Layer * layer, QKeyEvent * event);

	private:
		ruler_tool_state_t * ruler = NULL;
		static void draw(Viewport * viewport, QPixmap * pixmap, QPen & pen, int x1, int y1, int x2, int y2, double distance);
	};


	class LayerToolPan : public LayerTool {
	public:
		LayerToolPan(Window * window, Viewport * viewport);
		~LayerToolPan();

		LayerToolFuncStatus release_(Layer * layer, QMouseEvent * event);
		LayerToolFuncStatus move_(Layer * layer, QMouseEvent * event);
		LayerToolFuncStatus click_(Layer * layer, QMouseEvent * event);
	};


	class LayerToolSelect : public LayerTool {
	public:
		LayerToolSelect(Window * window, Viewport * viewport);
		~LayerToolSelect();

		LayerToolFuncStatus release_(Layer * layer, QMouseEvent * event);
		LayerToolFuncStatus move_(Layer * layer, QMouseEvent * event);
		LayerToolFuncStatus click_(Layer * layer, QMouseEvent * event);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _H_GENERIC_LAYER_TOOLS_H_ */
