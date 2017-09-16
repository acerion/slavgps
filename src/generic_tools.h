/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _H_GENERIC_TOOLS_H_
#define _H_GENERIC_TOOLS_H_



#include <QMouseEvent>

#include "layer.h"




namespace SlavGPS {




	class Window;
	class Viewport;



	LayerTool * ruler_create(Window * window, Viewport * viewport);
	LayerTool * zoomtool_create(Window * window, Viewport * viewport);
	LayerTool * pantool_create(Window * window, Viewport * viewport);
	LayerTool * selecttool_create(Window * window, Viewport * viewport);



	class GenericToolZoom : public LayerTool {
	public:
		GenericToolZoom(Window * window, Viewport * viewport);
		~GenericToolZoom();

		ToolStatus handle_mouse_click(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_move(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_release(Layer * layer, QMouseEvent * event);

	private:
		/* "ztr" == "zoom to rectangle". */
		bool ztr_is_active = false;
		int ztr_start_x = 0;
		int ztr_start_y = 0;
		QPixmap ztr_orig_viewport_pixmap; /* Pixmap with saved viewport's state without "zoom to rectangle" mark. */
	};



	class GenericToolRuler : public LayerTool {
	public:
		GenericToolRuler(Window * window, Viewport * viewport);
		~GenericToolRuler();

		ToolStatus handle_mouse_release(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_move(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_click(Layer * layer, QMouseEvent * event);
		ToolStatus handle_key_press(Layer * layer, QKeyEvent * event);
		void deactivate_tool(Layer * layer);

	private:
		void draw(QPainter & painter, int x1, int y1, int x2, int y2, double distance);

		QPen pen;

		QPixmap orig_viewport_pixmap; /* Pixmap with saved viewport's state without ruler drawn on top of it. */

		bool has_start_coord = false;
		Coord start_coord;
		bool invalidate_start_coord = false; /* Discard/invalidate ->start_coord on release of left mouse button? */
	};


	class LayerToolPan : public LayerTool {
	public:
		LayerToolPan(Window * window, Viewport * viewport);
		~LayerToolPan();

		ToolStatus handle_mouse_release(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_move(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_click(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_double_click(Layer * layer, QMouseEvent * event);
	};


	class LayerToolSelect : public LayerTool {
	public:
		LayerToolSelect(Window * window, Viewport * viewport);
		~LayerToolSelect();

		ToolStatus handle_mouse_release(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_move(Layer * layer, QMouseEvent * event);
		ToolStatus handle_mouse_click(Layer * layer, QMouseEvent * event);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _H_GENERIC_TOOLS_H_ */
