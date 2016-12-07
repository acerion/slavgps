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




typedef enum {
	VW_GEN_SINGLE_IMAGE,
	VW_GEN_DIRECTORY_OF_IMAGES,
	VW_GEN_KMZ_FILE,
} img_generation_t;




namespace SlavGPS {




	class LayerTRW;
	class LayersPanel;
	class Window;
	class LayerTool;
	class Window;




	class Window {
	public:

		Window();
		~Window();

		/* To call from main to start things off: */
		static Window * new_window();


		void finish_new();

		void draw_redraw();
		void draw_status();
		void draw_scroll(GdkEventScroll * event);
		void draw_sync();
		void draw_update();

		void open_file(char const * filename, bool change_filename);

		char const * get_filename();
		void set_filename(char const * filename);

		void set_busy_cursor();
		void clear_busy_cursor();

		void toggle_draw_scale(GtkAction * a);
		void toggle_draw_centermark(GtkAction * a);
		void toggle_draw_highlight(GtkAction * a);

		bool window_save();

		void update_recently_used_document(char const * filename);
		void setup_recent_files();

		bool export_to(std::list<Layer *> * layers, VikFileType_t vft, char const * dir, char const * extension);
		void export_to_common(VikFileType_t vft, char const * extension);

		void save_image_dir(char const * fn, unsigned int w, unsigned int h, double zoom, bool save_as_png, unsigned int tiles_w, unsigned int tiles_h);
		void draw_to_image_file(img_generation_t img_gen);


		char const * get_filename_2();

		void selected_layer(Layer * layer);
		Viewport * get_viewport();


		GtkWidget * get_drawmode_button(ViewportDrawMode mode);
		bool get_pan_move();

		void statusbar_update(char const * message, vik_statusbar_type_t vs_type);



		void enable_layer_tool(LayerType layer_type, int tool_id);
		GThread * get_thread();

		GtkWindow * get_toolkit_window(void);
		GtkWindow * get_toolkit_window_2(void);
		GtkWidget * get_toolkit_widget(void);
		void * get_toolkit_object(void);


		void pan_click(GdkEventButton * event);
		void pan_move(GdkEventMotion * event);
		void pan_release(GdkEventButton * event);



		Viewport * viewport = NULL;
		LayersPanel * layers_panel = NULL;
		VikStatusbar * viking_vs = NULL;
		VikToolbar * viking_vtb = NULL;

		//char * filename = NULL;
		//bool modified = false;
		//VikLoadType_t loaded_type = LOAD_TYPE_READ_FAILURE; /* AKA none. */

		unsigned int draw_image_width;
		unsigned int draw_image_height;
		bool draw_image_save_as_png = false;


		GtkUIManager * uim = NULL;

		GThread  * thread = NULL;
		/* Half-drawn update. */
		//Layer * trigger = NULL;
		//VikCoord trigger_center;


		LayerToolsBox * tb = NULL;



		GtkWidget * hpaned = NULL;
		GtkWidget * main_vbox = NULL;
		GtkWidget * menu_hbox = NULL;

		GtkActionGroup * action_group = NULL;

		GtkWindow * gtk_window_ = NULL;




		// private:


	private:
		void init_toolkit_widget(void);

		char type_string[30] = { 0 };

	}; /* class Window */




} /* namespace SlavGPS */




SlavGPS::Window * window_from_widget(void * widget);
GtkWindow * toolkit_window_from_widget(void * widget);
void window_init(void);




#endif /* #ifndef _SG_WINDOW_H_ */
