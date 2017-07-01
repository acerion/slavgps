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

#ifndef _H_GENERIC_LAYER_TOOLS_H_
#define _H_GENERIC_LAYER_TOOLS_H_



#include <QMouseEvent>

#include "layer.h"




namespace SlavGPS {




	class Window;
	class Viewport;




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
		Coord start_coord;
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
