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
#include "viktrack.h"
#include "layer.h"
#include "viktrwlayer.h"
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

		LayerTRW * get_selected_trw_layer();
		void set_selected_trw_layer(LayerTRW * trw);

		Tracks * get_selected_tracks();
		void set_selected_tracks(Tracks * tracks, LayerTRW * trw);

		Track * get_selected_track();
		void set_selected_track(Track * track, LayerTRW * trw);

		Waypoints * get_selected_waypoints();
		void set_selected_waypoints(Waypoints * waypoints, LayerTRW * trw);

		Waypoint * get_selected_waypoint();
		void set_selected_waypoint(Waypoint * wp, LayerTRW * trw);

		/* Return the VikLayer of the selected track(s) or waypoint(s) are in (maybe NULL). */
		//void * vik_window_get_containing_trw_layer(VikWindow *vw);

		/* Return indicates if a redraw is necessary. */
		bool clear_highlight();

		char const * get_filename_2();

		void selected_layer(Layer * layer);
		Viewport * get_viewport();


		GtkWidget * get_drawmode_button(VikViewportDrawMode mode);
		bool get_pan_move();
		LayersPanel * get_layers_panel();
		struct _VikStatusbar * get_statusbar();
		void statusbar_update(char const * message, vik_statusbar_type_t vs_type);

		static void set_redraw_trigger(Layer * layer);

		void enable_layer_tool(LayerType layer_type, int tool_id);
		GThread * get_thread();

		GtkWindow * get_toolkit_window(void);
		GtkWindow * get_toolkit_window_2(void);
		GtkWidget * get_toolkit_widget(void);
		void * get_toolkit_object(void);


		void pan_click(GdkEventButton * event);
		void pan_move(GdkEventMotion * event);
		void pan_release(GdkEventButton * event);


		/* Store at this level for highlighted selection drawing since it applies to the viewport and the layers panel. */
		/* Only one of these items can be selected at the same time. */
		LayerTRW * selected_trw = NULL;
		Tracks * selected_tracks = NULL;
		Track * selected_track = NULL;
		std::unordered_map<sg_uid_t, Waypoint *> * selected_waypoints = NULL;
		Waypoint * selected_waypoint = NULL;
		/* Only use for individual track or waypoint. */
		/* For track(s) & waypoint(s) it is the layer they are in - this helps refering to the individual item easier. */
		LayerTRW * containing_trw = NULL;

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

		bool only_updating_coord_mode_ui = false; /* Hack for a bug in GTK. */
		GtkUIManager * uim = NULL;

		GThread  * thread = NULL;
		/* Half-drawn update. */
		//Layer * trigger = NULL;
		//VikCoord trigger_center;


		LayerToolsBox * tb = NULL;

		/* Display controls. */
		bool select_move = false;




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
