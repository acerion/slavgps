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

#ifndef _VIKING_VIKWINDOW_H
#define _VIKING_VIKWINDOW_H
/* Requires <gtk/gtk.h> or glib, and coords.h*/

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <stdint.h>

#include <unordered_map>

#include "vikwaypoint.h"
#include "vikviewport.h"
#include "vikstatus.h"
#include "viktrack.h"
#include "viklayer.h"
#include "viktrwlayer.h"
#include "layer_trw_containers.h"
#include "file.h"
#include "toolbar.h"





#ifdef __cplusplus
extern "C" {
#endif





#define VIK_WINDOW_TYPE            (vik_window_get_type ())
#define VIK_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_WINDOW_TYPE, VikWindow))
#define VIK_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_WINDOW_TYPE, VikWindowClass))
#define IS_VIK_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_WINDOW_TYPE))
#define IS_VIK_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_WINDOW_TYPE))

typedef struct _VikWindow VikWindow;
typedef struct _VikWindowClass VikWindowClass;

struct _VikWindowClass {
	GtkWindowClass window_class;
	void (* newwindow) (VikWindow * vw);
	void (* openwindow) (VikWindow * vw, GSList * filenames);
};

GType vik_window_get_type();





#ifdef __cplusplus
}
#endif







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





#define TOOL_LAYER_TYPE_NONE -1

	typedef struct {
		LayerTool * active_tool;
		int n_tools;
		LayerTool ** tools;
		Window * window;
	} toolbox_tools_t;





	class Window {
	public:

		Window();
		~Window();

		// To call from main to start things off:
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

		/* Return the VikLayer of the selected track(s) or waypoint(s) are in (maybe NULL) */
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

		void enable_layer_tool(int layer_id, int tool_id);
		GThread * get_thread();




		/* Store at this level for highlighted selection drawing since it applies to the viewport and the layers panel */
		/* Only one of these items can be selected at the same time */
		LayerTRW * selected_trw;
		Tracks * selected_tracks;
		Track * selected_track;
		std::unordered_map<sg_uid_t, Waypoint *> * selected_waypoints;
		Waypoint * selected_waypoint;
		/* only use for individual track or waypoint */
		/* For track(s) & waypoint(s) it is the layer they are in - this helps refering to the individual item easier */
		LayerTRW * containing_trw;

		Viewport * viewport;
		LayersPanel * layers_panel;
		VikStatusbar * viking_vs;
		VikToolbar * viking_vtb;

		char * filename;
		bool modified;
		VikLoadType_t loaded_type;

		unsigned int draw_image_width;
		unsigned int draw_image_height;
		bool draw_image_save_as_png;



		bool only_updating_coord_mode_ui; /* hack for a bug in GTK */
		GtkUIManager * uim;

		GThread  * thread;
		/* half-drawn update */
		VikLayer * trigger;
		VikCoord trigger_center;


		// Display controls
		// NB scale, centermark and highlight are in viewport.
		bool show_full_screen;
		bool show_side_panel;
		bool show_statusbar;
		bool show_toolbar;
		bool show_main_menu;


		GdkCursor * busy_cursor;
		GdkCursor * viewport_cursor; // only a reference



		/* tool management state */
		unsigned int current_tool;
		uint16_t tool_layer_id;
		uint16_t tool_tool_id;
		toolbox_tools_t * vt;


		// Display controls
		bool select_move;
		bool pan_move;
		int pan_x, pan_y;
		int delayed_pan_x, delayed_pan_y; // Temporary storage
		bool single_click_pending;



		GtkWidget * hpaned;
		GtkWidget * main_vbox;
		GtkWidget * menu_hbox;




		GtkActionGroup * action_group;


		void * vw; /* VikWindow. */


		// private:
		void simple_map_update(bool only_new);
		void toggle_side_panel();
		void toggle_full_screen();
		void toggle_statusbar();
		void toggle_toolbar();
		void toggle_main_menu();

		char type_string[30];

	}; /* class Window */





} /* namespace SlavGPS */





#define VIK_WINDOW_FROM_WIDGET(x) VIK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(x)))
VikWindow * vik_window_from_layer(SlavGPS::Layer * layer);
SlavGPS::Window * window_from_layer(SlavGPS::Layer * layer);
SlavGPS::Window * window_from_widget(void * widget);
SlavGPS::Window * window_from_vik_window(VikWindow * vw);





#endif
