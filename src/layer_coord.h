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

#ifndef _SG_LAYER_COORD_H_
#define _SG_LAYER_COORD_H_




#include "layer.h"
#include "layer_interface.h"




namespace SlavGPS {




	class Viewport;




	class LayerCoordInterface : public LayerInterface {
	public:
		LayerCoordInterface();
		Layer * unmarshall(uint8_t * data, size_t data_len, Viewport * viewport);
	};




	class LayerCoord : public Layer {
	public:
		LayerCoord();
		~LayerCoord() {};

		/* Layer interface methods. */
		void draw_tree_item(Viewport * viewport, bool highlight_selected, bool parent_is_selected);
		bool set_param_value(uint16_t id, const SGVariant & param_value, bool is_file_operation);
		SGVariant get_param_value(param_id_t id, bool is_file_operation) const;

	private:
		void draw_latlon(Viewport * viewport);
		void draw_utm(Viewport * viewport);

		QColor color_deg;       /* Color of "degrees" coordinate lines. */
		QColor color_min;       /* Color of "minutes" coordinate lines. */
		QColor color_sec;       /* Color of "seconds" coordinate lines. */
		int32_t line_thickness; /* Base thickness of coordinate lines. */

		double deg_inc;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_COORD_H_ */
