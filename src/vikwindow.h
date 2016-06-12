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
#include <stdbool.h>
#include <stdint.h>

#include <unordered_map>

#include "vikwaypoint.h"
#include "vikviewport.h"
#include "vikstatus.h"
#include "viktrack.h"


#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration. */
struct _VikTrwLayer;


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

// To call from main to start things off:
VikWindow * vik_window_new_window();

void vik_window_new_window_finish(VikWindow * vw);

GtkWidget *vik_window_get_drawmode_button(VikWindow * vw, VikViewportDrawMode mode);
bool vik_window_get_pan_move(VikWindow *vw);
void vik_window_open_file(VikWindow * vw, char const * filename, bool changefilename);
struct _VikLayer;
void vik_window_selected_layer(VikWindow *vw, struct _VikLayer * vl);
SlavGPS::Viewport * vik_window_viewport(VikWindow *vw);
struct _VikLayersPanel * vik_window_layers_panel(VikWindow * vw);
struct _VikStatusbar * vik_window_get_statusbar(VikWindow * vw);
char const *vik_window_get_filename(VikWindow * vw);

void vik_window_statusbar_update(VikWindow * vw, char const * message, vik_statusbar_type_t vs_type);

void vik_window_set_redraw_trigger(struct _VikLayer * vl);

void vik_window_enable_layer_tool(VikWindow * vw, int layer_id, int tool_id);

void * vik_window_get_selected_trw_layer(VikWindow * vw); /* return type LayerTRW */
void vik_window_set_selected_trw_layer(VikWindow * vw, void * trw);
std::unordered_map<sg_uid_t, SlavGPS::Track *> * vik_window_get_selected_tracks(VikWindow * vw);
void vik_window_set_selected_tracks(VikWindow *vw, std::unordered_map<sg_uid_t, SlavGPS::Track *> * tracks, void * trw);
SlavGPS::Track * vik_window_get_selected_track (VikWindow *vw); /* return type Track */
void vik_window_set_selected_track(VikWindow * vw, SlavGPS::Track * track, void * trw);
std::unordered_map<sg_uid_t, SlavGPS::Waypoint *> * vik_window_get_selected_waypoints(VikWindow * vw);
void vik_window_set_selected_waypoints (VikWindow * vw, std::unordered_map<sg_uid_t, SlavGPS::Waypoint *> * waypoints, void * trw);
SlavGPS::Waypoint * vik_window_get_selected_waypoint(VikWindow * vw); /* return type VikWaypoint */
void vik_window_set_selected_waypoint(VikWindow * vw, SlavGPS::Waypoint * wp, void * trw);
/* Return the VikLayer of the selected track(s) or waypoint(s) are in (maybe NULL) */
//void * vik_window_get_containing_trw_layer (VikWindow *vw);
/* return indicates if a redraw is necessary */
bool vik_window_clear_highlight(VikWindow * vw);

GThread * vik_window_get_thread(VikWindow * vw);

void vik_window_set_busy_cursor(VikWindow * vw);
void vik_window_clear_busy_cursor(VikWindow * vw);

typedef struct {
	VikWindow * vw;
	SlavGPS::Viewport * viewport;
	void * trw; // LayerTRW
	bool holding;
	bool moving;
	bool is_waypoint; // otherwise a track
	GdkGC * gc;
	int oldx, oldy;
} tool_ed_t;

#ifdef __cplusplus
}
#endif

#define VIK_WINDOW_FROM_WIDGET(x) VIK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(x)))

#endif
