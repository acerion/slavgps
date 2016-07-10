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
 *
 */

#ifndef _SG_LAYER_DEM_H
#define _SG_LAYER_DEM_H





#include "viklayer.h"
#include "dem.h"





namespace SlavGPS {





	class LayerDEM : public Layer {
	public:
		LayerDEM();
		LayerDEM(Viewport * viewport);
		~LayerDEM();

		/* Layer interface methods. */
		void draw(Viewport * viewport);
		char const * tooltip();
		void marshall(uint8_t ** data, int * len);
		bool download_release(GdkEventButton * event, LayerTool * tool);
		bool add_file(std::string& dem_filename);
		void draw_dem(Viewport * viewport, VikDEM * dem);
		bool set_param(uint16_t id, VikLayerParamData data, Viewport * viewport, bool is_file_operation);
		VikLayerParamData get_param(uint16_t id, bool is_file_operation);


		GdkGC ** gcs;
		GdkGC ** gcsgradient;
		GList * files;
		double min_elev;
		double max_elev;
		GdkColor color;
		unsigned int source;
		unsigned int dem_type;

		// right click menu only stuff - similar to mapslayer
		GtkMenu * right_click_menu;
	};





}





#endif /* #ifndef _SG_LAYER_DEM_H */
