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

#ifndef _SG_WINDOW_H_
#define _SG_WINDOW_H_




#include <vector>
#include <cstdint>
#include <unordered_map>

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "vikwaypoint.h"
#include "viewport.h"
#include "vikstatus.h"
#include "track.h"
#include "layer.h"
#include "layer_trw.h"
#include "layer_trw_containers.h"
#include "file.h"
#include "toolbar.h"
#include "window_layer_tools.h"




namespace SlavGPS {




	class Window {
	public:

		Window();
		~Window();

		/* To call from main to start things off: */
		static Window * new_window();
		void finish_new();
		void draw_scroll(GdkEventScroll * event);
		void open_file(char const * filename, bool change_filename);
		char const * get_filename();
		void set_filename(char const * filename);
		bool window_save();
		bool export_to(std::list<Layer *> * layers, VikFileType_t vft, char const * dir, char const * extension);
		void export_to_common(VikFileType_t vft, char const * extension);
		char const * get_filename_2();
		void selected_layer(Layer * layer);
		GtkWidget * get_drawmode_button(ViewportDrawMode mode);
		void statusbar_update(char const * message, vik_statusbar_type_t vs_type);


		//char * filename = NULL;
		//bool modified = false;
		//VikLoadType_t loaded_type = LOAD_TYPE_READ_FAILURE; /* AKA none. */

		/* Half-drawn update. */
		//Layer * trigger = NULL;
		//VikCoord trigger_center;

	}; /* class Window */




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WINDOW_H_ */
