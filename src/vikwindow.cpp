/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2006, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2012-2015, Rob Norris <rw_norris@hotmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "viking.h"
#include "background.h"
#include "acquire.h"
#include "datasources.h"
#include "geojson.h"
#include "vikgoto.h"
#include "dems.h"
#include "mapcache.h"
#include "print.h"
#include "preferences.h"
#include "viklayer_defaults.h"
#include "icons/icons.h"
#include "vikexttools.h"
#include "vikexttool_datasources.h"
#include "garminsymbols.h"
#include "vikmapslayer.h"
#include "geonamessearch.h"
#include "vikutils.h"
#include "dir.h"
#include "kmz.h"
#include "file.h"
#include "fileutils.h"
#include "dialog.h"
#include "clipboard.h"
#include "settings.h"
#include "globals.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <ctype.h>
#include <assert.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gdk/gdkkeysyms.h>


using namespace SlavGPS;



// This seems rather arbitary, quite large and pointless
//  I mean, if you have a thousand windows open;
//   why not be allowed to open a thousand more...
#define MAX_WINDOWS 1024
static unsigned int window_count = 0;
static std::list<Window *> window_list;

#define VIKING_WINDOW_WIDTH      1000
#define VIKING_WINDOW_HEIGHT     800
#define DRAW_IMAGE_DEFAULT_WIDTH 1280
#define DRAW_IMAGE_DEFAULT_HEIGHT 1024
#define DRAW_IMAGE_DEFAULT_SAVE_AS_PNG true






// The last used directories
static char *last_folder_files_uri = NULL;
static char *last_folder_images_uri = NULL;

static void window_finalize(GObject *gob);
static GObjectClass *parent_class;



static void draw_update_cb(Window * window);

static void newwindow_cb(GtkAction *a, Window * window);

// Signals
static void open_window(Window * window, GSList * files);
static void destroy_window(GtkWidget * widget, void * data);

/* Drawing & stuff */

static bool delete_event(VikWindow * vw);

static bool key_press_event_cb(Window * window, GdkEventKey * event, void * data);

static void center_changed_cb(Window * window);
static void window_configure_event(Window * window);
static void draw_sync_cb(Window * window);
static void draw_scroll_cb(Window * window, GdkEventScroll * event);
static void draw_click_cb(Window * window, GdkEventButton * event);
static void draw_release_cb(Window * window, GdkEventButton * event);
static void draw_mouse_motion_cb(Window * window, GdkEventMotion * event);
static void draw_zoom_cb(GtkAction * a, Window * window);
static void draw_goto_cb(GtkAction * a, Window * window);
static void draw_refresh_cb(GtkAction * a, Window * window);

static bool vik_window_clear_highlight_cb(Window * window);

/* End Drawing Functions */

static void toggle_draw_scale(GtkAction * a, Window * window);
static void toggle_draw_centermark(GtkAction * a, Window * window);
static void toggle_draw_highlight(GtkAction * a, Window * window);

static void menu_addlayer_cb(GtkAction * a, Window * window);
static void menu_properties_cb(GtkAction * a, Window * window);
static void menu_delete_layer_cb(GtkAction * a, Window * window);


static void menu_cb(GtkAction * old, GtkAction * a, Window * window);
static void window_change_coord_mode_cb(GtkAction * old, GtkAction * a, Window * window);
static toolbox_tools_t* toolbox_create(Window * window);
static void toolbox_add_tool(toolbox_tools_t * vt, VikToolInterface * vti, int layer_type);
static int toolbox_get_tool(toolbox_tools_t * vt, char const * tool_name);
static void toolbox_activate(toolbox_tools_t * vt, char const * tool_name);
static const GdkCursor *toolbox_get_cursor(toolbox_tools_t * vt, char const * tool_name);
static void toolbox_click(toolbox_tools_t * vt, GdkEventButton * event);
static void toolbox_move(toolbox_tools_t * vt, GdkEventMotion * event);
static void toolbox_release(toolbox_tools_t * vt, GdkEventButton * event);


/* ui creation */
static void window_create_ui(Window * window);
static void register_vik_icons(GtkIconFactory * icon_factory);

/* i/o */
static void load_file(GtkAction * a, Window * window);
static bool save_file_as(GtkAction * a, Window * window);
static bool save_file(GtkAction * a, Window * window);
static bool save_file_and_exit(GtkAction * a, Window * window);


struct _VikWindow {
	GtkWindow gtkwindow;

	Window * window;
};

enum {
	TOOL_PAN = 0,
	TOOL_ZOOM,
	TOOL_RULER,
	TOOL_SELECT,
	TOOL_LAYER,
	NUMBER_OF_TOOLS
};

enum {
	VW_NEWWINDOW_SIGNAL,
	VW_OPENWINDOW_SIGNAL,
	VW_LAST_SIGNAL
};

static unsigned int window_signals[VW_LAST_SIGNAL] = { 0 };

// TODO get rid of this as this is unnecessary duplication...
static char *tool_names[NUMBER_OF_TOOLS] = { (char *) N_("Pan"), (char *) N_("Zoom"), (char *) N_("Ruler"), (char *) N_("Select") };

G_DEFINE_TYPE (VikWindow, vik_window, GTK_TYPE_WINDOW)

Viewport * Window::get_viewport()
{
	return this->viewport;
}

LayersPanel * Window::get_layers_panel()
{
	return this->layers_panel;
}

/**
 *  Returns the statusbar for the window
 */
VikStatusbar * Window::get_statusbar()
{
	return this->viking_vs;
}

/**
 *  Returns the 'project' filename
 */
char const * Window::get_filename_2()
{
	return this->filename;
}

typedef struct {
	VikStatusbar *vs;
	vik_statusbar_type_t vs_type;
	char * message; // Always make a copy of this data
} statusbar_idle_data;

/**
 * For the actual statusbar update!
 */
static bool statusbar_idle_update(statusbar_idle_data * sid)
{
	vik_statusbar_set_message(sid->vs, sid->vs_type, sid->message);
	free(sid->message);
	free(sid);
	return false;
}





/**
 * @message: The string to be displayed. This is copied.
 * @vs_type: The part of the statusbar to be updated.
 *
 * This updates any part of the statusbar with the new string.
 * It handles calling from the main thread or any background thread
 * ATM this mostly used from background threads - as from the main thread
 *  one may use the vik_statusbar_set_message() directly.
 */
void Window::statusbar_update(char const * message, vik_statusbar_type_t vs_type)
{
	GThread * thread = this->get_thread();
	if (!thread) {
		// Do nothing
		return;
	}

	statusbar_idle_data * sid = (statusbar_idle_data *) malloc(sizeof (statusbar_idle_data));
	sid->vs = this->viking_vs;
	sid->vs_type = vs_type;
	sid->message = g_strdup(message);

	if (g_thread_self() == thread) {
		g_idle_add ((GSourceFunc) statusbar_idle_update, sid);
	} else {
		// From a background thread
		gdk_threads_add_idle((GSourceFunc) statusbar_idle_update, sid);
	}
}


// Actual signal handlers
static void destroy_window(GtkWidget * widget, void * data)
{
	if (! --window_count) {
		free(last_folder_files_uri);
		free(last_folder_images_uri);
		gtk_main_quit();
	}
}

#define VIK_SETTINGS_WIN_SIDEPANEL "window_sidepanel"
#define VIK_SETTINGS_WIN_STATUSBAR "window_statusbar"
#define VIK_SETTINGS_WIN_TOOLBAR "window_toolbar"
// Menubar setting to off is never auto saved in case it's accidentally turned off
// It's not so obvious so to recover the menu visibility.
// Thus this value is for setting manually via editting the settings file directly
#define VIK_SETTINGS_WIN_MENUBAR "window_menubar"


VikWindow * vik_window_new_window(VikWindow * vw)
{
	Window * new_window = new Window();
	return (VikWindow *) new_window->vw;
}

Window * Window::new_window()
{
	if (window_count < MAX_WINDOWS) {
		Window * window = new Window();

		g_signal_connect(G_OBJECT (window->vw), "destroy",
				 G_CALLBACK (destroy_window), NULL);
		g_signal_connect(G_OBJECT (window->vw), "newwindow",
				 G_CALLBACK (vik_window_new_window), NULL);
		g_signal_connect(G_OBJECT (window->vw), "openwindow",
				 G_CALLBACK (open_window), NULL);

		gtk_widget_show_all(GTK_WIDGET(window->vw));

		if (a_vik_get_restore_window_state()) {
			// These settings are applied after the show all as these options hide widgets
			bool sidepanel;
			if (a_settings_get_boolean(VIK_SETTINGS_WIN_SIDEPANEL, &sidepanel)) {
				if (! sidepanel) {
					gtk_widget_hide(GTK_WIDGET(window->layers_panel->gob));
					GtkWidget *check_box = gtk_ui_manager_get_widget(window->uim, "/ui/MainMenu/View/SetShow/ViewSidePanel");
					gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_box), false);
				}
			}

			bool statusbar;
			if (a_settings_get_boolean(VIK_SETTINGS_WIN_STATUSBAR, &statusbar)) {
				if (! statusbar) {
					gtk_widget_hide(GTK_WIDGET(window->viking_vs));
					GtkWidget *check_box = gtk_ui_manager_get_widget(window->uim, "/ui/MainMenu/View/SetShow/ViewStatusBar");
					gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_box), false);
				}
			}

			bool toolbar;
			if (a_settings_get_boolean(VIK_SETTINGS_WIN_TOOLBAR, &toolbar)) {
				if (! toolbar) {
					gtk_widget_hide(toolbar_get_widget(window->viking_vtb));
					GtkWidget *check_box = gtk_ui_manager_get_widget(window->uim, "/ui/MainMenu/View/SetShow/ViewToolBar");
					gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_box), false);
				}
			}

			bool menubar;
			if (a_settings_get_boolean(VIK_SETTINGS_WIN_MENUBAR, &menubar)) {
				if (! menubar) {
					gtk_widget_hide(gtk_ui_manager_get_widget(window->uim, "/ui/MainMenu"));
					GtkWidget *check_box = gtk_ui_manager_get_widget(window->uim, "/ui/MainMenu/View/SetShow/ViewMainMenu");
					gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_box), false);
				}
			}
		}
		window_count++;

		return window;
	}
	return NULL;
}

/**
 * determine_location_thread:
 * @window:     The window that will get updated
 * @threaddata: Data used by our background thread mechanism
 *
 * Use the features in vikgoto to determine where we are
 * Then set up the viewport:
 *  1. To goto the location
 *  2. Set an appropriate level zoom for the location type
 *  3. Some statusbar message feedback
 */
static int determine_location_thread(Window * window, void * threaddata)
{
	struct LatLon ll;
	char * name = NULL;
	int ans = a_vik_goto_where_am_i(window->viewport, &ll, &name);

	int result = a_background_thread_progress(threaddata, 1.0);
	if (result != 0) {
		window->statusbar_update(_("Location lookup aborted"), VIK_STATUSBAR_INFO);
		return -1; /* Abort thread */
	}

	if (ans) {
		// Zoom out a little
		double zoom = 16.0;

		if (ans == 2) {
			// Position found with city precision - so zoom out more
			zoom = 128.0;
		} else if (ans == 3) {
			// Position found via country name search - so zoom wayyyy out
			zoom = 2048.0;
		}

		window->viewport->set_zoom(zoom);
		window->viewport->set_center_latlon(&ll, false);

		char * message = g_strdup_printf(_("Location found: %s"), name);
		window->statusbar_update(message, VIK_STATUSBAR_INFO);
		free(name);
		free(message);

		// Signal to redraw from the background
		window->layers_panel->emit_update();
	} else {
		window->statusbar_update(_("Unable to determine location"), VIK_STATUSBAR_INFO);
	}

	return 0;
}

/**
 * Steps to be taken once initial loading has completed
 */
void Window::finish_new()
{
	// Don't add a map if we've loaded a Viking file already
	if (this->filename) {
		return;
	}

	if (a_vik_get_startup_method() == VIK_STARTUP_METHOD_SPECIFIED_FILE) {
		this->open_file(a_vik_get_startup_file(), true);
		if (this->filename) {
			return;
		}
	}

	// Maybe add a default map layer
	if(a_vik_get_add_default_map_layer()) {
		LayerMaps * layer = new LayerMaps(this->viewport);
		layer->rename(_("Default Map"));

		this->layers_panel->get_top_layer()->add_layer(layer, true);

		this->draw_update();
	}

	// If not loaded any file, maybe try the location lookup
	if (this->loaded_type == LOAD_TYPE_READ_FAILURE) {
		if (a_vik_get_startup_method() == VIK_STARTUP_METHOD_AUTO_LOCATION) {

			vik_statusbar_set_message(this->viking_vs, VIK_STATUSBAR_INFO, _("Trying to determine location..."));

			a_background_thread(BACKGROUND_POOL_REMOTE,
					    GTK_WINDOW(vw),
					    _("Determining location"),
					    (vik_thr_func) determine_location_thread,
					    this,
					    NULL,
					    NULL,
					    1);
		}
	}
}

static void open_window(Window * window, GSList *files)
{
	if (!window) {
		return;
	}

	bool change_fn = (g_slist_length(files) == 1); /* only change fn if one file */
	GSList *cur_file = files;
	while (cur_file) {
		// Only open a new window if a viking file
		char *file_name = (char *) cur_file->data;
		if (window->filename && check_file_magic_vik(file_name)) {
			Window * new_window = Window::new_window();
			if (new_window) {
				new_window->open_file(file_name, true);
			}
		} else {
			window->open_file(file_name, change_fn);
		}
		free(file_name);
		cur_file = g_slist_next(cur_file);
	}
	g_slist_free(files);
}
// End signals

void Window::selected_layer(Layer * layer)
{
	if (!this->action_group) {
		return;
	}

	for (int type = VIK_LAYER_AGGREGATE; type < VIK_LAYER_NUM_TYPES; type++) {
		VikLayerInterface * layer_interface = vik_layer_get_interface((VikLayerTypeEnum) type);
		int tool_count = layer_interface->tools_count;

		for (int tool = 0; tool < tool_count; tool++) {
			GtkAction * action = gtk_action_group_get_action(this->action_group,
									 layer_interface->tools[tool].radioActionEntry.name);
			g_object_set(action, "sensitive", type == layer->type, NULL);
			toolbar_action_set_sensitive(this->viking_vtb, vik_layer_get_interface((VikLayerTypeEnum) type)->tools[tool].radioActionEntry.name, type == layer->type);
		}
	}
}

static void window_finalize(GObject * gob)
{
	VikWindow * vw = VIK_WINDOW(gob);
	if (!vw) {
		return;
	}

	G_OBJECT_CLASS(parent_class)->finalize(gob);
}


static void vik_window_class_init(VikWindowClass *klass)
{
	/* destructor */
	GObjectClass *object_class;

	window_signals[VW_NEWWINDOW_SIGNAL] = g_signal_new("newwindow", G_TYPE_FROM_CLASS (klass), (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION), G_STRUCT_OFFSET (VikWindowClass, newwindow), NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
	window_signals[VW_OPENWINDOW_SIGNAL] = g_signal_new("openwindow", G_TYPE_FROM_CLASS (klass), (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION), G_STRUCT_OFFSET (VikWindowClass, openwindow), NULL, NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = window_finalize;

	parent_class = (GObjectClass *) g_type_class_peek_parent(klass);

}

/* Menu View -> Zoom -> Value. */
static void zoom_changed_cb(GtkMenuShell * menushell, void * user_data)
{
	Window * window = (Window *) user_data;

	fprintf(stderr, "zoom changed\n");

	GtkWidget * aw = gtk_menu_get_active(GTK_MENU (menushell));
	int active = KPOINTER_TO_INT (g_object_get_data(G_OBJECT (aw), "position"));

	double zoom_request = pow(2, active-5);

	// But has it really changed?
	double current_zoom = window->viewport->get_zoom();
	if (current_zoom != 0.0 && zoom_request != current_zoom) {
		window->viewport->set_zoom(zoom_request);
		// Force drawing update
		window->draw_update();
	}
}

/**
 * @mpp: The initial zoom level
 */
static GtkWidget * create_zoom_menu_all_levels(double mpp)
{
	GtkWidget * menu = gtk_menu_new();
	char * itemLabels[] = { (char *) "0.031", (char *) "0.063", (char *) "0.125", (char *) "0.25", (char *) "0.5", (char *) "1", (char *) "2", (char *) "4", (char *) "8", (char *) "16", (char *) "32", (char *) "64", (char *) "128", (char *) "256", (char *) "512", (char *) "1024", (char *) "2048", (char *) "4096", (char *) "8192", (char *) "16384", (char *) "32768" };

	for (int i = 0 ; i < G_N_ELEMENTS(itemLabels) ; i++) {
		GtkWidget *item = gtk_menu_item_new_with_label(itemLabels[i]);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
		g_object_set_data(G_OBJECT (item), "position", KINT_TO_POINTER(i));
	}

	int active = 5 + round(log(mpp) / log(2));
	// Ensure value derived from mpp is in bounds of the menu
	if (active >= G_N_ELEMENTS(itemLabels)) {
		active = G_N_ELEMENTS(itemLabels) - 1;
	}
	if (active < 0) {
		active = 0;
	}
	gtk_menu_set_active(GTK_MENU(menu), active);

	return menu;
}

static GtkWidget * create_zoom_combo_all_levels()
{
	GtkWidget * combo = vik_combo_box_text_new();
	vik_combo_box_text_append(combo, "0.25");
	vik_combo_box_text_append(combo, "0.5");
	vik_combo_box_text_append(combo, "1");
	vik_combo_box_text_append(combo, "2");
	vik_combo_box_text_append(combo, "4");
	vik_combo_box_text_append(combo, "8");
	vik_combo_box_text_append(combo, "16");
	vik_combo_box_text_append(combo, "32");
	vik_combo_box_text_append(combo, "64");
	vik_combo_box_text_append(combo, "128");
	vik_combo_box_text_append(combo, "256");
	vik_combo_box_text_append(combo, "512");
	vik_combo_box_text_append(combo, "1024");
	vik_combo_box_text_append(combo, "2048");
	vik_combo_box_text_append(combo, "4096");
	vik_combo_box_text_append(combo, "8192");
	vik_combo_box_text_append(combo, "16384");
	vik_combo_box_text_append(combo, "32768");
	/* Create tooltip */
	gtk_widget_set_tooltip_text(combo, _("Select zoom level"));
	return combo;
}

static int zoom_popup_handler(GtkWidget * widget)
{
	if (!widget) {
		return false;
	}

	if (!GTK_IS_MENU (widget)) {
		return false;
	}

	/* The "widget" is the menu that was supplied when
	 * g_signal_connect_swapped() was called.
	 */
	GtkMenu * menu = GTK_MENU (widget);

	gtk_menu_popup(menu, NULL, NULL, NULL, NULL,
		       1, gtk_get_current_event_time());
	return true;
}

enum {
	TARGET_URIS,
};

static void drag_data_received_cb(GtkWidget * widget,
				  GdkDragContext *context,
				  int x,
				  int y,
				  GtkSelectionData * selection_data,
				  unsigned int target_type,
				  unsigned int time,
				  void * data)
{
	bool success = false;

	if ((selection_data != NULL) && (gtk_selection_data_get_length(selection_data) > 0)) {
		switch (target_type) {
		case TARGET_URIS: {
			char * str = (char *) gtk_selection_data_get_data(selection_data);
			fprintf(stderr, "DEBUG: drag received string:%s \n", str);

			// Convert string into GSList of individual entries for use with our open signal
			char ** entries = g_strsplit(str, "\r\n", 0);
			GSList * filenames = NULL;
			int entry_runner = 0;
			char * entry = entries[entry_runner];
			while (entry) {
				if (strcmp(entry, "")) {
					// Drag+Drop gives URIs. And so in particular, %20 in place of spaces in filenames
					//  thus need to convert the text into a plain string
					char *filename = g_filename_from_uri(entry, NULL, NULL);
					if (filename) {
						filenames = g_slist_append(filenames, filename);
					}
				}
				entry_runner++;
				entry = entries[entry_runner];
			}

			if (filenames) {
				g_signal_emit(G_OBJECT(VIK_WINDOW_FROM_WIDGET(widget)), window_signals[VW_OPENWINDOW_SIGNAL], 0, filenames);
			// NB: GSList & contents are freed by main.open_window
			}

			success = true;
			break;
		}
		default: break;
		}
	}

	gtk_drag_finish(context, success, false, time);
}

static void toolbar_tool_cb(GtkAction *old, GtkAction *current, void * gp)
{
	Window * window = (Window *) gp;

	GtkAction * action = gtk_action_group_get_action(window->action_group, gtk_action_get_name(current));
	if (action) {
		gtk_action_activate(action);
	}
}

static void toolbar_reload_cb(GtkActionGroup *grp, void * gp)
{
	Window * window = (Window *) gp;

	center_changed_cb(window);
}

#define VIK_SETTINGS_WIN_MAX "window_maximized"
#define VIK_SETTINGS_WIN_FULLSCREEN "window_fullscreen"
#define VIK_SETTINGS_WIN_WIDTH "window_width"
#define VIK_SETTINGS_WIN_HEIGHT "window_height"
#define VIK_SETTINGS_WIN_PANE_POSITION "window_horizontal_pane_position"
#define VIK_SETTINGS_WIN_SAVE_IMAGE_WIDTH "window_save_image_width"
#define VIK_SETTINGS_WIN_SAVE_IMAGE_HEIGHT "window_save_image_height"
#define VIK_SETTINGS_WIN_SAVE_IMAGE_PNG "window_save_image_as_png"
#define VIK_SETTINGS_WIN_COPY_CENTRE_FULL_FORMAT "window_copy_centre_full_format"

#define VIKING_ACCELERATOR_KEY_FILE "keys.rc"

static void vik_window_init(VikWindow * vw)
{
}

/**
 * Update the displayed map
 *  Only update the top most visible map layer
 *  ATM this assumes (as per defaults) the top most map has full alpha setting
 *   such that other other maps even though they may be active will not be seen
 *  It's more complicated to work out which maps are actually visible due to alpha settings
 *   and overkill for this simple refresh method.
 */
void Window::simple_map_update(bool only_new)
{
	// Find the most relevent single map layer to operate on
	Layer * layer = this->layers_panel->get_top_layer()->get_top_visible_layer_of_type(VIK_LAYER_MAPS);
	if (layer) {
		vik_maps_layer_download(layer->vl, this->viewport, only_new);
	}
}

/**
 * Used to handle keys pressed in main UI, e.g. as hotkeys.
 *
 * This is the global key press handler
 *  Global shortcuts are available at any time and hence are not restricted to when a certain tool is enabled
 */
static bool key_press_event_cb(Window * window, GdkEventKey * event, void * data)
{
	// The keys handled here are not in the menuing system for a couple of reasons:
	//  . Keeps the menu size compact (alebit at expense of discoverably)
	//  . Allows differing key bindings to perform the same actions

	// First decide if key events are related to the maps layer
	bool map_download = false;
	bool map_download_only_new = true; // Only new or reload

	GdkModifierType modifiers = (GdkModifierType) gtk_accelerator_get_default_mod_mask();

	// Standard 'Refresh' keys: F5 or Ctrl+r
	// Note 'F5' is actually handled via draw_refresh_cb() later on
	//  (not 'R' it's 'r' notice the case difference!!)
	if (event->keyval == GDK_r && (event->state & modifiers) == GDK_CONTROL_MASK) {
		map_download = true;
		map_download_only_new = true;
	}
	// Full cache reload with Ctrl+F5 or Ctrl+Shift+r [This is not in the menu system]
	// Note the use of uppercase R here since shift key has been pressed
	else if ((event->keyval == GDK_F5 && (event->state & modifiers) == GDK_CONTROL_MASK) ||
		 (event->keyval == GDK_R && (event->state & modifiers) == (GDK_CONTROL_MASK + GDK_SHIFT_MASK))) {
		map_download = true;
		map_download_only_new = false;
	}
	// Standard Ctrl+KP+ / Ctrl+KP- to zoom in/out respectively
	else if (event->keyval == GDK_KEY_KP_Add && (event->state & modifiers) == GDK_CONTROL_MASK) {
		window->viewport->zoom_in();
		window->draw_update();
		return true; // handled keypress
	}
	else if (event->keyval == GDK_KEY_KP_Subtract && (event->state & modifiers) == GDK_CONTROL_MASK) {
		window->viewport->zoom_out();
		window->draw_update();
		return true; // handled keypress
	}

	if (map_download) {
		window->simple_map_update(map_download_only_new);
		return true; // handled keypress
	}

	Layer * layer = window->layers_panel->get_selected();
	if (layer && window->vt->active_tool != -1 && window->vt->tools[window->vt->active_tool].ti.key_press) {
		int ltype = window->vt->tools[window->vt->active_tool].layer_type;
		if (layer && ltype == layer->type) {
			return window->vt->tools[window->vt->active_tool].ti.key_press(layer, event, window->vt->tools[window->vt->active_tool].state);
		}
	}

	// Ensure called only on window tools (i.e. not on any of the Layer tools since the layer is NULL)
	if (window->current_tool < TOOL_LAYER) {
		// No layer - but enable window tool keypress processing - these should be able to handle a NULL layer
		if (window->vt->tools[window->vt->active_tool].ti.key_press) {
			return window->vt->tools[window->vt->active_tool].ti.key_press(layer, event, window->vt->tools[window->vt->active_tool].state);
		}
	}

	/* Restore Main Menu via Escape key if the user has hidden it */
	/* This key is more likely to be used as they may not remember the function key */
	if (event->keyval == GDK_Escape) {
		GtkWidget *check_box = gtk_ui_manager_get_widget(window->uim, "/ui/MainMenu/View/SetShow/ViewMainMenu");
		if (check_box) {
			bool state = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(check_box));
			if (!state) {
				gtk_widget_show(gtk_ui_manager_get_widget(window->uim, "/ui/MainMenu"));
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_box), true);
				return true; /* handled keypress */
			}
		}
	}

	return false; /* don't handle the keypress */
}

static bool delete_event(VikWindow * vw)
{
	Window * window = vw->window;

#ifdef VIKING_PROMPT_IF_MODIFIED
	if (window->modified)
#else
	if (0)
#endif
	{
		GtkDialog *dia;
		dia = GTK_DIALOG (gtk_message_dialog_new(GTK_WINDOW(vw), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
							 _("Do you want to save the changes you made to the document \"%s\"?\n"
							   "\n"
							   "Your changes will be lost if you don't save them."),
							 window->get_filename()));
		gtk_dialog_add_buttons(dia, _("Don't Save"), GTK_RESPONSE_NO, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_YES, NULL);
		switch (gtk_dialog_run(dia)) {
		case GTK_RESPONSE_NO:
			gtk_widget_destroy(GTK_WIDGET(dia));
			return false;
		case GTK_RESPONSE_CANCEL:
			gtk_widget_destroy(GTK_WIDGET(dia));
			return true;
		default:
			gtk_widget_destroy(GTK_WIDGET(dia));
			return !save_file(NULL, vw->window);
		}
	}

	if (window_count == 1) {
		// On the final window close - save latest state - if it's wanted...
		if (a_vik_get_restore_window_state()) {
			int state = gdk_window_get_state(GTK_WIDGET(vw)->window);
			bool state_max = state & GDK_WINDOW_STATE_MAXIMIZED;
			a_settings_set_boolean(VIK_SETTINGS_WIN_MAX, state_max);

			bool state_fullscreen = state & GDK_WINDOW_STATE_FULLSCREEN;
			a_settings_set_boolean(VIK_SETTINGS_WIN_FULLSCREEN, state_fullscreen);

			a_settings_set_boolean(VIK_SETTINGS_WIN_SIDEPANEL, GTK_WIDGET_VISIBLE (GTK_WIDGET(window->layers_panel->gob)));

			a_settings_set_boolean(VIK_SETTINGS_WIN_STATUSBAR, GTK_WIDGET_VISIBLE (GTK_WIDGET(window->viking_vs)));

			a_settings_set_boolean(VIK_SETTINGS_WIN_TOOLBAR, GTK_WIDGET_VISIBLE (toolbar_get_widget(window->viking_vtb)));

			// If supersized - no need to save the enlarged width+height values
			if (! (state_fullscreen || state_max)) {
				int width, height;
				gtk_window_get_size(GTK_WINDOW (vw), &width, &height);
				a_settings_set_integer(VIK_SETTINGS_WIN_WIDTH, width);
				a_settings_set_integer(VIK_SETTINGS_WIN_HEIGHT, height);
			}

			a_settings_set_integer(VIK_SETTINGS_WIN_PANE_POSITION, gtk_paned_get_position(GTK_PANED(window->hpaned)));
		}

		a_settings_set_integer(VIK_SETTINGS_WIN_SAVE_IMAGE_WIDTH, window->draw_image_width);
		a_settings_set_integer(VIK_SETTINGS_WIN_SAVE_IMAGE_HEIGHT, window->draw_image_height);
		a_settings_set_boolean(VIK_SETTINGS_WIN_SAVE_IMAGE_PNG, window->draw_image_save_as_png);

		char *accel_file_name = g_build_filename(a_get_viking_dir(), VIKING_ACCELERATOR_KEY_FILE, NULL);
		gtk_accel_map_save(accel_file_name);
		free(accel_file_name);
	}

	return false;
}

/* Drawing stuff */
static void newwindow_cb(GtkAction *a, Window * window)
{
	g_signal_emit(G_OBJECT(window->vw), window_signals[VW_NEWWINDOW_SIGNAL], 0);
}

static void draw_update_cb(Window * window)
{
	window->draw_update();
}

void Window::draw_update()
{
	this->draw_redraw();
	this->draw_sync();
}

static void draw_sync_cb(Window * window)
{
	window->draw_sync();
}

void Window::draw_sync()
{
	this->viewport->sync();
	this->draw_status();
}

/*
 * Split the status update, as sometimes only need to update the tool part
 *  also on initialization the zoom related stuff is not ready to be used
 */
static void draw_status_tool(Window * window)
{
	if (window->current_tool == TOOL_LAYER) {
		// Use tooltip rather than the internal name as the tooltip is i8n
		vik_statusbar_set_message(window->viking_vs, VIK_STATUSBAR_TOOL, vik_layer_get_interface((VikLayerTypeEnum) window->tool_layer_id)->tools[window->tool_tool_id].radioActionEntry.tooltip);
	} else {
		vik_statusbar_set_message(window->viking_vs, VIK_STATUSBAR_TOOL, _(tool_names[window->current_tool]));
	}
}

void Window::draw_status()
{
	static char zoom_level[22];
	double xmpp = this->viewport->get_xmpp();
	double ympp = this->viewport->get_ympp();
	char *unit = this->viewport->get_coord_mode() == VIK_COORD_UTM ? _("mpp") : _("pixelfact");
	if (xmpp != ympp) {
		snprintf(zoom_level, 22, "%.3f/%.3f %s", xmpp, ympp, unit);
	} else {
		if ((int)xmpp - xmpp < 0.0) {
			snprintf(zoom_level, 22, "%.3f %s", xmpp, unit);
		} else {
			/* xmpp should be a whole number so don't show useless .000 bit */
			snprintf(zoom_level, 22, "%d %s", (int)xmpp, unit);
		}
	}

	vik_statusbar_set_message(this->viking_vs, VIK_STATUSBAR_ZOOM, zoom_level);

	draw_status_tool(this);
}

void Window::set_redraw_trigger(Layer * layer)
{
	Window * window = window_from_layer(layer);
	if (window) {
		window->trigger = layer->vl;
	}
}

static void window_configure_event(Window * window)
{
	static int first = 1;
	window->draw_redraw();
	if (first) {
		// This is a hack to set the cursor corresponding to the first tool
		// FIXME find the correct way to initialize both tool and its cursor
		first = 0;
		window->viewport_cursor = (GdkCursor *)toolbox_get_cursor(window->vt, "Pan");
		/* We set cursor, even if it is NULL: it resets to default */
		gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(window->viewport->vvp)), window->viewport_cursor);
	}
}

void Window::draw_redraw()
{
	VikCoord old_center = this->trigger_center;
	this->trigger_center = *(this->viewport->get_center());
	VikLayer *new_trigger = this->trigger;
	this->trigger = NULL;
	VikLayer *old_trigger = (VikLayer *) this->viewport->get_trigger();

	if (!new_trigger) {
		; /* do nothing -- have to redraw everything. */
	} else if ((old_trigger != new_trigger) || !vik_coord_equals(&old_center, &this->trigger_center) || (((Layer *) new_trigger->layer)->type == VIK_LAYER_AGGREGATE)) {
		this->viewport->set_trigger(new_trigger); /* todo: set to half_drawn mode if new trigger is above old */
	} else {
		this->viewport->set_half_drawn(true);
	}

	/* actually draw */
	this->viewport->clear();
	// Main layer drawing
	this->layers_panel->draw_all();
	// Draw highlight (possibly again but ensures it is on top - especially for when tracks overlap)
	if (this->viewport->get_draw_highlight()) {
		if (this->containing_trw && (this->selected_tracks || this->selected_waypoints)) {
			this->containing_trw->draw_highlight_items(this->selected_tracks, this->selected_waypoints, this->viewport);

		} else if (this->containing_trw && (this->selected_track || this->selected_waypoint)) {
			this->containing_trw->draw_highlight_item((Track *) this->selected_track, this->selected_waypoint, this->viewport);

		} else if (this->selected_trw) {
			this->selected_trw->draw_highlight(this->viewport);
		}
	}
	// Other viewport decoration items on top if they are enabled/in use
	this->viewport->draw_scale();
	this->viewport->draw_copyright();
	this->viewport->draw_centermark();
	this->viewport->draw_logo();

	this->viewport->set_half_drawn(false); /* just in case. */
}

bool draw_buf_done = true;

typedef struct {
	GdkDrawable * gdk_window;
	GdkGC * gdk_style;
	GdkPixmap * pixmap;
} draw_buf_data_t;

static int draw_buf(draw_buf_data_t * data)
{
	gdk_threads_enter();
	gdk_draw_drawable(data->gdk_window, data->gdk_style, data->pixmap,
			  0, 0, 0, 0, -1, -1);
	draw_buf_done = true;
	gdk_threads_leave();
	return false;
}


/* Mouse event handlers ************************************************************************/

static void vik_window_pan_click(Window * window, GdkEventButton * event)
{
	/* set panning origin */
	window->pan_move = false;
	window->pan_x = (int) event->x;
	window->pan_y = (int) event->y;
}

static void draw_click_cb(Window * window, GdkEventButton * event)
{
	gtk_widget_grab_focus(GTK_WIDGET(window->viewport->vvp));

	/* middle button pressed.  we reserve all middle button and scroll events
	 * for panning and zooming; tools only get left/right/movement
	 */
	if (event->button == 2) {
		if (window->vt->tools[window->vt->active_tool].ti.pan_handler) {
			// Tool still may need to do something (such as disable something)
			toolbox_click(window->vt, event);
		}
		vik_window_pan_click(window, event);
	} else {
		toolbox_click(window->vt, event);
	}
}

static void vik_window_pan_move(Window * window, GdkEventMotion * event)
{
	if (window->pan_x != -1) {
		window->viewport->set_center_screen(window->viewport->get_width()/2 - event->x + window->pan_x,
						    window->viewport->get_height()/2 - event->y + window->pan_y);
		window->pan_move = true;
		window->pan_x = event->x;
		window->pan_y = event->y;
		window->draw_update();
	}
}

/**
 * Utility function to get positional strings for the given location
 * lat and lon strings will get allocated and so need to be freed after use
 */
static void get_location_strings(Window * window, struct UTM utm, char **lat, char **lon)
{
	if (window->viewport->get_drawmode() == VIK_VIEWPORT_DRAWMODE_UTM) {
		// Reuse lat for the first part (Zone + N or S, and lon for the second part (easting and northing) of a UTM format:
		//  ZONE[N|S] EASTING NORTHING
		*lat = (char *) malloc(4*sizeof(char));
		// NB zone is stored in a char but is an actual number
		snprintf(*lat, 4, "%d%c", utm.zone, utm.letter);
		*lon = (char *) malloc(16*sizeof(char));
		snprintf(*lon, 16, "%d %d", (int)utm.easting, (int)utm.northing);
	} else {
		struct LatLon ll;
		a_coords_utm_to_latlon(&utm, &ll);
		a_coords_latlon_to_string(&ll, lat, lon);
	}
}

static void draw_mouse_motion_cb(Window * window, GdkEventMotion * event)
{
	static VikCoord coord;
	static struct UTM utm;
#define BUFFER_SIZE 50
	static char pointer_buf[BUFFER_SIZE];
	char *lat = NULL, *lon = NULL;
	int16_t alt;
	double zoom;
	VikDemInterpol interpol_method;

	/* This is a hack, but work far the best, at least for single pointer systems.
	 * See http://bugzilla.gnome.org/show_bug.cgi?id=587714 for more. */
	int x, y;
	gdk_window_get_pointer(event->window, &x, &y, NULL);
	event->x = x;
	event->y = y;

	toolbox_move(window->vt, event);

	window->viewport->screen_to_coord(event->x, event->y, &coord);
	vik_coord_to_utm(&coord, &utm);

	get_location_strings(window, utm, &lat, &lon);

	/* Change interpolate method according to scale */
	zoom = window->viewport->get_zoom();
	if (zoom > 2.0) {
		interpol_method = VIK_DEM_INTERPOL_NONE;
	} else if (zoom >= 1.0) {
		interpol_method = VIK_DEM_INTERPOL_SIMPLE;
	} else {
		interpol_method = VIK_DEM_INTERPOL_BEST;
	}


	if ((alt = a_dems_get_elev_by_coord(&coord, interpol_method)) != VIK_DEM_INVALID_ELEVATION) {
		if (a_vik_get_units_height() == VIK_UNITS_HEIGHT_METRES) {
			snprintf(pointer_buf, BUFFER_SIZE, _("%s %s %dm"), lat, lon, alt);
		} else {
			snprintf(pointer_buf, BUFFER_SIZE, _("%s %s %dft"), lat, lon, (int)VIK_METERS_TO_FEET(alt));
		}
	} else {
		snprintf(pointer_buf, BUFFER_SIZE, _("%s %s"), lat, lon);
	}
	free(lat);
	lat = NULL;
	free(lon);
	lon = NULL;
	vik_statusbar_set_message(window->viking_vs, VIK_STATUSBAR_POSITION, pointer_buf);

	vik_window_pan_move(window, event);

	/* This is recommended by the GTK+ documentation, but does not work properly.
	 * Use deprecated way until GTK+ gets a solution for correct motion hint handling:
	 * http://bugzilla.gnome.org/show_bug.cgi?id=587714
	 */
	/* gdk_event_request_motions (event); */
}

/**
 * Action the single click after a small timeout
 * If a double click has occurred then this will do nothing
 */
static bool vik_window_pan_timeout(Window * window)
{
	if (!window->single_click_pending) {
		// Double click happened, so don't do anything
		return false;
	}

	/* set panning origin */
	window->pan_move = false;
	window->single_click_pending = false;
	window->viewport->set_center_screen(window->delayed_pan_x, window->delayed_pan_y);
	window->draw_update();

	// Really turn off the pan moving!!
	window->pan_x = window->pan_y = -1;
	return false;
}

static void vik_window_pan_release(Window * window, GdkEventButton * event)
{
	bool do_draw = true;

	if (window->pan_move == false) {
		window->single_click_pending = !window->single_click_pending;

		if (window->single_click_pending) {
			// Store offset to use
			window->delayed_pan_x = window->pan_x;
			window->delayed_pan_y = window->pan_y;
			// Get double click time
			GtkSettings *gs = gtk_widget_get_settings(GTK_WIDGET(window->vw));
			GValue dct = { 0 }; // = G_VALUE_INIT; // GLIB 2.30+ only
			g_value_init(&dct, G_TYPE_INT);
			g_object_get_property(G_OBJECT(gs), "gtk-double-click-time", &dct);
			// Give chance for a double click to occur
			int timer = g_value_get_int(&dct) + 50;
			g_timeout_add(timer, (GSourceFunc)vik_window_pan_timeout, window);
			do_draw = false;
		} else {
			window->viewport->set_center_screen(window->pan_x, window->pan_y);
		}
	} else {
		window->viewport->set_center_screen(window->viewport->get_width()/2 - event->x + window->pan_x,
						    window->viewport->get_height()/2 - event->y + window->pan_y);
	}

	window->pan_move = false;
	window->pan_x = window->pan_y = -1;
	if (do_draw) {
		window->draw_update();
	}
}

static void draw_release_cb(Window * window, GdkEventButton * event)
{
	gtk_widget_grab_focus(GTK_WIDGET(window->viewport->vvp));

	if (event->button == 2) {  /* move / pan */
		if (window->vt->tools[window->vt->active_tool].ti.pan_handler) {
			// Tool still may need to do something (such as reenable something)
			toolbox_release(window->vt, event);
		}
		vik_window_pan_release(window, event);
	} else {
		toolbox_release(window->vt, event);
	}
}

static void draw_scroll_cb(Window * window, GdkEventScroll * event)
{
	window->draw_scroll(event);
}

void Window::draw_scroll(GdkEventScroll * event)
{
	unsigned int modifiers = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK);

	int width = this->viewport->get_width();
	int height = this->viewport->get_height();

	if (modifiers == GDK_CONTROL_MASK) {
		/* control == pan up & down */
		if (event->direction == GDK_SCROLL_UP) {
			this->viewport->set_center_screen(width/2, height/3);
		} else {
			this->viewport->set_center_screen(width/2, height*2/3);
		}
	} else if (modifiers == GDK_SHIFT_MASK) {
		/* shift == pan left & right */
		if (event->direction == GDK_SCROLL_UP) {
			this->viewport->set_center_screen(width/3, height/2);
		} else {
			this->viewport->set_center_screen(width*2/3, height/2);
		}
	} else if (modifiers == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) {
		// This zoom is on the center position
		if (event->direction == GDK_SCROLL_UP) {
			this->viewport->zoom_in();
		} else {
			this->viewport->zoom_out();
		}
	} else {
		/* make sure mouse is still over the same point on the map when we zoom */
		VikCoord coord;
		int x, y;
		int center_x = width / 2;
		int center_y = height / 2;
		this->viewport->screen_to_coord(event->x, event->y, &coord);
		if (event->direction == GDK_SCROLL_UP) {
			this->viewport->zoom_in();
		} else {
			this->viewport->zoom_out();
		}
		this->viewport->coord_to_screen(&coord, &x, &y);
		this->viewport->set_center_screen(center_x + (x - event->x),
						  center_y + (y - event->y));
	}

	this->draw_update();
}



/********************************************************************************
 ** Ruler tool code
 ********************************************************************************/
static void draw_ruler(Viewport * viewport, GdkDrawable *d, GdkGC *gc, int x1, int y1, int x2, int y2, double distance)
{
	PangoLayout *pl;
	char str[128];
	GdkGC *labgc = viewport->new_gc("#cccccc", 1);
	GdkGC *thickgc = gdk_gc_new(d);

	double len = sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2));
	double dx = (x2-x1)/len*10;
	double dy = (y2-y1)/len*10;
	double c = cos(DEG2RAD(15.0));
	double s = sin(DEG2RAD(15.0));
	double angle;
	double baseangle = 0;

	/* draw line with arrow ends */
	{
		int tmp_x1=x1, tmp_y1=y1, tmp_x2=x2, tmp_y2=y2;
		Viewport::clip_line(&tmp_x1, &tmp_y1, &tmp_x2, &tmp_y2);
		gdk_draw_line(d, gc, tmp_x1, tmp_y1, tmp_x2, tmp_y2);
	}

	Viewport::clip_line(&x1, &y1, &x2, &y2);
	gdk_draw_line(d, gc, x1, y1, x2, y2);

	gdk_draw_line(d, gc, x1 - dy, y1 + dx, x1 + dy, y1 - dx);
	gdk_draw_line(d, gc, x2 - dy, y2 + dx, x2 + dy, y2 - dx);
	gdk_draw_line(d, gc, x2, y2, x2 - (dx * c + dy * s), y2 - (dy * c - dx * s));
	gdk_draw_line(d, gc, x2, y2, x2 - (dx * c - dy * s), y2 - (dy * c + dx * s));
	gdk_draw_line(d, gc, x1, y1, x1 + (dx * c + dy * s), y1 + (dy * c - dx * s));
	gdk_draw_line(d, gc, x1, y1, x1 + (dx * c - dy * s), y1 + (dy * c + dx * s));

	/* draw compass */
#define CR 80
#define CW 4

	viewport->compute_bearing(x1, y1, x2, y2, &angle, &baseangle);

	{
		GdkColor color;
		gdk_gc_copy(thickgc, gc);
		gdk_gc_set_line_attributes(thickgc, CW, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER);
		gdk_color_parse("#2255cc", &color);
		gdk_gc_set_rgb_fg_color(thickgc, &color);
	}
	gdk_draw_arc(d, thickgc, false, x1-CR+CW/2, y1-CR+CW/2, 2*CR-CW, 2*CR-CW, (90 - RAD2DEG(baseangle))*64, -RAD2DEG(angle)*64);


	gdk_gc_copy(thickgc, gc);
	gdk_gc_set_line_attributes(thickgc, 2, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER);
	for (int i = 0; i < 180; i++) {
		c = cos(DEG2RAD(i)*2 + baseangle);
		s = sin(DEG2RAD(i)*2 + baseangle);

		if (i % 5) {
			gdk_draw_line(d, gc, x1 + CR*c, y1 + CR*s, x1 + (CR+CW)*c, y1 + (CR+CW)*s);
		} else {
			double ticksize = 2*CW;
			gdk_draw_line(d, thickgc, x1 + (CR-CW)*c, y1 + (CR-CW)*s, x1 + (CR+ticksize)*c, y1 + (CR+ticksize)*s);
		}
	}

	gdk_draw_arc(d, gc, false, x1-CR, y1-CR, 2*CR, 2*CR, 0, 64*360);
	gdk_draw_arc(d, gc, false, x1-CR-CW, y1-CR-CW, 2*(CR+CW), 2*(CR+CW), 0, 64*360);
	gdk_draw_arc(d, gc, false, x1-CR+CW, y1-CR+CW, 2*(CR-CW), 2*(CR-CW), 0, 64*360);
	c = (CR+CW*2)*cos(baseangle);
	s = (CR+CW*2)*sin(baseangle);
	gdk_draw_line(d, gc, x1-c, y1-s, x1+c, y1+s);
	gdk_draw_line(d, gc, x1+s, y1-c, x1-s, y1+c);

	/* draw labels */
#define LABEL(x, y, w, h) {					    \
		gdk_draw_rectangle(d, labgc, true, (x)-2, (y)-1, (w)+4, (h)+1); \
		gdk_draw_rectangle(d, gc, false, (x)-2, (y)-1, (w)+4, (h)+1); \
		gdk_draw_layout(d, gc, (x), (y), pl); }

	{
		int wd, hd, xd, yd;
		int wb, hb, xb, yb;

		pl = gtk_widget_create_pango_layout(GTK_WIDGET(viewport->vvp), NULL);
		pango_layout_set_font_description(pl, gtk_widget_get_style(GTK_WIDGET(viewport->vvp))->font_desc);
		pango_layout_set_text(pl, "N", -1);
		gdk_draw_layout(d, gc, x1-5, y1-CR-3*CW-8, pl);

		/* draw label with distance */
		vik_units_distance_t dist_units = a_vik_get_units_distance();
		switch (dist_units) {
		case VIK_UNITS_DISTANCE_KILOMETRES:
			if (distance >= 1000 && distance < 100000) {
				g_sprintf(str, "%3.2f km", distance/1000.0);
			} else if (distance < 1000) {
				g_sprintf(str, "%d m", (int)distance);
			} else {
				g_sprintf(str, "%d km", (int)distance/1000);
			}
			break;
		case VIK_UNITS_DISTANCE_MILES:
			if (distance >= VIK_MILES_TO_METERS(1) && distance < VIK_MILES_TO_METERS(100)) {
				g_sprintf(str, "%3.2f miles", VIK_METERS_TO_MILES(distance));
			} else if (distance < VIK_MILES_TO_METERS(1)) {
				g_sprintf(str, "%d yards", (int)(distance*1.0936133));
			} else {
				g_sprintf(str, "%d miles", (int)VIK_METERS_TO_MILES(distance));
			}
			break;
		case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
			if (distance >= VIK_NAUTICAL_MILES_TO_METERS(1) && distance < VIK_NAUTICAL_MILES_TO_METERS(100)) {
				g_sprintf(str, "%3.2f NM", VIK_METERS_TO_NAUTICAL_MILES(distance));
			} else if (distance < VIK_NAUTICAL_MILES_TO_METERS(1)) {
				g_sprintf(str, "%d yards", (int)(distance*1.0936133));
			} else {
				g_sprintf(str, "%d NM", (int)VIK_METERS_TO_NAUTICAL_MILES(distance));
			}
			break;
		default:
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. distance=%d\n", dist_units);
		}

		pango_layout_set_text(pl, str, -1);

		pango_layout_get_pixel_size(pl, &wd, &hd);
		if (dy>0) {
			xd = (x1+x2)/2 + dy;
			yd = (y1+y2)/2 - hd/2 - dx;
		} else {
			xd = (x1+x2)/2 - dy;
			yd = (y1+y2)/2 - hd/2 + dx;
		}

		if (xd < -5 || yd < -5 || xd > viewport->get_width() + 5 || yd > viewport->get_height() + 5) {
			xd = x2 + 10;
			yd = y2 - 5;
		}

		LABEL(xd, yd, wd, hd);

		/* draw label with bearing */
		g_sprintf(str, "%3.1f°", RAD2DEG(angle));
		pango_layout_set_text(pl, str, -1);
		pango_layout_get_pixel_size(pl, &wb, &hb);
		xb = x1 + CR*cos(angle-M_PI_2);
		yb = y1 + CR*sin(angle-M_PI_2);

		if (xb < -5 || yb < -5 || xb > viewport->get_width() + 5 || yb > viewport->get_height() + 5) {
			xb = x2 + 10;
			yb = y2 + 10;
		}

		{
			GdkRectangle r1 = {xd-2, yd-1, wd+4, hd+1}, r2 = {xb-2, yb-1, wb+4, hb+1};
			if (gdk_rectangle_intersect(&r1, &r2, &r2)) {
				xb = xd + wd + 5;
			}
		}
		LABEL(xb, yb, wb, hb);
	}
#undef LABEL

	g_object_unref(G_OBJECT (pl));
	g_object_unref(G_OBJECT (labgc));
	g_object_unref(G_OBJECT (thickgc));
}

typedef struct {
	Window * window;
	Viewport * viewport;
	bool has_oldcoord;
	VikCoord oldcoord;
} ruler_tool_state_t;

static void * ruler_create(Window * window, Viewport * viewport)
{
	ruler_tool_state_t *s = (ruler_tool_state_t *) malloc(1 * sizeof (ruler_tool_state_t));
	s->window = window;
	s->viewport = viewport;
	fprintf(stderr, "%s:%d: %x\n", __FUNCTION__, __LINE__, s->viewport);
	s->has_oldcoord = false;
	return s;
}

static void ruler_destroy(ruler_tool_state_t *s)
{
	free(s);
}

static VikLayerToolFuncStatus ruler_click(Layer * layer, GdkEventButton * event, ruler_tool_state_t *s)
{
	struct LatLon ll;
	VikCoord coord;
	char *temp;
	if (event->button == 1) {
		char *lat = NULL, *lon = NULL;
		s->viewport->screen_to_coord((int) event->x, (int) event->y, &coord);
		vik_coord_to_latlon(&coord, &ll);
		a_coords_latlon_to_string(&ll, &lat, &lon);
		if (s->has_oldcoord) {
			vik_units_distance_t dist_units = a_vik_get_units_distance();
			switch (dist_units) {
			case VIK_UNITS_DISTANCE_KILOMETRES:
				temp = g_strdup_printf("%s %s DIFF %f meters", lat, lon, vik_coord_diff(&coord, &(s->oldcoord)));
				break;
			case VIK_UNITS_DISTANCE_MILES:
				temp = g_strdup_printf("%s %s DIFF %f miles", lat, lon, VIK_METERS_TO_MILES(vik_coord_diff(&coord, &(s->oldcoord))));
				break;
			case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
				temp = g_strdup_printf("%s %s DIFF %f NM", lat, lon, VIK_METERS_TO_NAUTICAL_MILES(vik_coord_diff(&coord, &(s->oldcoord))));
				break;
			default:
				temp = g_strdup_printf("Just to keep the compiler happy");
				fprintf(stderr, "CRITICAL: Houston, we've had a problem. distance=%d\n", dist_units);
			}

			s->has_oldcoord = false;
		} else {
			temp = g_strdup_printf("%s %s", lat, lon);
			s->has_oldcoord = true;
		}

		vik_statusbar_set_message(s->window->viking_vs, VIK_STATUSBAR_INFO, temp);
		free(temp);

		s->oldcoord = coord;
	} else {
		s->viewport->set_center_screen((int) event->x, (int) event->y);
		s->window->draw_update();
	}
	return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus ruler_move(Layer * layer, GdkEventMotion * event, ruler_tool_state_t * s)
{
	Window * window = s->window;

	struct LatLon ll;
	VikCoord coord;
	char *temp;

	if (s->has_oldcoord) {
		static GdkPixmap *buf = NULL;
		char *lat = NULL, *lon = NULL;
		int w1 = s->viewport->get_width();
		int h1 = s->viewport->get_height();
		if (!buf) {
			buf = gdk_pixmap_new(gtk_widget_get_window(GTK_WIDGET(s->viewport->vvp)), w1, h1, -1);
		}

		int w2, h2;
		gdk_drawable_get_size(buf, &w2, &h2);
		if (w1 != w2 || h1 != h2) {
			g_object_unref(G_OBJECT (buf));
			buf = gdk_pixmap_new(gtk_widget_get_window(GTK_WIDGET(s->viewport->vvp)), w1, h1, -1);
		}

		s->viewport->screen_to_coord((int) event->x, (int) event->y, &coord);
		vik_coord_to_latlon(&coord, &ll);
		int oldx, oldy;
		s->viewport->coord_to_screen(&s->oldcoord, &oldx, &oldy);

		gdk_draw_drawable(buf, gtk_widget_get_style(GTK_WIDGET(s->viewport->vvp))->black_gc,
				  s->viewport->get_pixmap(), 0, 0, 0, 0, -1, -1);
		draw_ruler(s->viewport, buf, gtk_widget_get_style(GTK_WIDGET(s->viewport->vvp))->black_gc, oldx, oldy, event->x, event->y, vik_coord_diff(&coord, &(s->oldcoord)));
		if (draw_buf_done) {
			static draw_buf_data_t pass_along;
			pass_along.gdk_window = gtk_widget_get_window(GTK_WIDGET(s->viewport->vvp));
			pass_along.gdk_style = gtk_widget_get_style(GTK_WIDGET(s->viewport->vvp))->black_gc;
			pass_along.pixmap = buf;
			g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, (GSourceFunc) draw_buf, (void *) &pass_along, NULL);
			draw_buf_done = false;
		}
		a_coords_latlon_to_string(&ll, &lat, &lon);
		vik_units_distance_t dist_units = a_vik_get_units_distance();
		switch (dist_units) {
		case VIK_UNITS_DISTANCE_KILOMETRES:
			temp = g_strdup_printf("%s %s DIFF %f meters", lat, lon, vik_coord_diff(&coord, &(s->oldcoord)));
			break;
		case VIK_UNITS_DISTANCE_MILES:
			temp = g_strdup_printf("%s %s DIFF %f miles", lat, lon, VIK_METERS_TO_MILES (vik_coord_diff(&coord, &(s->oldcoord))));
			break;
		case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
			temp = g_strdup_printf("%s %s DIFF %f NM", lat, lon, VIK_METERS_TO_NAUTICAL_MILES (vik_coord_diff(&coord, &(s->oldcoord))));
			break;
		default:
			temp = g_strdup_printf("Just to keep the compiler happy");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. distance=%d\n", dist_units);
		}
		vik_statusbar_set_message(window->viking_vs, VIK_STATUSBAR_INFO, temp);
		free(temp);
	}
	return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus ruler_release(Layer * layer, GdkEventButton * event, ruler_tool_state_t *s)
{
	return VIK_LAYER_TOOL_ACK;
}

static void ruler_deactivate(Layer * layer, ruler_tool_state_t *s)
{
	s->window->draw_update();
}

static bool ruler_key_press(Layer * layer, GdkEventKey *event, ruler_tool_state_t *s)
{
	if (event->keyval == GDK_Escape) {
		s->has_oldcoord = false;
		ruler_deactivate(layer, s);
		return true;
  }
	// Regardless of whether we used it, return false so other GTK things may use it
	return false;
}

static VikToolInterface ruler_tool =
	// NB Ctrl+Shift+R is used for Refresh (deemed more important), so use 'U' instead
	{ { "Ruler", "vik-icon-ruler", N_("_Ruler"), "<control><shift>U", N_("Ruler Tool"), 2 },
	  ruler_create, /* (VikToolConstructorFunc) */
	  (VikToolDestructorFunc) ruler_destroy,
	  (VikToolActivationFunc) NULL,
	  (VikToolActivationFunc) ruler_deactivate,
	  (VikToolMouseFunc) ruler_click,
	  (VikToolMouseMoveFunc) ruler_move,
	  (VikToolMouseFunc) ruler_release,
	  (VikToolKeyFunc) ruler_key_press,
	  false,
	  GDK_CURSOR_IS_PIXMAP,
	  &cursor_ruler_pixbuf,
	  NULL };
/*** end ruler code ********************************************************/



/********************************************************************************
 ** Zoom tool code
 ********************************************************************************/

typedef struct {
	Window * window;
	GdkPixmap *pixmap;
	// Track zoom bounds for zoom tool with shift modifier:
	bool bounds_active;
	int start_x;
	int start_y;
} zoom_tool_state_t;

/*
 * In case the screen size has changed
 */
static void zoomtool_resize_pixmap(zoom_tool_state_t *zts)
{
	// Allocate a drawing area the size of the viewport
	int w1 = zts->window->viewport->get_width();
	int h1 = zts->window->viewport->get_height();

	if (!zts->pixmap) {
		// Totally new
		zts->pixmap = gdk_pixmap_new(gtk_widget_get_window(GTK_WIDGET(zts->window->viewport->vvp)), w1, h1, -1);
	}

	int w2, h2;
	gdk_drawable_get_size(zts->pixmap, &w2, &h2);

	if (w1 != w2 || h1 != h2) {
		// Has changed - delete and recreate with new values
		g_object_unref(G_OBJECT (zts->pixmap));
		zts->pixmap = gdk_pixmap_new(gtk_widget_get_window(GTK_WIDGET(zts->window->viewport->vvp)), w1, h1, -1);
	}
}

static void * zoomtool_create(Window * window, Viewport * viewport)
{
	zoom_tool_state_t *zts = (zoom_tool_state_t *) malloc(1 * sizeof (zoom_tool_state_t));
	zts->window = window;
	zts->pixmap = NULL;
	zts->start_x = 0;
	zts->start_y = 0;
	zts->bounds_active = false;
	return zts;
}

static void zoomtool_destroy(zoom_tool_state_t *zts)
{
	if (zts->pixmap) {
		g_object_unref(G_OBJECT (zts->pixmap));
	}
	free(zts);
}

static VikLayerToolFuncStatus zoomtool_click(Layer * layer, GdkEventButton * event, zoom_tool_state_t *zts)
{
	zts->window->modified = true;
	unsigned int modifiers = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK);

	VikCoord coord;
	int center_x = zts->window->viewport->get_width() / 2;
	int center_y = zts->window->viewport->get_height() / 2;

	bool skip_update = false;

	zts->bounds_active = false;

	if (modifiers == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) {
		// This zoom is on the center position
		zts->window->viewport->set_center_screen(center_x, center_y);
		if (event->button == 1) {
			zts->window->viewport->zoom_in();
		} else if (event->button == 3) {
			zts->window->viewport->zoom_out();
		}
	} else if (modifiers == GDK_CONTROL_MASK) {
		// This zoom is to recenter on the mouse position
		zts->window->viewport->set_center_screen((int) event->x, (int) event->y);
		if (event->button == 1) {
			zts->window->viewport->zoom_in();
		} else if (event->button == 3) {
			zts->window->viewport->zoom_out();
		}
	} else if (modifiers == GDK_SHIFT_MASK) {
		// Get start of new zoom bounds
		if (event->button == 1) {
			zts->bounds_active = true;
			zts->start_x = (int) event->x;
			zts->start_y = (int) event->y;
			skip_update = true;
		}
	} else {
		/* make sure mouse is still over the same point on the map when we zoom */
		zts->window->viewport->screen_to_coord(event->x, event->y, &coord);
		if (event->button == 1) {
			zts->window->viewport->zoom_in();
		} else if (event->button == 3) {
			zts->window->viewport->zoom_out();
		}
		int x, y;
		zts->window->viewport->coord_to_screen(&coord, &x, &y);
		zts->window->viewport->set_center_screen(center_x + (x - event->x),
						     center_y + (y - event->y));
	}

	if (!skip_update) {
		zts->window->draw_update();
	}

	return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus zoomtool_move(Layer * layer, GdkEventMotion * event, zoom_tool_state_t *zts)
{
	unsigned int modifiers = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK);

	if (zts->bounds_active && modifiers == GDK_SHIFT_MASK) {
		zoomtool_resize_pixmap(zts);

		// Blank out currently drawn area
		gdk_draw_drawable(zts->pixmap,
				  gtk_widget_get_style(GTK_WIDGET(zts->window->viewport->vvp))->black_gc,
				  zts->window->viewport->get_pixmap(),
				  0, 0, 0, 0, -1, -1);

		// Calculate new box starting point & size in pixels
		int xx, yy, width, height;
		if (event->y > zts->start_y) {
			yy = zts->start_y;
			height = event->y-zts->start_y;
		} else {
			yy = event->y;
			height = zts->start_y-event->y;
		}
		if (event->x > zts->start_x) {
			xx = zts->start_x;
			width = event->x-zts->start_x;
		} else {
			xx = event->x;
			width = zts->start_x-event->x;
		}

		// Draw the box
		gdk_draw_rectangle(zts->pixmap, gtk_widget_get_style(GTK_WIDGET(zts->window->viewport->vvp))->black_gc, false, xx, yy, width, height);

		// Only actually draw when there's time to do so
		if (draw_buf_done) {
			static draw_buf_data_t pass_along;
			pass_along.gdk_window = gtk_widget_get_window(GTK_WIDGET(zts->window->viewport->vvp));
			pass_along.gdk_style = gtk_widget_get_style(GTK_WIDGET(zts->window->viewport->vvp))->black_gc;
			pass_along.pixmap = zts->pixmap;
			g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, (GSourceFunc) draw_buf, &pass_along, NULL);
			draw_buf_done = false;
		}
	} else {
		zts->bounds_active = false;
	}

	return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus zoomtool_release(Layer * layer, GdkEventButton * event, zoom_tool_state_t *zts)
{
	unsigned int modifiers = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK);

	// Ensure haven't just released on the exact same position
	//  i.e. probably haven't moved the mouse at all
	if (zts->bounds_active && modifiers == GDK_SHIFT_MASK &&
	    (event->x < zts->start_x-5 || event->x > zts->start_x+5) &&
	    (event->y < zts->start_y-5 || event->y > zts->start_y+5)) {

		VikCoord coord1, coord2;
		zts->window->viewport->screen_to_coord(zts->start_x, zts->start_y, &coord1);
		zts->window->viewport->screen_to_coord(event->x, event->y, &coord2);

		// From the extend of the bounds pick the best zoom level
		// c.f. trw_layer_zoom_to_show_latlons()
		// Maybe refactor...
		struct LatLon maxmin[2];
		vik_coord_to_latlon(&coord1, &maxmin[0]);
		vik_coord_to_latlon(&coord2, &maxmin[1]);

		vu_zoom_to_show_latlons_common(zts->window->viewport->get_coord_mode(), zts->window->viewport, maxmin, VIK_VIEWPORT_MIN_ZOOM, false);
	} else {
		// When pressing shift and clicking for zoom, then jump three levels
		if (modifiers == GDK_SHIFT_MASK) {
			// Zoom in/out by three if possible
			zts->window->viewport->set_center_screen(event->x, event->y);
			if (event->button == 1) {
				zts->window->viewport->zoom_in();
				zts->window->viewport->zoom_in();
				zts->window->viewport->zoom_in();
			} else if (event->button == 3) {
				zts->window->viewport->zoom_out();
				zts->window->viewport->zoom_out();
				zts->window->viewport->zoom_out();
			}
		}
	}

	zts->window->draw_update();

	// Reset
	zts->bounds_active = false;

	return VIK_LAYER_TOOL_ACK;
}





static VikToolInterface zoom_tool =
	{ { "Zoom", "vik-icon-zoom", N_("_Zoom"), "<control><shift>Z", N_("Zoom Tool"), 1 },
	  zoomtool_create, /* (VikToolConstructorFunc) */
	  (VikToolDestructorFunc) zoomtool_destroy,
	  (VikToolActivationFunc) NULL,
	  (VikToolActivationFunc) NULL,
	  (VikToolMouseFunc) zoomtool_click,
	  (VikToolMouseMoveFunc) zoomtool_move,
	  (VikToolMouseFunc) zoomtool_release,
	  NULL,
	  false,
	  GDK_CURSOR_IS_PIXMAP,
	  &cursor_zoom_pixbuf,
	  NULL };
/*** end zoom code ********************************************************/

/********************************************************************************
 ** Pan tool code
 ********************************************************************************/
static void * pantool_create(Window * window, Viewport * viewport)
{
	return window;
}

// NB Double clicking means this gets called THREE times!!!
static VikLayerToolFuncStatus pantool_click(Layer * layer, GdkEventButton * event, Window * window)
{
	window->modified = true;

	if (event->type == GDK_2BUTTON_PRESS) {
		// Zoom in / out on double click
		// No need to change the center as that has already occurred in the first click of a double click occurrence
		if (event->button == 1) {
			unsigned int modifier = event->state & GDK_SHIFT_MASK;
			if (modifier) {
				window->viewport->zoom_out();
			} else {
				window->viewport->zoom_in();
			}
		} else if (event->button == 3) {
			window->viewport->zoom_out();
		}

		window->draw_update();
	} else {
		// Standard pan click
		if (event->button == 1) {
			vik_window_pan_click(window, event);
		}
	}

	return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus pantool_move(Layer * layer, GdkEventMotion * event, Window * window)
{
	vik_window_pan_move(window, event);
	return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus pantool_release(Layer * layer, GdkEventButton * event, Window * window)
{
	if (event->button == 1) {
		vik_window_pan_release(window, event);
	}
	return VIK_LAYER_TOOL_ACK;
}

static VikToolInterface pan_tool =
	{ { "Pan", "vik-icon-pan", N_("_Pan"), "<control><shift>P", N_("Pan Tool"), 0 },
	  pantool_create, /* (VikToolConstructorFunc) */
	  (VikToolDestructorFunc) NULL,
	  (VikToolActivationFunc) NULL,
	  (VikToolActivationFunc) NULL,
	  (VikToolMouseFunc) pantool_click,
	  (VikToolMouseMoveFunc) pantool_move,
	  (VikToolMouseFunc) pantool_release,
	  NULL,
	  false,
	  GDK_FLEUR,
	  NULL,
	  NULL };
/*** end pan code ********************************************************/

/********************************************************************************
 ** Select tool code
 ********************************************************************************/
static void * selecttool_create(Window * window, Viewport * viewport)
{
	tool_ed_t *t = (tool_ed_t *) malloc(1 * sizeof (tool_ed_t));
	t->window = window;
	t->viewport = viewport;
	t->trw = NULL;
	t->is_waypoint = false;
	return t;
}

static void selecttool_destroy(tool_ed_t * t)
{
	free(t);
}

typedef struct {
	bool cont;
	Viewport * viewport;
	GdkEventButton * event;
	tool_ed_t * tool_edit;
} clicker;

static void click_layer_selected(Layer * layer, clicker * ck)
{
	/* Do nothing when function call returns true; */
	/* i.e. stop on first found item */
	if (ck->cont) {
		if (layer->visible) {
			ck->cont = !layer->select_click(ck->event, ck->viewport, ck->tool_edit);
		}
	}
}

#ifdef WINDOWS
// Hopefully Alt keys by default
#define VIK_MOVE_MODIFIER GDK_MOD1_MASK
#else
// Alt+mouse on Linux desktops tend to be used by the desktop manager
// Thus use an alternate modifier - you may need to set something into this group
#define VIK_MOVE_MODIFIER GDK_MOD5_MASK
#endif

static VikLayerToolFuncStatus selecttool_click(Layer * layer, GdkEventButton * event, tool_ed_t * t)
{
	t->window->select_move = false;
	/* Only allow selection on primary button */
	if (event->button == 1) {

		if (event->state & VIK_MOVE_MODIFIER) {
			vik_window_pan_click(t->window, event);
		} else {
			/* Enable click to apply callback to potentially all track/waypoint layers */
			/* Useful as we can find things that aren't necessarily in the currently selected layer */
			std::list<Layer *> * layers = t->window->layers_panel->get_all_layers_of_type(VIK_LAYER_TRW, false); // Don't get invisible layers
			clicker ck;
			ck.cont = true;
			ck.viewport = t->window->viewport;
			fprintf(stderr, "%s:%d: %x\n", __FUNCTION__, __LINE__, ck.viewport);
			ck.event = event;
			ck.tool_edit = t;
			for (auto iter = layers->begin(); iter != layers->end(); iter++) {
				click_layer_selected(*iter, &ck);
			}
			delete layers;

			// If nothing found then deselect & redraw screen if necessary to remove the highlight
			if (ck.cont) {
				GtkTreeIter iter;
				VikTreeview *vtv = t->window->layers_panel->get_treeview();

				if (vtv->tree->get_selected_iter(&iter)) {
					// Only clear if selected thing is a TrackWaypoint layer or a sublayer
					int type = vtv->tree->get_type(&iter);
					if (type == VIK_TREEVIEW_TYPE_SUBLAYER ||
					    ((Layer *) vtv->tree->get_pointer(&iter))->type == VIK_LAYER_TRW) { /* kamil: get_layer() ? */

						vtv->tree->unselect(&iter);
						if (t->window->clear_highlight()) {
							t->window->draw_update();
						}
					}
				}
			} else {
				// Something found - so enable movement
				t->window->select_move = true;
			}
		}
	} else if ((event->button == 3) && (layer && layer->type == VIK_LAYER_TRW)) {
		if (layer->visible) {
			/* Act on currently selected item to show menu */
			if (t->window->selected_track || t->window->selected_waypoint) {
				layer->show_selected_viewport_menu(event, t->window->viewport);
			}
		}
	}

	return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus selecttool_move(Layer * layer, GdkEventMotion * event, tool_ed_t * t)
{
	if (t->window->select_move) {
		// Don't care about vl here
		if (t->trw) {
			layer->select_move(event, t->viewport, t); /* kamilFIXME: layer->select_move or trw->select_move? */
		}
	} else {
		// Optional Panning
		if (event->state & VIK_MOVE_MODIFIER) {
			vik_window_pan_move(t->window, event);
		}
	}

	return VIK_LAYER_TOOL_ACK;
}

static VikLayerToolFuncStatus selecttool_release(Layer * layer, GdkEventButton * event, tool_ed_t *t)
{
	if (t->window->select_move) {
		// Don't care about vl here
		if (t->trw) {
			((LayerTRW *) t->trw)->select_release(event, t->viewport, t);
		}
	}

	if (event->button == 1 && (event->state & VIK_MOVE_MODIFIER)) {
		vik_window_pan_release(t->window, event);
	}

	// Force pan off incase it was on
	t->window->pan_move = false;
	t->window->pan_x = t->window->pan_y = -1;

	// End of this select movement
	t->window->select_move = false;

	return VIK_LAYER_TOOL_ACK;
}

static VikToolInterface select_tool =
	{ { "Select", "vik-icon-select", N_("_Select"), "<control><shift>S", N_("Select Tool"), 3 },
	  selecttool_create, /* (VikToolConstructorFunc) */
	  (VikToolDestructorFunc) selecttool_destroy,
	  (VikToolActivationFunc) NULL,
	  (VikToolActivationFunc) NULL,
	  (VikToolMouseFunc) selecttool_click,
	  (VikToolMouseMoveFunc) selecttool_move,
	  (VikToolMouseFunc) selecttool_release,
	  (VikToolKeyFunc) NULL,
	  false,
	  GDK_LEFT_PTR,
	  NULL,
	  NULL };
/*** end select tool code ********************************************************/

static void draw_pan_cb(GtkAction * a, Window * window)
{
	// Since the treeview cell editting intercepts standard keyboard handlers, it means we can receive events here
	// Thus if currently editting, ensure we don't move the viewport when Ctrl+<arrow> is received
	Layer * sel = window->layers_panel->get_selected();
	if (sel && sel->vt->tree->get_editing()) {
		return;
	}

	if (!strcmp(gtk_action_get_name(a), "PanNorth")) {
		window->viewport->set_center_screen(window->viewport->get_width()/2, 0);
	} else if (!strcmp(gtk_action_get_name(a), "PanEast")) {
		window->viewport->set_center_screen(window->viewport->get_width(), window->viewport->get_height()/2);
	} else if (!strcmp(gtk_action_get_name(a), "PanSouth")) {
		window->viewport->set_center_screen(window->viewport->get_width()/2, window->viewport->get_height());
	} else if (!strcmp(gtk_action_get_name(a), "PanWest")) {
		window->viewport->set_center_screen(0, window->viewport->get_height()/2);
	}
	window->draw_update();
}

static void draw_zoom_cb(GtkAction * a, Window * window)
{
	unsigned int what = 128;

	if (!strcmp(gtk_action_get_name(a), "ZoomIn")) {
		what = -3;
	} else if (!strcmp(gtk_action_get_name(a), "ZoomOut")) {
		what = -4;
	} else if (!strcmp(gtk_action_get_name(a), "Zoom0.25")) {
		what = -2;
	} else if (!strcmp(gtk_action_get_name(a), "Zoom0.5")) {
		what = -1;
	} else {
		char *s = (char *)gtk_action_get_name(a);
		what = atoi(s+4);
	}

	switch (what) {
	case -3:
		window->viewport->zoom_in();
		break;
	case -4:
		window->viewport->zoom_out();
		break;
	case -1:
		window->viewport->set_zoom(0.5);
		break;
	case -2:
		window->viewport->set_zoom(0.25);
		break;
	default:
		window->viewport->set_zoom(what);
	}
	window->draw_update();
}

static void draw_goto_cb(GtkAction * a, Window * window)
{
	VikCoord new_center;

	if (!strcmp(gtk_action_get_name(a), "GotoLL")) {
		struct LatLon ll, llold;
		vik_coord_to_latlon(window->viewport->get_center(), &llold);
		if (a_dialog_goto_latlon(GTK_WINDOW(window->vw), &ll, &llold)) {
			vik_coord_load_from_latlon(&new_center, window->viewport->get_coord_mode(), &ll);
		} else {
			return;
		}
	} else if (!strcmp(gtk_action_get_name(a), "GotoUTM")) {
		struct UTM utm, utmold;
		vik_coord_to_utm(window->viewport->get_center(), &utmold);
		if (a_dialog_goto_utm(GTK_WINDOW(window->vw), &utm, &utmold)) {
			vik_coord_load_from_utm(&new_center, window->viewport->get_coord_mode(), &utm);
		} else {
			return;
		}
	} else {
		fprintf(stderr, "CRITICAL: Houston, we've had a problem.\n");
		return;
	}

	window->viewport->set_center_coord(&new_center, true);
	window->draw_update();
}

/**
 * center_changed_cb:
 */
static void center_changed_cb(Window * window)
{
	// ATM Keep back always available, so when we pan - we can jump to the last requested position
	/*
	  GtkAction* action_back = gtk_action_group_get_action(window->action_group, "GoBack");
	  if (action_back) {
	  gtk_action_set_sensitive(action_back, vik_viewport_back_available(window->viewport));
	  }
	*/
	GtkAction* action_forward = gtk_action_group_get_action(window->action_group, "GoForward");
	if (action_forward) {
		gtk_action_set_sensitive(action_forward, window->viewport->forward_available());
	}

	toolbar_action_set_sensitive(window->viking_vtb, "GoForward", window->viewport->forward_available());
}

/**
 * draw_goto_back_and_forth:
 */
static void draw_goto_back_and_forth(GtkAction * a, Window * window)
{
	bool changed = false;
	if (!strcmp(gtk_action_get_name(a), "GoBack")) {
		changed = window->viewport->go_back();
	} else if (!strcmp(gtk_action_get_name(a), "GoForward")) {
		changed = window->viewport->go_forward();
	} else {
		return;
	}

	// Recheck buttons sensitivities, as the center changed signal is not sent on back/forward changes
	//  (otherwise we would get stuck in an infinite loop!)
	center_changed_cb(window);

	if (changed) {
		window->draw_update();
	}
}

/**
 * Refresh maps displayed
 */
static void draw_refresh_cb(GtkAction * a, Window * window)
{
	// Only get 'new' maps
	window->simple_map_update(true);
}

static void menu_addlayer_cb(GtkAction * a, Window * window)
{
	for (int type = 0; ((VikLayerTypeEnum) type) < VIK_LAYER_NUM_TYPES; type++) {
		if (!strcmp(vik_layer_get_interface((VikLayerTypeEnum) type)->name, gtk_action_get_name(a))) {
			if (window->layers_panel->new_layer((VikLayerTypeEnum) type)) {
				window->draw_update();
				window->modified = true;
			}
		}
	}
}

static void menu_copy_layer_cb(GtkAction * a, Window * window)
{
	a_clipboard_copy_selected(window->layers_panel);
}

static void menu_cut_layer_cb(GtkAction * a, Window * window)
{
	window->layers_panel->cut_selected();
	window->modified = true;
}

static void menu_paste_layer_cb(GtkAction * a, Window * window)
{
	if (window->layers_panel->paste_selected()) {
		window->modified = true;
	}
}

static void menu_properties_cb(GtkAction * a, Window * window)
{
	if (!window->layers_panel->properties()) {
		a_dialog_info_msg(GTK_WINDOW(window->vw), _("You must select a layer to show its properties."));
	}
}

static void help_help_cb(GtkAction * a, Window * window)
{
#ifdef WINDOWS
	ShellExecute(NULL, "open", ""PACKAGE".pdf", NULL, NULL, SW_SHOWNORMAL);
#else /* WINDOWS */
	char * uri = g_strdup_printf("ghelp:%s", PACKAGE);
	GError *error = NULL;
	bool show = gtk_show_uri(NULL, uri, GDK_CURRENT_TIME, &error);
	if (!show && !error)
		// No error to show, so unlikely this will get called
		a_dialog_error_msg(GTK_WINDOW(window->vw), _("The help system is not available."));
	else if (error) {
		// Main error path
		a_dialog_error_msg_extra(GTK_WINDOW(window->vw), _("Help is not available because: %s.\nEnsure a Mime Type ghelp handler program is installed (e.g. yelp)."), error->message);
		g_error_free(error);
	}
	free(uri);
#endif /* WINDOWS */
}

void Window::toggle_side_panel()
{
	this->show_side_panel = !this->show_side_panel;
	if (this->show_side_panel) {
		gtk_widget_show(GTK_WIDGET(this->layers_panel->gob));
	} else {
		gtk_widget_hide(GTK_WIDGET(this->layers_panel->gob));
	}
}

void Window::toggle_full_screen()
{
	this->show_full_screen = !this->show_full_screen;
	if (this->show_full_screen) {
		gtk_window_fullscreen(GTK_WINDOW(this->vw));
	} else {
		gtk_window_unfullscreen(GTK_WINDOW(this->vw));
	}
}

void Window::toggle_statusbar()
{
	this->show_statusbar = !this->show_statusbar;
	if (this->show_statusbar) {
		gtk_widget_show(GTK_WIDGET(this->viking_vs));
	} else {
		gtk_widget_hide(GTK_WIDGET(this->viking_vs));
	}
}

void Window::toggle_toolbar()
{
	this->show_toolbar = !this->show_toolbar;
	if (this->show_toolbar) {
		gtk_widget_show(toolbar_get_widget(this->viking_vtb));
	} else {
		gtk_widget_hide(toolbar_get_widget(this->viking_vtb));
	}
}

void Window::toggle_main_menu()
{
	this->show_main_menu = !this->show_main_menu;
	if (this->show_main_menu) {
		gtk_widget_show(gtk_ui_manager_get_widget(this->uim, "/ui/MainMenu"));
	} else {
		gtk_widget_hide(gtk_ui_manager_get_widget(this->uim, "/ui/MainMenu"));
	}
}

// Only for 'view' toggle menu widgets ATM.
GtkWidget * get_show_widget_by_name(Window * window, char const * name)
{
	if (!name) {
		return NULL;
	}

	// ATM only FullScreen is *not* in SetShow path
	char *path;
	if (strcmp("FullScreen", name)) {
		path = g_strconcat("/ui/MainMenu/View/SetShow/", name, NULL);
	} else {
		path = g_strconcat("/ui/MainMenu/View/", name, NULL);
	}

	GtkWidget *widget = gtk_ui_manager_get_widget(window->uim, path);
	free(path);

	return widget;
}

static void tb_view_side_panel_cb(GtkAction * a, Window * window)
{
	bool next_state = !window->show_side_panel;
	GtkWidget *check_box = get_show_widget_by_name(window, gtk_action_get_name(a));
	bool menu_state = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM(check_box));
	if(next_state != menu_state) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_box), next_state);
	} else {
		window->toggle_side_panel();
	}
}

static void tb_full_screen_cb(GtkAction * a, Window * window)
{
	bool next_state = !window->show_full_screen;
	GtkWidget *check_box = get_show_widget_by_name(window, gtk_action_get_name(a));
	bool menu_state = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(check_box));
	if (next_state != menu_state) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_box), next_state);
	} else {
		window->toggle_full_screen();
	}
}

static void tb_view_statusbar_cb(GtkAction * a, Window * window)
{
	bool next_state = !window->show_statusbar;
	GtkWidget *check_box = get_show_widget_by_name(window, gtk_action_get_name(a));
	bool menu_state = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(check_box));
	if (next_state != menu_state) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_box), next_state);
	} else {
		window->toggle_statusbar();
	}
}

static void tb_view_toolbar_cb(GtkAction * a, Window * window)
{
	bool next_state = !window->show_toolbar;
	GtkWidget *check_box = get_show_widget_by_name(window, gtk_action_get_name(a));
	bool menu_state = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(check_box));
	if (next_state != menu_state) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_box), next_state);
	} else {
		window->toggle_toolbar();
	}
}

static void tb_view_main_menu_cb(GtkAction * a, Window * window)
{
	bool next_state = !window->show_main_menu;
	GtkWidget *check_box = get_show_widget_by_name(window, gtk_action_get_name(a));
	bool menu_state = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(check_box));
	if (next_state != menu_state) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_box), next_state);
	} else {
		window->toggle_main_menu();
	}
}

static void tb_set_draw_scale_cb(GtkAction * a, Window * window)
{
	bool next_state = !window->viewport->get_draw_scale();
	GtkWidget *check_box = get_show_widget_by_name(window, gtk_action_get_name(a));
	bool menu_state = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(check_box));
	if (next_state != menu_state) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_box), next_state);
	} else {
		window->viewport->set_draw_scale(next_state);
		window->draw_update();
	}
}

static void tb_set_draw_centermark_cb(GtkAction * a, Window * window)
{
	bool next_state = !window->viewport->get_draw_centermark();
	GtkWidget *check_box = get_show_widget_by_name(window, gtk_action_get_name(a));
	bool menu_state = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(check_box));
	if (next_state != menu_state) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_box), next_state);
	} else {
		window->viewport->set_draw_centermark(next_state);
		window->draw_update();
	}
}

static void tb_set_draw_highlight_cb(GtkAction * a, Window * window)
{
	bool next_state = !window->viewport->get_draw_highlight();
	GtkWidget *check_box = get_show_widget_by_name(window, gtk_action_get_name(a));
	bool menu_state = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(check_box));
	if (next_state != menu_state) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_box), next_state);
	} else {
		window->viewport->set_draw_highlight(next_state);
		window->draw_update();
	}
}

static void help_about_cb(GtkAction * a, Window * window)
{
	a_dialog_about(GTK_WINDOW(window->vw));
}

static void help_cache_info_cb(GtkAction * a, Window * window)
{
	// NB: No i18n as this is just for debug
	int byte_size = a_mapcache_get_size();
	char *msg_sz = NULL;
#if GLIB_CHECK_VERSION(2,30,0)
	msg_sz = g_format_size_full(byte_size, G_FORMAT_SIZE_LONG_FORMAT);
#else
	msg_sz = g_format_size_for_display(byte_size);
#endif
	char * msg = g_strdup_printf("Map Cache size is %s with %d items", msg_sz, a_mapcache_get_count());
	a_dialog_info_msg_extra(GTK_WINDOW(window->vw), "%s", msg);
	free(msg_sz);
	free(msg);
}

static void back_forward_info_cb(GtkAction * a, Window * window)
{
	window->viewport->show_centers(GTK_WINDOW(window->vw));
}

static void menu_delete_layer_cb(GtkAction * a, Window * window)
{
	if (window->layers_panel->get_selected()) {
		window->layers_panel->delete_selected();
		window->modified = true;
	} else {
		a_dialog_info_msg(GTK_WINDOW(window->vw), _("You must select a layer to delete."));
	}
}

static void full_screen_cb(GtkAction * a, Window * window)
{
	bool next_state = !window->show_full_screen;
	GtkToggleToolButton *tbutton =(GtkToggleToolButton *)toolbar_get_widget_by_name(window->viking_vtb, gtk_action_get_name(a));
	if (tbutton) {
		bool tb_state = gtk_toggle_tool_button_get_active(tbutton);
		if (next_state != tb_state) {
			gtk_toggle_tool_button_set_active(tbutton, next_state);
		} else {
			window->toggle_full_screen();
		}
	} else {
		window->toggle_full_screen();
	}
}

static void view_side_panel_cb(GtkAction * a, Window * window)
{
	bool next_state = !window->show_side_panel;
	GtkToggleToolButton *tbutton = (GtkToggleToolButton *)toolbar_get_widget_by_name(window->viking_vtb, gtk_action_get_name(a));
	if (tbutton) {
		bool tb_state = gtk_toggle_tool_button_get_active(tbutton);
		if (next_state != tb_state) {
			gtk_toggle_tool_button_set_active(tbutton, next_state);
		} else {
			window->toggle_side_panel();
		}
	} else {
		window->toggle_side_panel();
	}
}

static void view_statusbar_cb(GtkAction * a, Window * window)
{
	bool next_state = !window->show_statusbar;
	GtkToggleToolButton *tbutton = (GtkToggleToolButton *)toolbar_get_widget_by_name(window->viking_vtb, gtk_action_get_name(a));
	if (tbutton) {
		bool tb_state = gtk_toggle_tool_button_get_active(tbutton);
		if (next_state != tb_state) {
			gtk_toggle_tool_button_set_active(tbutton, next_state);
		} else {
			window->toggle_statusbar();
		}
	} else {
		window->toggle_statusbar();
	}
}

static void view_toolbar_cb(GtkAction * a, Window * window)
{
	bool next_state = !window->show_toolbar;
	GtkToggleToolButton *tbutton = (GtkToggleToolButton *)toolbar_get_widget_by_name(window->viking_vtb, gtk_action_get_name(a));
	if (tbutton) {
		bool tb_state = gtk_toggle_tool_button_get_active(tbutton);
		if (next_state != tb_state) {
			gtk_toggle_tool_button_set_active(tbutton, next_state);
		} else {
			window->toggle_toolbar();
		}
	} else {
		window->toggle_toolbar();
	}
}

static void view_main_menu_cb(GtkAction * a, Window * window)
{
	bool next_state = !window->show_main_menu;
	GtkToggleToolButton *tbutton = (GtkToggleToolButton *)toolbar_get_widget_by_name(window->viking_vtb, gtk_action_get_name(a));
	if (tbutton) {
		bool tb_state = gtk_toggle_tool_button_get_active(tbutton);
		if (next_state != tb_state) {
			gtk_toggle_tool_button_set_active(tbutton, next_state);
		} else {
			window->toggle_main_menu();
		}
	} else {
		window->toggle_main_menu();
	}
}

/***************************************
 ** tool management routines
 **
 ***************************************/

static toolbox_tools_t * toolbox_create(Window * window)
{
	toolbox_tools_t *vt = (toolbox_tools_t *) malloc(1 * sizeof (toolbox_tools_t));
	vt->tools = NULL;
	vt->n_tools = 0;
	vt->active_tool = -1;
	vt->window = window;
	return vt;
}

static void toolbox_add_tool(toolbox_tools_t *vt, VikToolInterface *vti, int layer_type)
{
	vt->tools = g_renew(toolbox_tool_t, vt->tools, vt->n_tools+1);
	vt->tools[vt->n_tools].ti = *vti;
	vt->tools[vt->n_tools].layer_type = layer_type;
	if (vti->create) {
		vt->tools[vt->n_tools].state = vti->create(vt->window, vt->window->viewport);
	} else {
		vt->tools[vt->n_tools].state = NULL;
	}
	vt->n_tools++;
}

static int toolbox_get_tool(toolbox_tools_t *vt, char const *tool_name)
{
	int i;
	for (i = 0; i < vt->n_tools; i++) {
		if (!strcmp(tool_name, vt->tools[i].ti.radioActionEntry.name)) {
			break;
		}
	}
	return i;
}

static void toolbox_activate(toolbox_tools_t *vt, char const *tool_name)
{
	int tool = toolbox_get_tool(vt, tool_name);
	toolbox_tool_t *t = &vt->tools[tool];
	Layer * layer = vt->window->layers_panel->get_selected();
#if 0
	if (!layer) {
		return;
	}
#endif

	if (tool == vt->n_tools) {
		fprintf(stderr, "CRITICAL: trying to activate a non-existent tool...\n");
		return;
	}
	/* is the tool already active? */
	if (vt->active_tool == tool) {
		return;
	}

	if (vt->active_tool != -1) {
		if (vt->tools[vt->active_tool].ti.deactivate) {
			vt->tools[vt->active_tool].ti.deactivate(NULL, vt->tools[vt->active_tool].state);
		}
	}
	if (t->ti.activate) {
		t->ti.activate(layer, t->state);
	}
	vt->active_tool = tool;
}

static const GdkCursor *toolbox_get_cursor(toolbox_tools_t *vt, char const *tool_name)
{
	int tool = toolbox_get_tool(vt, tool_name);
	toolbox_tool_t *t = &vt->tools[tool];
	if (t->ti.cursor == NULL) {
		if (t->ti.cursor_type == GDK_CURSOR_IS_PIXMAP && t->ti.cursor_data != NULL) {
			GdkPixbuf *cursor_pixbuf = gdk_pixbuf_from_pixdata(t->ti.cursor_data, false, NULL);
			/* TODO: settable offeset */
			t->ti.cursor = gdk_cursor_new_from_pixbuf(gdk_display_get_default(), cursor_pixbuf, 3, 3);
			g_object_unref(G_OBJECT(cursor_pixbuf));
		} else {
			t->ti.cursor = gdk_cursor_new(t->ti.cursor_type);
		}
	}
	return t->ti.cursor;
}

static void toolbox_click(toolbox_tools_t *vt, GdkEventButton * event)
{
	Layer * layer = vt->window->layers_panel->get_selected();
#if 0
	if (!layer) {
		return;
	}
#endif

	if (vt->active_tool != -1 && vt->tools[vt->active_tool].ti.click) {
		int ltype = vt->tools[vt->active_tool].layer_type;
		if (ltype == TOOL_LAYER_TYPE_NONE || (layer && ltype == layer->type))
			vt->tools[vt->active_tool].ti.click(layer, event, vt->tools[vt->active_tool].state);
	}
}

static void toolbox_move(toolbox_tools_t *vt, GdkEventMotion * event)
{
	Layer * layer = vt->window->layers_panel->get_selected();
#if 0
	if (!layer) {
		return;
	}
#endif

	if (vt->active_tool != -1 && vt->tools[vt->active_tool].ti.move) {
		int ltype = vt->tools[vt->active_tool].layer_type;
		if (ltype == TOOL_LAYER_TYPE_NONE || (layer && ltype == layer->type))
			if (VIK_LAYER_TOOL_ACK_GRAB_FOCUS == vt->tools[vt->active_tool].ti.move(layer, event, vt->tools[vt->active_tool].state))
				gtk_widget_grab_focus(GTK_WIDGET(vt->window->viewport->vvp));
	}
}

static void toolbox_release(toolbox_tools_t *vt, GdkEventButton * event)
{
	Layer * layer = vt->window->layers_panel->get_selected();
#if 0
	if (!layer) {
		return;
	}
#endif

	if (vt->active_tool != -1 && vt->tools[vt->active_tool].ti.release) {
		int ltype = vt->tools[vt->active_tool].layer_type;
		if (ltype == TOOL_LAYER_TYPE_NONE || (layer && ltype == layer->type))
			vt->tools[vt->active_tool].ti.release(layer, event, vt->tools[vt->active_tool].state);
	}
}
/** End tool management ************************************/

void Window::enable_layer_tool(int layer_id, int tool_id)
{
	gtk_action_activate(gtk_action_group_get_action(this->action_group, vik_layer_get_interface((VikLayerTypeEnum) layer_id)->tools[tool_id].radioActionEntry.name));
}

// Be careful with usage - as it may trigger actions being continually alternately by the menu and toolbar items
// DON'T Use this from menu callback with toggle toolbar items!!
static void toolbar_sync(Window * window, char const *name, bool state)
{
	GtkToggleToolButton *tbutton = (GtkToggleToolButton *)toolbar_get_widget_by_name(window->viking_vtb, name);
	if (tbutton) {
		// Causes toggle signal action to be raised.
		gtk_toggle_tool_button_set_active(tbutton, state);
	}
}

/* this function gets called whenever a menu is clicked */
// Note old is not used
static void menu_cb(GtkAction *old, GtkAction * a, Window * window)
{
	// Ensure Toolbar kept in sync
	char const *name = gtk_action_get_name(a);
	toolbar_sync(window, name, true);

	/* White Magic, my friends ... White Magic... */
	int tool_id;
	toolbox_activate(window->vt, name);

	window->viewport_cursor = (GdkCursor *)toolbox_get_cursor(window->vt, name);

	if (gtk_widget_get_window(GTK_WIDGET(window->viewport->vvp)))
		/* We set cursor, even if it is NULL: it resets to default */
		gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(window->viewport->vvp)), window->viewport_cursor);

	if (!strcmp(name, "Pan")) {
		window->current_tool = TOOL_PAN;
	} else if (!strcmp(name, "Zoom")) {
		window->current_tool = TOOL_ZOOM;
	} else if (!strcmp(name, "Ruler")) {
		window->current_tool = TOOL_RULER;
	} else if (!strcmp(name, "Select")) {
		window->current_tool = TOOL_SELECT;
	} else {
		int layer_id;
		for (layer_id=0; ((VikLayerTypeEnum) layer_id) < VIK_LAYER_NUM_TYPES; layer_id++) {
			for (tool_id = 0; tool_id < vik_layer_get_interface((VikLayerTypeEnum) layer_id)->tools_count; tool_id++) {
				if (!g_strcmp0(vik_layer_get_interface((VikLayerTypeEnum) layer_id)->tools[tool_id].radioActionEntry.name, name)) {
					window->current_tool = TOOL_LAYER;
					window->tool_layer_id = (VikLayerTypeEnum) layer_id;
					window->tool_tool_id = tool_id;
				}
			}
		}
	}
	draw_status_tool(window);
}

void Window::set_filename(char const * filename)
{
	if (this->filename) {
		free(this->filename);
	}
	if (filename == NULL) {
		this->filename = NULL;
	} else {
		this->filename = g_strdup(filename);
	}

	/* Refresh window's title */
	char const * file = this->get_filename();
	char * title = g_strdup_printf("%s - Viking", file);
	gtk_window_set_title(GTK_WINDOW(this->vw), title);
	free(title);
}

char const * Window::get_filename()
{
	return this->filename ? a_file_basename(this->filename) : _("Untitled");
}

GtkWidget * Window::get_drawmode_button(VikViewportDrawMode mode)
{
	GtkWidget *mode_button;
	char *buttonname;
	switch (mode) {
#ifdef VIK_CONFIG_EXPEDIA
	case VIK_VIEWPORT_DRAWMODE_EXPEDIA:
		buttonname = (char *) "/ui/MainMenu/View/ModeExpedia";
		break;
#endif
	case VIK_VIEWPORT_DRAWMODE_MERCATOR:
		buttonname = (char *) "/ui/MainMenu/View/ModeMercator";
		break;
	case VIK_VIEWPORT_DRAWMODE_LATLON:
		buttonname = (char *) "/ui/MainMenu/View/ModeLatLon";
		break;
	default:
		buttonname = (char *) "/ui/MainMenu/View/ModeUTM";
	}
	mode_button = gtk_ui_manager_get_widget(this->uim, buttonname);
	assert(mode_button);
	return mode_button;
}

/**
 * get_pan_move:
 *
 * Retrieves window's pan_move.
 *
 * Should be removed as soon as possible.
 *
 * Returns: window's pan_move
 *
 **/
bool Window::get_pan_move()
{
	return this->pan_move;
}

static void on_activate_recent_item(GtkRecentChooser *chooser, Window * window)
{
	char * filename = gtk_recent_chooser_get_current_uri(chooser);
	if (filename != NULL) {
		GFile * file = g_file_new_for_uri(filename);
		char * path = g_file_get_path(file);
		g_object_unref(file);
		if (window->filename) {
			GSList *filenames = NULL;
			filenames = g_slist_append(filenames, path);
			g_signal_emit(G_OBJECT(window->vw), window_signals[VW_OPENWINDOW_SIGNAL], 0, filenames);
			// NB: GSList & contents are freed by main.open_window
		} else {
			window->open_file(path, true);
			free(path);
		}
	}

	free(filename);
}

void Window::setup_recent_files()
{
	GtkRecentFilter * filter = gtk_recent_filter_new();
	/* gtk_recent_filter_add_application (filter, g_get_application_name()); */
	gtk_recent_filter_add_group(filter, "viking");

	GtkRecentManager * manager = gtk_recent_manager_get_default();
	GtkWidget * menu = gtk_recent_chooser_menu_new_for_manager(manager);
	gtk_recent_chooser_set_sort_type(GTK_RECENT_CHOOSER (menu), GTK_RECENT_SORT_MRU);
	gtk_recent_chooser_add_filter(GTK_RECENT_CHOOSER (menu), filter);
	gtk_recent_chooser_set_limit(GTK_RECENT_CHOOSER (menu), a_vik_get_recent_number_files());

	GtkWidget * menu_item = gtk_ui_manager_get_widget(this->uim, "/ui/MainMenu/File/OpenRecentFile");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM (menu_item), menu);

	g_signal_connect(G_OBJECT (menu), "item-activated",
			 G_CALLBACK (on_activate_recent_item), this);
}

/*
 *
 */
void Window::update_recently_used_document(char const * filename)
{
	/* Update Recently Used Document framework */
	GtkRecentManager *manager = gtk_recent_manager_get_default();
	GtkRecentData * recent_data = g_slice_new(GtkRecentData);
	char *groups[] = { (char *) "viking", NULL};
	GFile * file = g_file_new_for_commandline_arg(filename);
	char * uri = g_file_get_uri(file);
	char * basename = g_path_get_basename(filename);
	g_object_unref(file);
	file = NULL;

	recent_data->display_name   = basename;
	recent_data->description    = NULL;
	recent_data->mime_type      = (char *) "text/x-gps-data";
	recent_data->app_name       = (char *) g_get_application_name();
	recent_data->app_exec       = g_strjoin(" ", g_get_prgname(), "%f", NULL);
	recent_data->groups         = groups;
	recent_data->is_private     = false;
	if (!gtk_recent_manager_add_full(manager, uri, recent_data)) {
		char *msg = g_strdup_printf(_("Unable to add '%s' to the list of recently used documents"), uri);
		vik_statusbar_set_message(this->viking_vs, VIK_STATUSBAR_INFO, msg);
		free(msg);
	}

	free(uri);
	free(basename);
	free(recent_data->app_exec);
	g_slice_free(GtkRecentData, recent_data);
}

/**
 * Call this before doing things that may take a long time and otherwise not show any other feedback
 *  such as loading and saving files
 */
void Window::set_busy_cursor()
{
	gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(this->vw)), this->busy_cursor);
	// Viewport has a separate cursor
	gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(this->viewport->vvp)), this->busy_cursor);
	// Ensure cursor updated before doing stuff
	while (gtk_events_pending()) {
		gtk_main_iteration();
	}
}

void Window::clear_busy_cursor()
{
	gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(this->vw)), NULL);
	// Restore viewport cursor
	gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(this->viewport->vvp)), this->viewport_cursor);
}

void vik_window_open_file(Window * window, char const * filename, bool change_filename)
{
	window->open_file(filename, change_filename);
}

void Window::open_file(char const * filename, bool change_filename)
{
	this->set_busy_cursor();

	// Enable the *new* filename to be accessible by the Layers codez
	char *original_filename = g_strdup(this->filename);
	free(this->filename);
	this->filename = g_strdup(filename);
	bool success = false;
	bool restore_original_filename = false;

	LayerAggregate * agg = this->layers_panel->get_top_layer();
	this->loaded_type = a_file_load(agg, this->viewport, filename);
	switch (this->loaded_type) {
	case LOAD_TYPE_READ_FAILURE:
		a_dialog_error_msg(GTK_WINDOW(this->vw), _("The file you requested could not be opened."));
		break;
	case LOAD_TYPE_GPSBABEL_FAILURE:
		a_dialog_error_msg(GTK_WINDOW(this->vw), _("GPSBabel is required to load files of this type or GPSBabel encountered problems."));
		break;
	case LOAD_TYPE_GPX_FAILURE:
		a_dialog_error_msg_extra(GTK_WINDOW(this->vw), _("Unable to load malformed GPX file %s"), filename);
		break;
	case LOAD_TYPE_UNSUPPORTED_FAILURE:
		a_dialog_error_msg_extra(GTK_WINDOW(this->vw), _("Unsupported file type for %s"), filename);
		break;
	case LOAD_TYPE_VIK_FAILURE_NON_FATAL:
		{
			// Since we can process .vik files with issues just show a warning in the status bar
			// Not that a user can do much about it... or tells them what this issue is yet...
			char *msg = g_strdup_printf(_("WARNING: issues encountered loading %s"), a_file_basename(filename));
			vik_statusbar_set_message(this->viking_vs, VIK_STATUSBAR_INFO, msg);
			free(msg);
		}
		// No break, carry on to show any data
	case LOAD_TYPE_VIK_SUCCESS:
		{
			restore_original_filename = true; // NB Will actually get inverted by the 'success' component below
			GtkWidget *mode_button;
			/* Update UI */
			if (change_filename) {
				this->set_filename(filename);
			}
			mode_button = this->get_drawmode_button(this->viewport->get_drawmode());
			this->only_updating_coord_mode_ui = true; /* if we don't set this, it will change the coord to UTM if we click Lat/Lon. I don't know why. */
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mode_button), true);
			this->only_updating_coord_mode_ui = false;

			this->layers_panel->change_coord_mode(this->viewport->get_coord_mode());

			// Slightly long winded methods to align loaded viewport settings with the UI
			//  Since the rewrite for toolbar + menu actions
			//  there no longer exists a simple way to directly change the UI to a value for toggle settings
			//  it only supports toggling the existing setting (otherwise get infinite loops in trying to align tb+menu elements)
			// Thus get state, compare them, if different then invert viewport setting and (re)sync the setting (via toggling)
			bool vp_state_scale = this->viewport->get_draw_scale();
			bool ui_state_scale = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(get_show_widget_by_name(this, "ShowScale")));
			if (vp_state_scale != ui_state_scale) {
				this->viewport->set_draw_scale(!vp_state_scale);
				this->toggle_draw_scale(NULL);
			}
			bool vp_state_centermark = this->viewport->get_draw_centermark();
			bool ui_state_centermark = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(get_show_widget_by_name(this, "ShowCenterMark")));
			if (vp_state_centermark != ui_state_centermark) {
				this->viewport->set_draw_centermark(!vp_state_centermark);
				this->toggle_draw_centermark(NULL);
			}
			bool vp_state_highlight = this->viewport->get_draw_highlight();
			bool ui_state_highlight = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(get_show_widget_by_name(this, "ShowHighlight")));
			if (vp_state_highlight != ui_state_highlight) {
				this->viewport->set_draw_highlight(!vp_state_highlight);
				this->toggle_draw_highlight(NULL);
			}
		}
		// NB No break, carry on to redraw
		//case LOAD_TYPE_OTHER_SUCCESS:
	default:
		success = true;
		// When LOAD_TYPE_OTHER_SUCCESS *only*, this will maintain the existing Viking project
		restore_original_filename = ! restore_original_filename;
		this->update_recently_used_document(filename);
		this->draw_update();
		break;
	}

	if (!success || restore_original_filename) {
		// Load didn't work or want to keep as the existing Viking project, keep using the original name
		this->set_filename(original_filename);
	}
	free(original_filename);

	this->clear_busy_cursor();
}

static void load_file(GtkAction * a, Window * window)
{
	// Window * window = vw->window;
	GSList *files = NULL;
	GSList *cur_file = NULL;
	bool newwindow;
	if (!strcmp(gtk_action_get_name(a), "Open")) {
		newwindow = true;
	} else if (!strcmp(gtk_action_get_name(a), "Append")) {
		newwindow = false;
	} else {
		fprintf(stderr, "CRITICAL: Houston, we've had a problem.\n");
		return;
	}

	GtkWidget * dialog = gtk_file_chooser_dialog_new(_("Please select a GPS data file to open. "),
							 GTK_WINDOW(window->vw),
							 GTK_FILE_CHOOSER_ACTION_OPEN,
							 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							 GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
							 NULL);
	if (last_folder_files_uri) {
		gtk_file_chooser_set_current_folder_uri(GTK_FILE_CHOOSER(dialog), last_folder_files_uri);
	}

	GtkFileFilter *filter;
  // NB file filters are listed this way for alphabetical ordering
#ifdef VIK_CONFIG_GEOCACHES
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Geocaching"));
	gtk_file_filter_add_pattern(filter, "*.loc"); // No MIME type available
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
#endif

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Google Earth"));
	gtk_file_filter_add_mime_type(filter, "application/vnd.google-earth.kml+xml");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("GPX"));
	gtk_file_filter_add_pattern(filter, "*.gpx"); // No MIME type available
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("JPG"));
	gtk_file_filter_add_mime_type(filter, "image/jpeg");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Viking"));
	gtk_file_filter_add_pattern(filter, "*.vik");
	gtk_file_filter_add_pattern(filter, "*.viking");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

	// NB could have filters for gpspoint (*.gps,*.gpsoint?) + gpsmapper (*.gsm,*.gpsmapper?)
	// However assume this are barely used and thus not worthy of inclusion
	//   as they'll just make the options too many and have no clear file pattern
	//   one can always use the all option
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("All"));
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	// Default to any file - same as before open filters were added
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);

	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), true);
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window->vw));
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), true);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		free(last_folder_files_uri);
		last_folder_files_uri = gtk_file_chooser_get_current_folder_uri(GTK_FILE_CHOOSER(dialog));

#ifdef VIKING_PROMPT_IF_MODIFIED
		if ((window->modified || window->filename) && newwindow) {
#else
		if (window->filename && newwindow) {
#endif
			g_signal_emit(G_OBJECT(window->vw), window_signals[VW_OPENWINDOW_SIGNAL], 0, gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog)));
		} else {

			files = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
			bool change_fn = newwindow && (g_slist_length(files)==1); /* only change fn if one file */
			bool first_vik_file = true;
			cur_file = files;
			while (cur_file) {

				char * file_name = (char *) cur_file->data;
				if (newwindow && check_file_magic_vik(file_name)) {
					// Load first of many .vik files in current window
					if (first_vik_file) {
						window->open_file(file_name, true);
						first_vik_file = false;
					} else {
						// Load each subsequent .vik file in a separate window
						Window * new_window = Window::new_window();
						if (new_window) {
							new_window->open_file(file_name, true);
						}
					}
				} else {
					// Other file types
					window->open_file(file_name, change_fn);
				}

				free(file_name);
				cur_file = g_slist_next(cur_file);
			}
			g_slist_free(files);
		}
	}
	gtk_widget_destroy(dialog);
}

static bool save_file_as(GtkAction * a, Window * window)
{
	bool rv = false;
	char const * fn;

	GtkWidget * dialog = gtk_file_chooser_dialog_new(_("Save as Viking File."),
							 GTK_WINDOW(window->vw),
							 GTK_FILE_CHOOSER_ACTION_SAVE,
							 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							 GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
							 NULL);
	if (last_folder_files_uri) {
		gtk_file_chooser_set_current_folder_uri(GTK_FILE_CHOOSER(dialog), last_folder_files_uri);
	}

	GtkFileFilter * filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("All"));
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Viking"));
	gtk_file_filter_add_pattern(filter, "*.vik");
	gtk_file_filter_add_pattern(filter, "*.viking");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	// Default to a Viking file
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);

	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window->vw));
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), true);


	// Auto append / replace extension with '.vik' to the suggested file name as it's going to be a Viking File
	char * auto_save_name = g_strdup(window->get_filename());
	if (!a_file_check_ext(auto_save_name, ".vik")) {
		auto_save_name = g_strconcat(auto_save_name, ".vik", NULL);
	}

	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), auto_save_name);

	while (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		if (g_file_test(fn, G_FILE_TEST_EXISTS) == false || a_dialog_yes_or_no(GTK_WINDOW(dialog), _("The file \"%s\" exists, do you wish to overwrite it?"), a_file_basename(fn))) {
			window->set_filename(fn);
			rv = window->window_save();
			if (rv) {
				window->modified = false;
				free(last_folder_files_uri);
				last_folder_files_uri = gtk_file_chooser_get_current_folder_uri(GTK_FILE_CHOOSER(dialog));
			}
			break;
		}
	}
	free(auto_save_name);
	gtk_widget_destroy(dialog);
	return rv;
}

bool Window::window_save()
{
	this->set_busy_cursor();
	bool success = true;

	if (a_file_save(this->layers_panel->get_top_layer(), this->viewport, this->filename)) {
		this->update_recently_used_document(this->filename);
	} else {
		a_dialog_error_msg(GTK_WINDOW(this->vw), _("The filename you requested could not be opened for writing."));
		success = false;
	}
	this->clear_busy_cursor();
	return success;
}

static bool save_file(GtkAction * a, Window * window)
{
	if (!window->filename) {
		return save_file_as(NULL, window);
	} else {
		window->modified = false;
		return window->window_save();
	}
}

/**
 * export_to:
 *
 * Export all TRW Layers in the list to individual files in the specified directory
 *
 * Returns: %true on success
 */
bool Window::export_to(std::list<Layer *> * layers, VikFileType_t vft, char const *dir, char const *extension)
{
	bool success = true;

	int export_count = 0;

	this->set_busy_cursor();

	for (auto iter = layers->begin(); iter != layers->end(); iter++) {
		Layer * l = *iter;
		char *fn = g_strconcat(dir, G_DIR_SEPARATOR_S, l->name, extension, NULL);

		// Some protection in attempting to write too many same named files
		// As this will get horribly slow...
		bool safe = false;
		int ii = 2;
		while (ii < 5000) {
			if (g_file_test(fn, G_FILE_TEST_EXISTS)) {
				// Try rename
				free(fn);

				fn = g_strdup_printf ("%s%s%s#%03d%s", dir, G_DIR_SEPARATOR_S, l->name, ii, extension);
			} else {
				safe = true;
				break;
			}
			ii++;
		}
		if (ii == 5000) {
			success = false;
		}

		// NB: We allow exporting empty layers
		if (safe) {
			bool this_success = a_file_export((LayerTRW *) (*iter), fn, vft, NULL, true);

			// Show some progress
			if (this_success) {
				export_count++;
				char *message = g_strdup_printf(_("Exporting to file: %s"), fn);
				vik_statusbar_set_message(this->viking_vs, VIK_STATUSBAR_INFO, message);
				while (gtk_events_pending()) {
					gtk_main_iteration();
				}
				free(message);
			}

			success = success && this_success;
		}

		free(fn);
	}

	this->clear_busy_cursor();

	// Confirm what happened.
	char *message = g_strdup_printf(_("Exported files: %d"), export_count);
	vik_statusbar_set_message(this->viking_vs, VIK_STATUSBAR_INFO, message);
	free(message);

	return success;
}

void Window::export_to_common(VikFileType_t vft, char const * extension)
{
	std::list<Layer *> * layers = this->layers_panel->get_all_layers_of_type(VIK_LAYER_TRW, true);

	if (!layers || layers->empty()) {
		a_dialog_info_msg(GTK_WINDOW(this->vw), _("Nothing to Export!"));
		/* kamilFIXME: delete layers? */
		return;
	}

	GtkWidget *dialog = gtk_file_chooser_dialog_new(_("Export to directory"),
							GTK_WINDOW(this->vw),
							GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
							GTK_STOCK_CANCEL,
							GTK_RESPONSE_REJECT,
							GTK_STOCK_OK,
							GTK_RESPONSE_ACCEPT,
							NULL);
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(this->vw));
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), true);
	gtk_window_set_modal(GTK_WINDOW(dialog), true);

	gtk_widget_show_all(dialog);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		char *dir = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		gtk_widget_destroy(dialog);
		if (dir) {
			if (!this->export_to(layers, vft, dir, extension)) {
				a_dialog_error_msg(GTK_WINDOW(this->vw),_("Could not convert all files"));
			}
			free(dir);
		}
	} else {
		gtk_widget_destroy(dialog);
	}

	delete layers;
}

static void export_to_gpx_cb(GtkAction * a, Window * window)
{
	window->export_to_common(FILE_TYPE_GPX, ".gpx");
}

static void export_to_kml_cb(GtkAction * a, Window * window)
{
	window->export_to_common(FILE_TYPE_KML, ".kml");
}

static void file_properties_cb(GtkAction * a, Window * window)
{
	char *message = NULL;
	if (window->filename) {
		if (g_file_test(window->filename, G_FILE_TEST_EXISTS)) {
			// Get some timestamp information of the file
			GStatBuf stat_buf;
			if (g_stat(window->filename, &stat_buf) == 0) {
				char time_buf[64];
				strftime(time_buf, sizeof(time_buf), "%c", gmtime((const time_t *)&stat_buf.st_mtime));
				char *size = NULL;
				int byte_size = stat_buf.st_size;
#if GLIB_CHECK_VERSION(2,30,0)
				size = g_format_size_full(byte_size, G_FORMAT_SIZE_DEFAULT);
#else
				size = g_format_size_for_display(byte_size);
#endif
				message = g_strdup_printf("%s\n\n%s\n\n%s", window->filename, time_buf, size);
				free(size);
			}
		} else {
			message = strdup(_("File not accessible"));
		}
	} else {
		message = strdup(_("No Viking File"));
	}

	// Show the info
	a_dialog_info_msg(GTK_WINDOW(window->vw), message);
	free(message);
}

static void my_acquire(Window * window, VikDataSourceInterface *datasource)
{
	vik_datasource_mode_t mode = datasource->mode;
	if (mode == VIK_DATASOURCE_AUTO_LAYER_MANAGEMENT)
		mode = VIK_DATASOURCE_CREATENEWLAYER;
	a_acquire(window, window->layers_panel, window->viewport, mode, datasource, NULL, NULL);
}

static void acquire_from_gps(GtkAction * a, Window * window)
{
	my_acquire(window, &vik_datasource_gps_interface);
}

static void acquire_from_file(GtkAction * a, Window * window)
{
	my_acquire(window, &vik_datasource_file_interface);
}

static void acquire_from_geojson(GtkAction * a, Window * window)
{
	my_acquire(window, &vik_datasource_geojson_interface);
}

static void acquire_from_routing(GtkAction * a, Window * window)
{
	my_acquire(window, &vik_datasource_routing_interface);
}

#ifdef VIK_CONFIG_OPENSTREETMAP
static void acquire_from_osm(GtkAction * a, Window * window)
{
	my_acquire(window, &vik_datasource_osm_interface);
}

static void acquire_from_my_osm(GtkAction * a, Window * window)
{
	my_acquire(window, &vik_datasource_osm_my_traces_interface);
}
#endif

#ifdef VIK_CONFIG_GEOCACHES
static void acquire_from_gc(GtkAction * a, Window * window)
{
	my_acquire(window, &vik_datasource_gc_interface);
}
#endif

#ifdef VIK_CONFIG_GEOTAG
static void acquire_from_geotag(GtkAction * a, Window * window)
{
	my_acquire(window, &vik_datasource_geotag_interface);
}
#endif

#ifdef VIK_CONFIG_GEONAMES
static void acquire_from_wikipedia(GtkAction * a, Window * window)
{
	my_acquire(window, &vik_datasource_wikipedia_interface);
}
#endif

static void acquire_from_url(GtkAction * a, Window * window)
{
	my_acquire(window, &vik_datasource_url_interface);
}

static void goto_default_location(GtkAction * a, Window * window)
{
	struct LatLon ll;
	ll.lat = a_vik_get_default_lat();
	ll.lon = a_vik_get_default_long();
	window->viewport->set_center_latlon(&ll, true);
	window->layers_panel->emit_update();
}


static void goto_address(GtkAction * a, Window * window)
{
	a_vik_goto(window, window->viewport);
	window->layers_panel->emit_update();
}

static void mapcache_flush_cb(GtkAction * a, Window * window)
{
	a_mapcache_flush();
}

static void menu_copy_centre_cb(GtkAction * a, Window * window)
{
	char *lat = NULL, *lon = NULL;

	const VikCoord * coord = window->viewport->get_center();
	struct UTM utm;
	vik_coord_to_utm(coord, &utm);

	bool full_format = false;
	(void)a_settings_get_boolean(VIK_SETTINGS_WIN_COPY_CENTRE_FULL_FORMAT, &full_format);

	if (full_format) {
		// Bells & Whistles - may include degrees, minutes and second symbols
		get_location_strings(window, utm, &lat, &lon);
	} else {
		// Simple x.xx y.yy format
		struct LatLon ll;
		a_coords_utm_to_latlon(&utm, &ll);
		lat = g_strdup_printf("%.6f", ll.lat);
		lon = g_strdup_printf("%.6f", ll.lon);
	}

	char *msg = g_strdup_printf("%s %s", lat, lon);
	free(lat);
	free(lon);

	a_clipboard_copy(VIK_CLIPBOARD_DATA_TEXT, 0, 0, 0, msg, NULL);

	free(msg);
}

static void layer_defaults_cb(GtkAction * a, Window * window)
{
	char ** texts = g_strsplit(gtk_action_get_name(a), "Layer", 0);

	if (!texts[1]) {
		return; // Internally broken :(
	}

	if (!a_layer_defaults_show_window(GTK_WINDOW(window->vw), texts[1])) {
		a_dialog_info_msg(GTK_WINDOW(window->vw), _("This layer has no configurable properties."));
	}
	// NB no update needed

	g_strfreev(texts);
}

static void preferences_change_update(Window * window)
{
	// Want to update all TrackWaypoint layers
	std::list<Layer *> * layers = window->layers_panel->get_all_layers_of_type(VIK_LAYER_TRW, true);
	if (!layers || layers->empty()) {
		return;
	}

	for (auto iter = layers->begin(); iter != layers->end(); iter++) {
		// Reset the individual waypoints themselves due to the preferences change
		LayerTRW * trw = (LayerTRW *) *iter;
		trw->reset_waypoints();
	}

	delete layers;

	window->draw_update();
}

static void preferences_cb(GtkAction * a, Window * window)
{
	bool wp_icon_size = a_vik_get_use_large_waypoint_icons();

	a_preferences_show_window(GTK_WINDOW(window->vw));

	// Has the waypoint size setting changed?
	if (wp_icon_size != a_vik_get_use_large_waypoint_icons()) {
		// Delete icon indexing 'cache' and so automatically regenerates with the new setting when changed
		clear_garmin_icon_syms();

		// Update all windows
		for (auto i = window_list.begin(); i != window_list.end(); i++) {
			preferences_change_update(*i);
		}
	}

	// Ensure TZ Lookup initialized
	if (a_vik_get_time_ref_frame() == VIK_TIME_REF_WORLD) {
		vu_setup_lat_lon_tz_lookup();
	}

	toolbar_apply_settings(window->viking_vtb, window->main_vbox, window->menu_hbox, true);
}

static void default_location_cb(GtkAction * a, Window * window)
{
	/* Simplistic repeat of preference setting
	   Only the name & type are important for setting the preference via this 'external' way */
	VikLayerParam pref_lat[] = {
		{ VIK_LAYER_NUM_TYPES,
		  VIKING_PREFERENCES_NAMESPACE "default_latitude",
		  VIK_LAYER_PARAM_DOUBLE,
		  VIK_LAYER_GROUP_NONE,
		  NULL,
		  VIK_LAYER_WIDGET_SPINBUTTON,
		  NULL,
		  NULL,
		  NULL,
		  NULL,
		  NULL,
		  NULL,
		},
	};
	VikLayerParam pref_lon[] = {
		{ VIK_LAYER_NUM_TYPES,
		  VIKING_PREFERENCES_NAMESPACE "default_longitude",
		  VIK_LAYER_PARAM_DOUBLE,
		  VIK_LAYER_GROUP_NONE,
		  NULL,
		  VIK_LAYER_WIDGET_SPINBUTTON,
		  NULL,
		  NULL,
		  NULL,
		  NULL,
		  NULL,
		  NULL,
		},
	};

	/* Get current center */
	struct LatLon ll;
	vik_coord_to_latlon(window->viewport->get_center(), &ll);

	/* Apply to preferences */
	VikLayerParamData vlp_data;
	vlp_data.d = ll.lat;
	a_preferences_run_setparam(vlp_data, pref_lat);
	vlp_data.d = ll.lon;
	a_preferences_run_setparam(vlp_data, pref_lon);
	/* Remember to save */
	a_preferences_save_to_file();
}

/**
 * Delete All
 */
static void clear_cb(GtkAction * a, Window * window)
{
	// Do nothing if empty
	if (!window->layers_panel->get_top_layer()->is_empty()) {
		if (a_dialog_yes_or_no(GTK_WINDOW(window->vw), _("Are you sure you wish to delete all layers?"), NULL)) {
			window->layers_panel->clear();
			window->set_filename(NULL);
			window->draw_update();
		}
	}
}

static void window_close(GtkAction * a, Window * window)
{
	if (!delete_event((VikWindow *) window->vw)) {
		gtk_widget_destroy(GTK_WIDGET(window->vw));
	}
}

static bool save_file_and_exit(GtkAction * a, Window * window)
{
	if (save_file(NULL, window)) {
		window_close(NULL, window);
		return(true);
	} else {
		return(false);
	}
}

static void zoom_to_cb(GtkAction * a, Window * window)
{
	double xmpp = window->viewport->get_xmpp(), ympp = window->viewport->get_ympp();
	if (a_dialog_custom_zoom(GTK_WINDOW(window->vw), &xmpp, &ympp)) {
		window->viewport->set_xmpp(xmpp);
		window->viewport->set_ympp(ympp);
		window->draw_update();
	}
}

static void save_image_file(Window * window, char const *fn, unsigned int w, unsigned int h, double zoom, bool save_as_png, bool save_kmz)
{
	/* more efficient way: stuff draws directly to pixbuf (fork viewport) */
	GdkPixbuf *pixbuf_to_save;
	double old_xmpp, old_ympp;
	GError *error = NULL;

	GtkWidget * msgbox = gtk_message_dialog_new(GTK_WINDOW(window->vw),
						    (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
						    GTK_MESSAGE_INFO,
						    GTK_BUTTONS_NONE,
						    _("Generating image file..."));

	g_signal_connect_swapped(msgbox, "response", G_CALLBACK (gtk_widget_destroy), msgbox);
	// Ensure dialog shown
	gtk_widget_show_all(msgbox);
	// Try harder...
	vik_statusbar_set_message(window->viking_vs, VIK_STATUSBAR_INFO, _("Generating image file..."));
	while (gtk_events_pending()) {
		gtk_main_iteration();
	}
	// Despite many efforts & variations, GTK on my Linux system doesn't show the actual msgbox contents :(
	// At least the empty box can give a clue something's going on + the statusbar msg...
	// Windows version under Wine OK!

	/* backup old zoom & set new */
	old_xmpp = window->viewport->get_xmpp();
	old_ympp = window->viewport->get_ympp();
	window->viewport->set_zoom(zoom);

	/* reset width and height: */
	window->viewport->configure_manually(w, h);

	/* draw all layers */
	window->draw_redraw();

	/* save buffer as file. */
	pixbuf_to_save = gdk_pixbuf_get_from_drawable(NULL, GDK_DRAWABLE(window->viewport->get_pixmap()), NULL, 0, 0, 0, 0, w, h);
	if (!pixbuf_to_save) {
		fprintf(stderr, "WARNING: Failed to generate internal pixmap size: %d x %d\n", w, h);
		gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(msgbox), _("Failed to generate internal image.\n\nTry creating a smaller image."));

		// goto cleanup;
		vik_statusbar_set_message(window->viking_vs, VIK_STATUSBAR_INFO, "");
		gtk_dialog_add_button(GTK_DIALOG(msgbox), GTK_STOCK_OK, GTK_RESPONSE_OK);
		gtk_dialog_run(GTK_DIALOG(msgbox)); // Don't care about the result

		/* pretend like nothing happened ;) */
		window->viewport->set_xmpp(old_xmpp);
		window->viewport->set_ympp(old_ympp);
		window->viewport->configure();
		window->draw_update();

		return;
	}

	int ans = 0; // Default to success

	if (save_kmz) {
		double north, east, south, west;
		window->viewport->get_min_max_lat_lon(&south, &north, &west, &east);
		ans = kmz_save_file(pixbuf_to_save, fn, north, east, south, west);
	} else {
		gdk_pixbuf_save(pixbuf_to_save, fn, save_as_png ? "png" : "jpeg", &error, NULL);
		if (error) {
			fprintf(stderr, "WARNING: Unable to write to file %s: %s\n", fn, error->message);
			g_error_free(error);
			ans = 42;
		}
	}

	if (ans == 0) {
		gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(msgbox), _("Image file generated."));
	} else {
		gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(msgbox), _("Failed to generate image file."));
	}

	g_object_unref(G_OBJECT(pixbuf_to_save));

 cleanup:
	vik_statusbar_set_message(window->viking_vs, VIK_STATUSBAR_INFO, "");
	gtk_dialog_add_button(GTK_DIALOG(msgbox), GTK_STOCK_OK, GTK_RESPONSE_OK);
	gtk_dialog_run(GTK_DIALOG(msgbox)); // Don't care about the result

	/* pretend like nothing happened ;) */
	window->viewport->set_xmpp(old_xmpp);
	window->viewport->set_ympp(old_ympp);
	window->viewport->configure();
	window->draw_update();
}

void Window::save_image_dir(char const * fn, unsigned int w, unsigned int h, double zoom, bool save_as_png, unsigned int tiles_w, unsigned int tiles_h)
{
	unsigned long size = sizeof(char) * (strlen(fn) + 15);
	char *name_of_file = (char *) malloc(size);
	struct UTM utm;

	/* *** copied from above *** */


	/* backup old zoom & set new */
	double old_xmpp = this->viewport->get_xmpp();
	double old_ympp = this->viewport->get_ympp();
	this->viewport->set_zoom(zoom);

	/* reset width and height: do this only once for all images (same size) */
	this->viewport->configure_manually(w, h);
	/* *** end copy from above *** */

	assert (this->viewport->get_coord_mode() == VIK_COORD_UTM);

	if (g_mkdir(fn,0777) != 0) {
		fprintf(stderr, "WARNING: %s: Failed to create directory %s\n", __FUNCTION__, fn);
	}

	struct UTM utm_orig = *((const struct UTM *) this->viewport->get_center());

	GError *error = NULL;
	for (unsigned int y = 1; y <= tiles_h; y++) {
		for (unsigned int x = 1; x <= tiles_w; x++) {
			snprintf(name_of_file, size, "%s%cy%d-x%d.%s", fn, G_DIR_SEPARATOR, y, x, save_as_png ? "png" : "jpg");
			utm = utm_orig;
			if (tiles_w & 0x1) {
				utm.easting += ((double)x - ceil(((double)tiles_w)/2)) * (w*zoom);
			} else {
				utm.easting += ((double)x - (((double)tiles_w)+1)/2) * (w*zoom);
			}

			if (tiles_h & 0x1) {/* odd */
				utm.northing -= ((double)y - ceil(((double)tiles_h)/2)) * (h*zoom);
			} else { /* even */
				utm.northing -= ((double)y - (((double)tiles_h)+1)/2) * (h*zoom);
			}

			/* move to correct place. */
			this->viewport->set_center_utm(&utm, false);

			this->draw_redraw();

			/* save buffer as file. */
			GdkPixbuf * pixbuf_to_save = gdk_pixbuf_get_from_drawable(NULL, GDK_DRAWABLE (this->viewport->get_pixmap()), NULL, 0, 0, 0, 0, w, h);
			gdk_pixbuf_save(pixbuf_to_save, name_of_file, save_as_png ? "png" : "jpeg", &error, NULL);
			if (error) {
				char *msg = g_strdup_printf(_("Unable to write to file %s: %s"), name_of_file, error->message);
				vik_statusbar_set_message(this->viking_vs, VIK_STATUSBAR_INFO, msg);
				free(msg);
				g_error_free(error);
			}

			g_object_unref(G_OBJECT(pixbuf_to_save));
		}
	}

	this->viewport->set_center_utm(&utm_orig, false);
	this->viewport->set_xmpp(old_xmpp);
	this->viewport->set_ympp(old_ympp);
	this->viewport->configure();
	this->draw_update();

	free(name_of_file);
}

static void draw_to_image_file_current_window_cb(GtkWidget* widget,GdkEventButton * event, void ** pass_along)
{
	Window * window = (Window *) pass_along[0];
	GtkSpinButton *width_spin = GTK_SPIN_BUTTON(pass_along[1]), *height_spin = GTK_SPIN_BUTTON(pass_along[2]);

	int active = gtk_combo_box_get_active(GTK_COMBO_BOX(pass_along[3]));
	double zoom = pow(2, active-2);

	double width_min, width_max, height_min, height_max;

	gtk_spin_button_get_range(width_spin, &width_min, &width_max);
	gtk_spin_button_get_range(height_spin, &height_min, &height_max);

	/* TODO: support for xzoom and yzoom values */
	int width = window->viewport->get_width() * window->viewport->get_xmpp() / zoom;
	int height = window->viewport->get_height() * window->viewport->get_xmpp() / zoom;

	if (width > width_max || width < width_min || height > height_max || height < height_min) {
		a_dialog_info_msg(GTK_WINDOW(window->vw), _("Viewable region outside allowable pixel size bounds for image. Clipping width/height values."));
	}

	gtk_spin_button_set_value(width_spin, width);
	gtk_spin_button_set_value(height_spin, height);
}

static void draw_to_image_file_total_area_cb(GtkSpinButton *spinbutton, void ** pass_along)
{
	GtkSpinButton *width_spin = GTK_SPIN_BUTTON(pass_along[1]), *height_spin = GTK_SPIN_BUTTON(pass_along[2]);

	int active = gtk_combo_box_get_active(GTK_COMBO_BOX(pass_along[3]));
	double zoom = pow(2, active-2);

	char *label_text;
	double w = gtk_spin_button_get_value(width_spin) * zoom;
	double h = gtk_spin_button_get_value(height_spin) * zoom;
	if (pass_along[4]) { /* save many images; find TOTAL area covered */
		w *= gtk_spin_button_get_value(GTK_SPIN_BUTTON(pass_along[4]));
		h *= gtk_spin_button_get_value(GTK_SPIN_BUTTON(pass_along[5]));
	}
	vik_units_distance_t dist_units = a_vik_get_units_distance();
	switch (dist_units) {
	case VIK_UNITS_DISTANCE_KILOMETRES:
		label_text = g_strdup_printf(_("Total area: %ldm x %ldm (%.3f sq. km)"), (glong)w, (glong)h, (w*h/1000000));
		break;
	case VIK_UNITS_DISTANCE_MILES:
		label_text = g_strdup_printf(_("Total area: %ldm x %ldm (%.3f sq. miles)"), (glong)w, (glong)h, (w*h/2589988.11));
		break;
	case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
		label_text = g_strdup_printf(_("Total area: %ldm x %ldm (%.3f sq. NM)"), (glong)w, (glong)h, (w*h/(1852.0*1852.0)));
		break;
	default:
		label_text = g_strdup_printf("Just to keep the compiler happy");
		fprintf(stderr, "CRITICAL: Houston, we've had a problem. distance=%d\n", dist_units);
	}

	gtk_label_set_text(GTK_LABEL(pass_along[6]), label_text);
	free(label_text);
}

/*
 * Get an allocated filename (or directory as specified)
 */
static char * draw_image_filename(Window * window, img_generation_t img_gen)
{
	char * fn = NULL;
	if (img_gen != VW_GEN_DIRECTORY_OF_IMAGES) {
		// Single file
		GtkWidget * dialog = gtk_file_chooser_dialog_new(_("Save Image"),
								 GTK_WINDOW(window->vw),
								 GTK_FILE_CHOOSER_ACTION_SAVE,
								 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
								 GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
								 NULL);
		if (last_folder_images_uri) {
			gtk_file_chooser_set_current_folder_uri(GTK_FILE_CHOOSER(dialog), last_folder_images_uri);
		}

		GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
		/* Add filters */
		GtkFileFilter *filter;
		filter = gtk_file_filter_new();
		gtk_file_filter_set_name(filter, _("All"));
		gtk_file_filter_add_pattern(filter, "*");
		gtk_file_chooser_add_filter(chooser, filter);

		if (img_gen == VW_GEN_KMZ_FILE) {
			filter = gtk_file_filter_new();
			gtk_file_filter_set_name(filter, _("KMZ"));
			gtk_file_filter_add_mime_type(filter, "vnd.google-earth.kmz");
			gtk_file_filter_add_pattern(filter, "*.kmz");
			gtk_file_chooser_add_filter(chooser, filter);
			gtk_file_chooser_set_filter(chooser, filter);
		} else {
			filter = gtk_file_filter_new();
			gtk_file_filter_set_name(filter, _("JPG"));
			gtk_file_filter_add_mime_type(filter, "image/jpeg");
			gtk_file_chooser_add_filter(chooser, filter);

			if (!window->draw_image_save_as_png) {
				gtk_file_chooser_set_filter(chooser, filter);
			}

			filter = gtk_file_filter_new();
			gtk_file_filter_set_name(filter, _("PNG"));
			gtk_file_filter_add_mime_type(filter, "image/png");
			gtk_file_chooser_add_filter(chooser, filter);

			if (window->draw_image_save_as_png) {
				gtk_file_chooser_set_filter(chooser, filter);
			}
		}

		gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window->vw));
		gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), true);

		if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
			free(last_folder_images_uri);
			last_folder_images_uri = gtk_file_chooser_get_current_folder_uri(GTK_FILE_CHOOSER(dialog));

			fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
			if (g_file_test(fn, G_FILE_TEST_EXISTS)) {
				if (! a_dialog_yes_or_no(GTK_WINDOW(dialog), _("The file \"%s\" exists, do you wish to overwrite it?"), a_file_basename(fn))) {
					fn = NULL;
				}
			}
		}
		gtk_widget_destroy(dialog);
	} else {
		// A directory
		// For some reason this method is only written to work in UTM...
		if (window->viewport->get_coord_mode() != VIK_COORD_UTM) {
			a_dialog_error_msg(GTK_WINDOW(window->vw), _("You must be in UTM mode to use this feature"));
			return fn;
		}

		GtkWidget * dialog = gtk_file_chooser_dialog_new(_("Choose a directory to hold images"),
								 GTK_WINDOW(window->vw),
								 GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
								 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
								 GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
								 NULL);
		gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window->vw));
		gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), true);

		if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
			fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		}
		gtk_widget_destroy(dialog);
	}
	return fn;
}

void Window::draw_to_image_file(img_generation_t img_gen)
{
	/* todo: default for answers inside VikWindow or static (thruout instance) */
	GtkWidget * dialog = gtk_dialog_new_with_buttons(_("Save to Image File"), GTK_WINDOW(this->vw),
							 (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
							 GTK_STOCK_CANCEL,
							 GTK_RESPONSE_REJECT,
							 GTK_STOCK_OK,
							 GTK_RESPONSE_ACCEPT,
							 NULL);

	// only used for VW_GEN_DIRECTORY_OF_IMAGES
	GtkWidget *tiles_width_spin = NULL, *tiles_height_spin = NULL;

	GtkWidget * width_label = gtk_label_new(_("Width(pixels):"));
	GtkWidget * width_spin = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(this->draw_image_width, 10, 50000, 10, 100, 0)), 10, 0);
	GtkWidget * height_label = gtk_label_new(_("Height (pixels):"));
	GtkWidget * height_spin = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(this->draw_image_height, 10, 50000, 10, 100, 0)), 10, 0);
#ifdef WINDOWS
	GtkWidget *win_warning_label = gtk_label_new(_("WARNING: USING LARGE IMAGES OVER 10000x10000\nMAY CRASH THE PROGRAM!"));
#endif
	GtkWidget * zoom_label = gtk_label_new(_("Zoom (meters per pixel):"));
	/* TODO: separate xzoom and yzoom factors */
	GtkWidget * zoom_combo = create_zoom_combo_all_levels();

	double mpp = this->viewport->get_xmpp();
	int active = 2 + round(log(mpp) / log(2));

	// Can we not hard code size here?
	if (active > 17) {
		active = 17;
	}
	if (active < 0) {
		active = 0;
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(zoom_combo), active);

	GtkWidget * total_size_label = gtk_label_new(NULL);

	GtkWidget * current_window_button = gtk_button_new_with_label(_("Area in current viewable window"));
	void * current_window_pass_along[7];
	current_window_pass_along[0] = this;
	current_window_pass_along[1] = width_spin;
	current_window_pass_along[2] = height_spin;
	current_window_pass_along[3] = zoom_combo;
	current_window_pass_along[4] = NULL; // Only for directory of tiles: width
	current_window_pass_along[5] = NULL; // Only for directory of tiles: height
	current_window_pass_along[6] = total_size_label;
	g_signal_connect(G_OBJECT(current_window_button), "button_press_event", G_CALLBACK(draw_to_image_file_current_window_cb), current_window_pass_along);

	GtkWidget *png_radio = gtk_radio_button_new_with_label(NULL, _("Save as PNG"));
	GtkWidget *jpeg_radio = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(png_radio), _("Save as JPEG"));

	if (img_gen == VW_GEN_KMZ_FILE) {
		// Don't show image type selection if creating a KMZ (always JPG internally)
		// Start with viewable area by default
		draw_to_image_file_current_window_cb(current_window_button, NULL, current_window_pass_along);
	} else {
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), jpeg_radio, false, false, 0);
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), png_radio, false, false, 0);
	}

	if (!this->draw_image_save_as_png) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(jpeg_radio), true);
	}

	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), width_label, false, false, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), width_spin, false, false, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), height_label, false, false, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), height_spin, false, false, 0);
#ifdef WINDOWS
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), win_warning_label, false, false, 0);
#endif
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), current_window_button, false, false, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), zoom_label, false, false, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), zoom_combo, false, false, 0);

	if (img_gen == VW_GEN_DIRECTORY_OF_IMAGES) {
		GtkWidget *tiles_width_label, *tiles_height_label;

		tiles_width_label = gtk_label_new(_("East-west image tiles:"));
		tiles_width_spin = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(5, 1, 10, 1, 100, 0)), 1, 0);
		tiles_height_label = gtk_label_new(_("North-south image tiles:"));
		tiles_height_spin = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(5, 1, 10, 1, 100, 0)), 1, 0);
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), tiles_width_label, false, false, 0);
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), tiles_width_spin, false, false, 0);
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), tiles_height_label, false, false, 0);
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), tiles_height_spin, false, false, 0);

		current_window_pass_along[4] = tiles_width_spin;
		current_window_pass_along[5] = tiles_height_spin;
		g_signal_connect(G_OBJECT(tiles_width_spin), "value-changed", G_CALLBACK(draw_to_image_file_total_area_cb), current_window_pass_along);
		g_signal_connect(G_OBJECT(tiles_height_spin), "value-changed", G_CALLBACK(draw_to_image_file_total_area_cb), current_window_pass_along);
	}
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), total_size_label, false, false, 0);
	g_signal_connect(G_OBJECT(width_spin), "value-changed", G_CALLBACK(draw_to_image_file_total_area_cb), current_window_pass_along);
	g_signal_connect(G_OBJECT(height_spin), "value-changed", G_CALLBACK(draw_to_image_file_total_area_cb), current_window_pass_along);
	g_signal_connect(G_OBJECT(zoom_combo), "changed", G_CALLBACK(draw_to_image_file_total_area_cb), current_window_pass_along);

	draw_to_image_file_total_area_cb(NULL, current_window_pass_along); /* set correct size info now */

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gtk_widget_hide(GTK_WIDGET(dialog));

		char *fn = draw_image_filename(this, img_gen);
		if (!fn) {
			return;
		}

		int active_z = gtk_combo_box_get_active(GTK_COMBO_BOX(zoom_combo));
		double zoom = pow(2, active_z-2);

		if (img_gen == VW_GEN_SINGLE_IMAGE) {
			save_image_file(this, fn,
					this->draw_image_width = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(width_spin)),
					this->draw_image_height = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(height_spin)),
					zoom,
					this->draw_image_save_as_png = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(png_radio)),
					false);
		} else if (img_gen == VW_GEN_KMZ_FILE) {
			// Remove some viewport overlays as these aren't useful in KMZ file.
			bool restore_xhair = this->viewport->get_draw_centermark();
			if (restore_xhair) {
				this->viewport->set_draw_centermark(false);
			}
			bool restore_scale = this->viewport->get_draw_scale();
			if (restore_scale) {
				this->viewport->set_draw_scale(false);
			}

			save_image_file(this,
					fn,
					gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(width_spin)),
					gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(height_spin)),
					zoom,
					false, // JPG
					true);

			if (restore_xhair) {
				this->viewport->set_draw_centermark(true);
			}

			if (restore_scale) {
				this->viewport->set_draw_scale(true);
			}

			if (restore_xhair || restore_scale) {
				this->draw_update();
			}
		} else {
			// NB is in UTM mode ATM
			this->save_image_dir(fn,
				       this->draw_image_width = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(width_spin)),
				       this->draw_image_height = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(height_spin)),
				       zoom,
				       this->draw_image_save_as_png = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(png_radio)),
				       gtk_spin_button_get_value(GTK_SPIN_BUTTON(tiles_width_spin)),
				       gtk_spin_button_get_value(GTK_SPIN_BUTTON(tiles_height_spin)));
		}

		free(fn);
	}
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void draw_to_kmz_file_cb(GtkAction * a, Window * window)
{
	if (window->viewport->get_coord_mode() == VIK_COORD_UTM) {
		a_dialog_error_msg(GTK_WINDOW(window->vw), _("This feature is not available in UTM mode"));
		return;
	}
	// NB ATM This only generates a KMZ file with the current viewport image - intended mostly for map images [but will include any lines/icons from track & waypoints that are drawn]
	// (it does *not* include a full KML dump of every track, waypoint etc...)
	window->draw_to_image_file(VW_GEN_KMZ_FILE);
}

static void draw_to_image_file_cb(GtkAction * a, Window * window)
{
	window->draw_to_image_file(VW_GEN_SINGLE_IMAGE);
}

static void draw_to_image_dir_cb(GtkAction * a, Window * window)
{
	window->draw_to_image_file(VW_GEN_DIRECTORY_OF_IMAGES);
}

/**
 *
 */
static void import_kmz_file_cb(GtkAction * a, Window * window)
{
	GtkWidget * dialog = gtk_file_chooser_dialog_new(_("Open File"),
							 GTK_WINDOW(window->vw),
							 GTK_FILE_CHOOSER_ACTION_OPEN,
							 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							 GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
							 NULL);

	GtkFileFilter * filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("KMZ"));
	gtk_file_filter_add_mime_type(filter, "vnd.google-earth.kmz");
	gtk_file_filter_add_pattern(filter, "*.kmz");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("All"));
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	// Default to any file - same as before open filters were added
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)  {
		char *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		// TODO convert ans value into readable explaination of failure...
		int ans = kmz_open_file(fn, window->viewport, window->layers_panel);
		if (ans) {
			a_dialog_error_msg_extra(GTK_WINDOW(window->vw), _("Unable to import %s."), fn);
		}

		window->draw_update();
	}
	gtk_widget_destroy(dialog);
}

static void print_cb(GtkAction * a, Window * window)
{
	a_print(window, window->viewport);
}

/* really a misnomer: changes coord mode (actual coordinates) AND/OR draw mode (viewport only) */
static void window_change_coord_mode_cb(GtkAction * old_a, GtkAction * a, Window * window)
{
	char const *name = gtk_action_get_name(a);
	GtkToggleToolButton *tbutton = (GtkToggleToolButton *)toolbar_get_widget_by_name(window->viking_vtb, name);
	if (tbutton) {
		gtk_toggle_tool_button_set_active(tbutton, true);
	}

	VikViewportDrawMode drawmode;
	if (!strcmp(name, "ModeUTM")) {
		drawmode = VIK_VIEWPORT_DRAWMODE_UTM;
	} else if (!strcmp(name, "ModeLatLon")) {
		drawmode = VIK_VIEWPORT_DRAWMODE_LATLON;
	} else if (!strcmp(name, "ModeExpedia")) {
		drawmode = VIK_VIEWPORT_DRAWMODE_EXPEDIA;
	} else if (!strcmp(name, "ModeMercator")) {
		drawmode = VIK_VIEWPORT_DRAWMODE_MERCATOR;
	} else {
		fprintf(stderr, "CRITICAL: Houston, we've had a problem.\n");
		return;
	}

	if (!window->only_updating_coord_mode_ui) {
		VikViewportDrawMode olddrawmode = window->viewport->get_drawmode();
		if (olddrawmode != drawmode) {
			/* this takes care of coord mode too */
			window->viewport->set_drawmode(drawmode);
			if (drawmode == VIK_VIEWPORT_DRAWMODE_UTM) {
				window->layers_panel->change_coord_mode(VIK_COORD_UTM);
			} else if (olddrawmode == VIK_VIEWPORT_DRAWMODE_UTM) {
				window->layers_panel->change_coord_mode(VIK_COORD_LATLON);
			}
			window->draw_update();
		}
	}
}

static void toggle_draw_scale(GtkAction * a, Window * window)
{
	window->toggle_draw_scale(a);
}

void Window::toggle_draw_scale(GtkAction * a)
{
	bool state = !this->viewport->get_draw_scale();
	GtkWidget * check_box = gtk_ui_manager_get_widget(this->uim, "/ui/MainMenu/View/SetShow/ShowScale");
	if (!check_box) {
		return;
	}
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_box), state);
	this->viewport->set_draw_scale(state);
	this->draw_update();
}

static void toggle_draw_centermark(GtkAction * a, Window * window)
{
	window->toggle_draw_centermark(a);
}

void Window::toggle_draw_centermark(GtkAction * a)
{
	bool state = !this->viewport->get_draw_centermark();
	GtkWidget * check_box = gtk_ui_manager_get_widget(this->uim, "/ui/MainMenu/View/SetShow/ShowCenterMark");
	if (!check_box) {
		return;
	}
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_box), state);
	this->viewport->set_draw_centermark(state);
	this->draw_update();
}

static void toggle_draw_highlight(GtkAction * a, Window * window)
{
	window->toggle_draw_highlight(a);
}

void Window::toggle_draw_highlight(GtkAction * a)
{
	bool state = !this->viewport->get_draw_highlight();
	GtkWidget * check_box = gtk_ui_manager_get_widget(this->uim, "/ui/MainMenu/View/SetShow/ShowHighlight");
	if (!check_box) {
		return;
	}
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_box), state);
	this->viewport->set_draw_highlight(state);
	this->draw_update();
}

static void set_bg_color(GtkAction * a, Window * window)
{
	GtkWidget * colorsd = gtk_color_selection_dialog_new(_("Choose a background color"));
	GdkColor * color = window->viewport->get_background_gdkcolor();
	gtk_color_selection_set_previous_color(GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), color);
	gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), color);
	if (gtk_dialog_run(GTK_DIALOG(colorsd)) == GTK_RESPONSE_OK) {
		gtk_color_selection_get_current_color(GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), color);
		window->viewport->set_background_gdkcolor(color);
		window->draw_update();
	}
	free(color);
	gtk_widget_destroy(colorsd);
}

static void set_highlight_color(GtkAction * a, Window * window)
{
	GtkWidget * colorsd = gtk_color_selection_dialog_new(_("Choose a track highlight color"));
	GdkColor * color = window->viewport->get_highlight_gdkcolor();
	gtk_color_selection_set_previous_color(GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), color);
	gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), color);
	if (gtk_dialog_run(GTK_DIALOG(colorsd)) == GTK_RESPONSE_OK) {
		gtk_color_selection_get_current_color(GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), color);
		window->viewport->set_highlight_gdkcolor(color);
		window->draw_update();
	}
	free(color);
	gtk_widget_destroy(colorsd);
}


/***********************************************************************************************
 ** GUI Creation
 ***********************************************************************************************/

static GtkActionEntry entries[] = {
	{ "File",     NULL, N_("_File"),     0, 0, 0 },
	{ "Edit",     NULL, N_("_Edit"),     0, 0, 0 },
	{ "View",     NULL, N_("_View"),     0, 0, 0 },
	{ "SetShow",  NULL, N_("_Show"),     0, 0, 0 },
	{ "SetZoom",  NULL, N_("_Zoom"),     0, 0, 0 },
	{ "SetPan",   NULL, N_("_Pan"),      0, 0, 0 },
	{ "Layers",   NULL, N_("_Layers"),   0, 0, 0 },
	{ "Tools",    NULL, N_("_Tools"),    0, 0, 0 },
	{ "Exttools", NULL, N_("_Webtools"), 0, 0, 0 },
	{ "Help",     NULL, N_("_Help"),     0, 0, 0 },

	{ "New",                 GTK_STOCK_NEW,            N_("_New"),                               "<control>N",       N_("New file"),                                           (GCallback) newwindow_cb              },
	{ "Open",                GTK_STOCK_OPEN,           N_("_Open..."),                           "<control>O",       N_("Open a file"),                                        (GCallback) load_file                 },
	{ "OpenRecentFile",      NULL,                     N_("Open _Recent File"),                  NULL,               NULL,                                                     (GCallback) NULL                      },
	{ "Append",              GTK_STOCK_ADD,            N_("Append _File..."),                    NULL,               N_("Append data from a different file"),                  (GCallback) load_file                 },
	{ "Export",              GTK_STOCK_CONVERT,        N_("_Export All"),                        NULL,               N_("Export All TrackWaypoint Layers"),                    (GCallback) NULL                      },
	{ "ExportGPX",           NULL,                     N_("_GPX..."),           	             NULL,               N_("Export as GPX"),                                      (GCallback) export_to_gpx_cb          },
	{ "Acquire",             GTK_STOCK_GO_DOWN,        N_("A_cquire"),                           NULL,               NULL,                                                     (GCallback) NULL                      },
	{ "AcquireGPS",          NULL,                     N_("From _GPS..."),                       NULL,               N_("Transfer data from a GPS device"),                    (GCallback) acquire_from_gps          },
	{ "AcquireGPSBabel",     NULL,                     N_("Import File With GPS_Babel..."),      NULL,               N_("Import file via GPSBabel converter"),                 (GCallback) acquire_from_file         },
	{ "AcquireRouting",      NULL,                     N_("_Directions..."),                     NULL,               N_("Get driving directions"),                             (GCallback) acquire_from_routing      },
#ifdef VIK_CONFIG_OPENSTREETMAP
	{ "AcquireOSM",          NULL,                     N_("_OSM Traces..."),                     NULL,               N_("Get traces from OpenStreetMap"),                      (GCallback) acquire_from_osm          },
	{ "AcquireMyOSM",        NULL,                     N_("_My OSM Traces..."),      	     NULL,               N_("Get Your Own Traces from OpenStreetMap"),             (GCallback) acquire_from_my_osm       },
#endif
#ifdef VIK_CONFIG_GEOCACHES
	{ "AcquireGC",           NULL,                     N_("Geo_caches..."),    	             NULL,               N_("Get Geocaches from geocaching.com"),                  (GCallback) acquire_from_gc           },
#endif
#ifdef VIK_CONFIG_GEOTAG
	{ "AcquireGeotag",       NULL,                     N_("From Geotagged _Images..."),          NULL,               N_("Create waypoints from geotagged images"),             (GCallback) acquire_from_geotag       },
#endif
	{ "AcquireURL",          NULL,                     N_("From _URL..."),                       NULL,               N_("Get a file from a URL"),                              (GCallback) acquire_from_url          },
#ifdef VIK_CONFIG_GEONAMES
	{ "AcquireWikipedia",    NULL,                     N_("From _Wikipedia Waypoints"),          NULL,               N_("Create waypoints from Wikipedia items in the current view"), (GCallback) acquire_from_wikipedia },
#endif
	{ "Save",                GTK_STOCK_SAVE,           N_("_Save"),                              "<control>S",       N_("Save the file"),                                      (GCallback) save_file                 },
	{ "SaveAs",              GTK_STOCK_SAVE_AS,        N_("Save _As..."),                        NULL,               N_("Save the file under different name"),                 (GCallback) save_file_as              },
	{ "FileProperties",      NULL,                     N_("Properties..."),                      NULL,               N_("File Properties"),                                    (GCallback) file_properties_cb        },
#ifdef HAVE_ZIP_H
	{ "ImportKMZ",           GTK_STOCK_CONVERT,        N_("Import KMZ _Map File..."),            NULL,               N_("Import a KMZ file"), (GCallback)import_kmz_file_cb },
	{ "GenKMZ",              GTK_STOCK_DND,            N_("Generate _KMZ Map File..."),          NULL,               N_("Generate a KMZ file with an overlay of the current view"), (GCallback) draw_to_kmz_file_cb  },
#endif
	{ "GenImg",              GTK_STOCK_FILE,           N_("_Generate Image File..."),            NULL,               N_("Save a snapshot of the workspace into a file"),       (GCallback) draw_to_image_file_cb     },
	{ "GenImgDir",           GTK_STOCK_DND_MULTIPLE,   N_("Generate _Directory of Images..."),   NULL,               N_("Generate _Directory of Images"),                      (GCallback) draw_to_image_dir_cb      },
	{ "Print",               GTK_STOCK_PRINT,          N_("_Print..."),                          NULL,               N_("Print maps"),                                         (GCallback) print_cb                  },
	{ "Exit",                GTK_STOCK_QUIT,           N_("E_xit"),                              "<control>W",       N_("Exit the program"),                                   (GCallback) window_close              },
	{ "SaveExit",            GTK_STOCK_QUIT,           N_("Save and Exit"),                      NULL,               N_("Save and Exit the program"),                          (GCallback) save_file_and_exit        },

	{ "GoBack",              GTK_STOCK_GO_BACK,        N_("Go to the Pre_vious Location"),       NULL,               N_("Go to the previous location"),                        (GCallback) draw_goto_back_and_forth  },
	{ "GoForward",           GTK_STOCK_GO_FORWARD,     N_("Go to the _Next Location"),           NULL,               N_("Go to the next location"),                            (GCallback) draw_goto_back_and_forth  },
	{ "GotoDefaultLocation", GTK_STOCK_HOME,           N_("Go to the _Default Location"),        NULL,               N_("Go to the default location"),                         (GCallback) goto_default_location     },
	{ "GotoSearch",          GTK_STOCK_JUMP_TO,        N_("Go to _Location..."),    	     NULL,               N_("Go to address/place using text search"),              (GCallback) goto_address              },
	{ "GotoLL",              GTK_STOCK_JUMP_TO,        N_("_Go to Lat/Lon..."),                  NULL,               N_("Go to arbitrary lat/lon coordinate"),                 (GCallback) draw_goto_cb              },
	{ "GotoUTM",             GTK_STOCK_JUMP_TO,        N_("Go to UTM..."),                       NULL,               N_("Go to arbitrary UTM coordinate"),                     (GCallback) draw_goto_cb              },
	{ "Refresh",             GTK_STOCK_REFRESH,        N_("_Refresh"),                           "F5",               N_("Refresh any maps displayed"),                         (GCallback) draw_refresh_cb           },
	{ "SetHLColor",          GTK_STOCK_SELECT_COLOR,   N_("Set _Highlight Color..."),            NULL,               N_("Set Highlight Color"),                                (GCallback) set_highlight_color       },
	{ "SetBGColor",          GTK_STOCK_SELECT_COLOR,   N_("Set Bac_kground Color..."),           NULL,               N_("Set Background Color"),                               (GCallback) set_bg_color              },
	{ "ZoomIn",              GTK_STOCK_ZOOM_IN,        N_("Zoom _In"),                           "<control>plus",    N_("Zoom In"),                                            (GCallback) draw_zoom_cb              },
	{ "ZoomOut",             GTK_STOCK_ZOOM_OUT,       N_("Zoom _Out"),                          "<control>minus",   N_("Zoom Out"),                                           (GCallback) draw_zoom_cb              },
	{ "ZoomTo",              GTK_STOCK_ZOOM_FIT,       N_("Zoom _To..."),                        "<control>Z",       N_("Zoom To"),                                            (GCallback) zoom_to_cb                },
	{ "PanNorth",            NULL,                     N_("Pan _North"),                         "<control>Up",      NULL,                                                     (GCallback) draw_pan_cb               },
	{ "PanEast",             NULL,                     N_("Pan _East"),                          "<control>Right",   NULL,                                                     (GCallback) draw_pan_cb               },
	{ "PanSouth",            NULL,                     N_("Pan _South"),                         "<control>Down",    NULL,                                                     (GCallback) draw_pan_cb               },
	{ "PanWest",             NULL,                     N_("Pan _West"),                          "<control>Left",    NULL,                                                     (GCallback) draw_pan_cb               },
	{ "BGJobs",              GTK_STOCK_EXECUTE,        N_("Background _Jobs"),                   NULL,               N_("Background Jobs"),                                    (GCallback) a_background_show_window  },

	{ "Cut",                 GTK_STOCK_CUT,            N_("Cu_t"),                               NULL,               N_("Cut selected layer"),                                 (GCallback) menu_cut_layer_cb         },
	{ "Copy",                GTK_STOCK_COPY,           N_("_Copy"),                              NULL,               N_("Copy selected layer"),                                (GCallback) menu_copy_layer_cb        },
	{ "Paste",               GTK_STOCK_PASTE,          N_("_Paste"),                             NULL,               N_("Paste layer into selected container layer or otherwise above selected layer"), (GCallback) menu_paste_layer_cb },
	{ "Delete",              GTK_STOCK_DELETE,         N_("_Delete"),                            NULL,               N_("Remove selected layer"),                              (GCallback) menu_delete_layer_cb      },
	{ "DeleteAll",           NULL,                     N_("Delete All"),                         NULL,               NULL,                                                     (GCallback) clear_cb                  },
	{ "CopyCentre",          NULL,                     N_("Copy Centre _Location"),              "<control>h",       NULL,                                                     (GCallback) menu_copy_centre_cb       },
	{ "MapCacheFlush",       NULL,                     N_("_Flush Map Cache"),                   NULL,               NULL,                                                     (GCallback) mapcache_flush_cb         },
	{ "SetDefaultLocation",  GTK_STOCK_GO_FORWARD,     N_("_Set the Default Location"),          NULL,               N_("Set the Default Location to the current position"),   (GCallback) default_location_cb       },
	{ "Preferences",         GTK_STOCK_PREFERENCES,    N_("_Preferences"),                       NULL,               N_("Program Preferences"),                                (GCallback) preferences_cb            },
	{ "LayerDefaults",       GTK_STOCK_PROPERTIES,     N_("_Layer Defaults"),                    NULL,               NULL,                                                     NULL                                  },
	{ "Properties",          GTK_STOCK_PROPERTIES,     N_("_Properties"),                        NULL,               N_("Layer Properties"),                                   (GCallback) menu_properties_cb        },

	{ "HelpEntry",           GTK_STOCK_HELP,           N_("_Help"),                              "F1",               N_("Help"),                                               (GCallback) help_help_cb              },
	{ "About",               GTK_STOCK_ABOUT,          N_("_About"),                             NULL,               N_("About"),                                              (GCallback) help_about_cb             },
};

static GtkActionEntry debug_entries[] = {
	{ "MapCacheInfo", NULL,                "_Map Cache Info",                   NULL,         NULL,                                           (GCallback)help_cache_info_cb    },
	{ "BackForwardInfo", NULL,             "_Back/Forward Info",                NULL,         NULL,                                           (GCallback)back_forward_info_cb  },
};

static GtkActionEntry entries_gpsbabel[] = {
	{ "ExportKML", NULL,                   N_("_KML..."),           	      NULL,         N_("Export as KML"),                                (GCallback)export_to_kml_cb },
};

static GtkActionEntry entries_geojson[] = {
	{ "AcquireGeoJSON",   NULL,            N_("Import Geo_JSON File..."),   NULL,         N_("Import GeoJSON file"),                          (GCallback)acquire_from_geojson },
};

/* Radio items */
static GtkRadioActionEntry mode_entries[] = {
	{ "ModeUTM",         NULL,         N_("_UTM Mode"),               "<control>u", NULL, VIK_VIEWPORT_DRAWMODE_UTM },
	{ "ModeExpedia",     NULL,         N_("_Expedia Mode"),           "<control>e", NULL, VIK_VIEWPORT_DRAWMODE_EXPEDIA },
	{ "ModeMercator",    NULL,         N_("_Mercator Mode"),          "<control>m", NULL, VIK_VIEWPORT_DRAWMODE_MERCATOR },
	{ "ModeLatLon",      NULL,         N_("Lat_/Lon Mode"),           "<control>l", NULL, VIK_VIEWPORT_DRAWMODE_LATLON },
};

static GtkToggleActionEntry toggle_entries[] = {
	{ "ShowScale",      NULL,                 N_("Show _Scale"),               "<shift>F5",  N_("Show Scale"),                              (GCallback)toggle_draw_scale, true },
	{ "ShowCenterMark", NULL,                 N_("Show _Center Mark"),         "F6",         N_("Show Center Mark"),                        (GCallback)toggle_draw_centermark, true },
	{ "ShowHighlight",  GTK_STOCK_UNDERLINE,  N_("Show _Highlight"),           "F7",         N_("Show Highlight"),                          (GCallback)toggle_draw_highlight, true },
	{ "FullScreen",     GTK_STOCK_FULLSCREEN, N_("_Full Screen"),              "F11",        N_("Activate full screen mode"),               (GCallback)full_screen_cb, false },
	{ "ViewSidePanel",  GTK_STOCK_INDEX,      N_("Show Side _Panel"),          "F9",         N_("Show Side Panel"),                         (GCallback)view_side_panel_cb, true },
	{ "ViewStatusBar",  NULL,                 N_("Show Status_bar"),           "F12",        N_("Show Statusbar"),                          (GCallback)view_statusbar_cb, true },
	{ "ViewToolbar",    NULL,                 N_("Show _Toolbar"),             "F3",         N_("Show Toolbar"),                            (GCallback)view_toolbar_cb, true },
	{ "ViewMainMenu",   NULL,                 N_("Show _Menu"),                "F4",         N_("Show Menu"),                               (GCallback)view_main_menu_cb, true },
};

// This must match the toggle entries order above
static GCallback toggle_entries_toolbar_cb[] = {
	(GCallback) tb_set_draw_scale_cb,
	(GCallback) tb_set_draw_centermark_cb,
	(GCallback) tb_set_draw_highlight_cb,
	(GCallback) tb_full_screen_cb,
	(GCallback) tb_view_side_panel_cb,
	(GCallback) tb_view_statusbar_cb,
	(GCallback) tb_view_toolbar_cb,
	(GCallback) tb_view_main_menu_cb,
};

#include "menu.xml.h"
static void window_create_ui(Window * window)
{
	GtkIconSet * icon_set;
	GtkRadioActionEntry *tools = NULL, *radio;

	GtkUIManager * uim = gtk_ui_manager_new();
	window->uim = uim;

	toolbox_add_tool(window->vt, &ruler_tool, TOOL_LAYER_TYPE_NONE);
	toolbox_add_tool(window->vt, &zoom_tool, TOOL_LAYER_TYPE_NONE);
	toolbox_add_tool(window->vt, &pan_tool, TOOL_LAYER_TYPE_NONE);
	toolbox_add_tool(window->vt, &select_tool, TOOL_LAYER_TYPE_NONE);

	toolbar_action_tool_entry_register(window->viking_vtb, &pan_tool.radioActionEntry);
	toolbar_action_tool_entry_register(window->viking_vtb, &zoom_tool.radioActionEntry);
	toolbar_action_tool_entry_register(window->viking_vtb, &ruler_tool.radioActionEntry);
	toolbar_action_tool_entry_register(window->viking_vtb, &select_tool.radioActionEntry);

	GError * error = NULL;
	unsigned int mid;
	if (!(mid = gtk_ui_manager_add_ui_from_string(uim, menu_xml, -1, &error))) {
		g_error_free(error);
		exit(1);
	}

	GtkActionGroup * action_group = gtk_action_group_new("MenuActions");
	gtk_action_group_set_translation_domain(action_group, PACKAGE_NAME);
	gtk_action_group_add_actions(action_group, entries, G_N_ELEMENTS (entries), window);
	gtk_action_group_add_toggle_actions(action_group, toggle_entries, G_N_ELEMENTS (toggle_entries), window);
	gtk_action_group_add_radio_actions(action_group, mode_entries, G_N_ELEMENTS (mode_entries), 4, (GCallback)window_change_coord_mode_cb, window);
	if (vik_debug) {
		if (gtk_ui_manager_add_ui_from_string(uim,
						      "<ui><menubar name='MainMenu'><menu action='Help'>"
						      "<menuitem action='MapCacheInfo'/>"
						      "<menuitem action='BackForwardInfo'/>"
						      "</menu></menubar></ui>",
						      -1, NULL)) {
			gtk_action_group_add_actions(action_group, debug_entries, G_N_ELEMENTS (debug_entries), window);
		}
	}

	for (unsigned int i = 0; i < G_N_ELEMENTS (entries); i++) {
		if (entries[i].callback) {
			toolbar_action_entry_register(window->viking_vtb, &entries[i]);
		}
	}

	if (G_N_ELEMENTS (toggle_entries) !=  G_N_ELEMENTS (toggle_entries_toolbar_cb)) {
		fprintf(stdout,  "Broken entries definitions\n");
		exit(1);
	}
	for (unsigned int i = 0; i < G_N_ELEMENTS (toggle_entries); i++) {
		if (toggle_entries_toolbar_cb[i]) {
			toolbar_action_toggle_entry_register(window->viking_vtb, &toggle_entries[i], (void *) toggle_entries_toolbar_cb[i]);
		}
	}

	for (unsigned int i = 0; i < G_N_ELEMENTS (mode_entries); i++) {
		toolbar_action_mode_entry_register(window->viking_vtb, &mode_entries[i]);
	}

	// Use this to see if GPSBabel is available:
	if (a_babel_available()) {
		// If going to add more entries then might be worth creating a menu_gpsbabel.xml.h file
		if (gtk_ui_manager_add_ui_from_string(uim,
						      "<ui><menubar name='MainMenu'><menu action='File'><menu action='Export'><menuitem action='ExportKML'/></menu></menu></menubar></ui>",
						      -1, &error)) {
			gtk_action_group_add_actions(action_group, entries_gpsbabel, G_N_ELEMENTS (entries_gpsbabel), window);
		}
	}

	// GeoJSON import capability
	if (g_find_program_in_path(a_geojson_program_import())) {
		if (gtk_ui_manager_add_ui_from_string(uim,
						      "<ui><menubar name='MainMenu'><menu action='File'><menu action='Acquire'><menuitem action='AcquireGeoJSON'/></menu></menu></menubar></ui>",
						      -1, &error)) {
			gtk_action_group_add_actions(action_group, entries_geojson, G_N_ELEMENTS (entries_geojson), window);
		}
	}

	GtkIconFactory * icon_factory = gtk_icon_factory_new();
	gtk_icon_factory_add_default(icon_factory);

	register_vik_icons(icon_factory);

	// Copy the tool RadioActionEntries out of the main Window structure into an extending array 'tools'
	//  so that it can be applied to the UI in one action group add function call below
	unsigned int ntools = 0;
	for (unsigned int i = 0; i < window->vt->n_tools; i++) {
		tools = g_renew(GtkRadioActionEntry, tools, ntools+1);
		radio = &tools[ntools];
		ntools++;
		*radio = window->vt->tools[i].ti.radioActionEntry;
		radio->value = ntools;
	}

	for (int i = 0; i<VIK_LAYER_NUM_TYPES; i++) {
		GtkActionEntry action;
		gtk_ui_manager_add_ui(uim, mid,  "/ui/MainMenu/Layers/",
				      vik_layer_get_interface((VikLayerTypeEnum) i)->name,
				      vik_layer_get_interface((VikLayerTypeEnum) i)->name,
				      GTK_UI_MANAGER_MENUITEM, false);

		icon_set = gtk_icon_set_new_from_pixbuf(gdk_pixbuf_from_pixdata(vik_layer_get_interface((VikLayerTypeEnum) i)->icon, false, NULL));
		gtk_icon_factory_add(icon_factory, vik_layer_get_interface((VikLayerTypeEnum) i)->name, icon_set);
		gtk_icon_set_unref(icon_set);

		action.name = vik_layer_get_interface((VikLayerTypeEnum) i)->name;
		action.stock_id = vik_layer_get_interface((VikLayerTypeEnum) i)->name;
		action.label = g_strdup_printf(_("New _%s Layer"), vik_layer_get_interface((VikLayerTypeEnum) i)->name);
		action.accelerator = vik_layer_get_interface((VikLayerTypeEnum) i)->accelerator;
		action.tooltip = NULL;
		action.callback = (GCallback)menu_addlayer_cb;
		gtk_action_group_add_actions(action_group, &action, 1, window);

		free((char*)action.label);

		if (vik_layer_get_interface((VikLayerTypeEnum) i)->tools_count) {
			gtk_ui_manager_add_ui(uim, mid,  "/ui/MainMenu/Tools/", vik_layer_get_interface((VikLayerTypeEnum) i)->name, NULL, GTK_UI_MANAGER_SEPARATOR, false);
		}

		// Further tool copying for to apply to the UI, also apply menu UI setup
		for (unsigned int j = 0; j < vik_layer_get_interface((VikLayerTypeEnum) i)->tools_count; j++) {
			tools = g_renew(GtkRadioActionEntry, tools, ntools+1);
			radio = &tools[ntools];
			ntools++;

			gtk_ui_manager_add_ui(uim, mid,  "/ui/MainMenu/Tools",
					      vik_layer_get_interface((VikLayerTypeEnum) i)->tools[j].radioActionEntry.label,
					      vik_layer_get_interface((VikLayerTypeEnum) i)->tools[j].radioActionEntry.name,
					      GTK_UI_MANAGER_MENUITEM, false);

			toolbox_add_tool(window->vt, &(vik_layer_get_interface((VikLayerTypeEnum) i)->tools[j]), i);
			toolbar_action_tool_entry_register(window->viking_vtb, &(vik_layer_get_interface((VikLayerTypeEnum) i)->tools[j].radioActionEntry));

			*radio = vik_layer_get_interface((VikLayerTypeEnum) i)->tools[j].radioActionEntry;
			// Overwrite with actual number to use
			radio->value = ntools;
		}

		GtkActionEntry action_dl;
		char *layername = g_strdup_printf("Layer%s", vik_layer_get_interface((VikLayerTypeEnum) i)->fixed_layer_name);
		gtk_ui_manager_add_ui(uim, mid,  "/ui/MainMenu/Edit/LayerDefaults",
				      vik_layer_get_interface((VikLayerTypeEnum) i)->name,
				      layername,
				      GTK_UI_MANAGER_MENUITEM, false);
		free(layername);

		// For default layers use action names of the form 'Layer<LayerName>'
		// This is to avoid clashing with just the layer name used above for the tool actions
		action_dl.name = g_strconcat("Layer", vik_layer_get_interface((VikLayerTypeEnum) i)->fixed_layer_name, NULL);
		action_dl.stock_id = NULL;
		action_dl.label = g_strconcat("_", vik_layer_get_interface((VikLayerTypeEnum) i)->name, "...", NULL); // Prepend marker for keyboard accelerator
		action_dl.accelerator = NULL;
		action_dl.tooltip = NULL;
		action_dl.callback = (GCallback)layer_defaults_cb;
		gtk_action_group_add_actions(action_group, &action_dl, 1, window);
		free((char*)action_dl.name);
		free((char*)action_dl.label);
	}
	g_object_unref(icon_factory);

	gtk_action_group_add_radio_actions(action_group, tools, ntools, 0, (GCallback) menu_cb, window);
	free(tools);

	gtk_ui_manager_insert_action_group(uim, action_group, 0);

	for (unsigned int i = 0; i<VIK_LAYER_NUM_TYPES; i++) {
		for (unsigned int j = 0; j < vik_layer_get_interface((VikLayerTypeEnum) i)->tools_count; j++) {
			GtkAction *action = gtk_action_group_get_action(action_group,
									vik_layer_get_interface((VikLayerTypeEnum) i)->tools[j].radioActionEntry.name);
			g_object_set(action, "sensitive", false, NULL);
		}
	}

	// This is done last so we don't need to track the value of mid anymore
	vik_ext_tools_add_action_items(window, window->uim, action_group, mid);

	window->action_group = action_group;

	GtkAccelGroup * accel_group = gtk_ui_manager_get_accel_group(uim);
	gtk_window_add_accel_group(GTK_WINDOW (window), accel_group);
	gtk_ui_manager_ensure_update(uim);

	window->setup_recent_files();
}


// TODO - add method to add tool icons defined from outside this file
//  and remove the reverse dependency on icon definition from this file
static struct {
	const GdkPixdata *data;
	char *stock_id;
} stock_icons[] = {
	{ &mover_22_pixbuf,		(char *) "vik-icon-pan"              },
	{ &zoom_18_pixbuf,		(char *) "vik-icon-zoom"             },
	{ &ruler_18_pixbuf,		(char *) "vik-icon-ruler"            },
	{ &select_18_pixbuf,		(char *) "vik-icon-select"           },
	{ &vik_new_route_18_pixbuf,     (char *) "vik-icon-Create Route"     },
	{ &route_finder_18_pixbuf,	(char *) "vik-icon-Route Finder"     },
	{ &demdl_18_pixbuf,		(char *) "vik-icon-DEM Download"     },
	{ &showpic_18_pixbuf,		(char *) "vik-icon-Show Picture"     },
	{ &addtr_18_pixbuf,		(char *) "vik-icon-Create Track"     },
	{ &edtr_18_pixbuf,		(char *) "vik-icon-Edit Trackpoint"  },
	{ &addwp_18_pixbuf,		(char *) "vik-icon-Create Waypoint"  },
	{ &edwp_18_pixbuf,		(char *) "vik-icon-Edit Waypoint"    },
	{ &geozoom_18_pixbuf,		(char *) "vik-icon-Georef Zoom Tool" },
	{ &geomove_18_pixbuf,		(char *) "vik-icon-Georef Move Map"  },
	{ &mapdl_18_pixbuf,		(char *) "vik-icon-Maps Download"    },
};

static int n_stock_icons = G_N_ELEMENTS (stock_icons);

static void
register_vik_icons(GtkIconFactory *icon_factory)
{
	GtkIconSet *icon_set;

	for (int i = 0; i < n_stock_icons; i++) {
		icon_set = gtk_icon_set_new_from_pixbuf(gdk_pixbuf_from_pixdata(stock_icons[i].data, false, NULL));
		gtk_icon_factory_add(icon_factory, stock_icons[i].stock_id, icon_set);
		gtk_icon_set_unref(icon_set);
	}
}

LayerTRW * Window::get_selected_trw_layer()
{
	return this->selected_trw;
}

void Window::set_selected_trw_layer(LayerTRW * trw)
{
	this->selected_trw       = trw;
	this->containing_trw     = trw;
	/* Clear others */
	this->selected_track     = NULL;
	this->selected_tracks    = NULL;
	this->selected_waypoint  = NULL;
	this->selected_waypoints = NULL;
	// Set highlight thickness
	this->viewport->set_highlight_thickness(this->containing_trw->get_property_tracks_line_thickness());
}

Tracks * Window::get_selected_tracks()
{
	return this->selected_tracks;
}

void Window::set_selected_tracks(Tracks * tracks, LayerTRW * trw)
{
	this->selected_tracks    = tracks;
	this->containing_trw     = trw;
	/* Clear others */
	this->selected_trw       = NULL;
	this->selected_track     = NULL;
	this->selected_waypoint  = NULL;
	this->selected_waypoints = NULL;
	// Set highlight thickness
	this->viewport->set_highlight_thickness(this->containing_trw->get_property_tracks_line_thickness());
}

Track * Window::get_selected_track()
{
	return this->selected_track;
}

void Window::set_selected_track(Track * track, LayerTRW * trw)
{
	this->selected_track     = track;
	this->containing_trw     = trw;
	/* Clear others */
	this->selected_trw       = NULL;
	this->selected_tracks    = NULL;
	this->selected_waypoint  = NULL;
	this->selected_waypoints = NULL;
	// Set highlight thickness
	this->viewport->set_highlight_thickness(this->containing_trw->get_property_tracks_line_thickness());
}

Waypoints * Window::get_selected_waypoints()
{
	return this->selected_waypoints;
}

void Window::set_selected_waypoints(Waypoints * waypoints, LayerTRW * trw)
{
	this->selected_waypoints = waypoints;
	this->containing_trw     = trw;
	/* Clear others */
	this->selected_trw       = NULL;
	this->selected_track     = NULL;
	this->selected_tracks    = NULL;
	this->selected_waypoint  = NULL;
}

Waypoint * Window::get_selected_waypoint()
{
	return this->selected_waypoint;
}

void Window::set_selected_waypoint(Waypoint * wp, LayerTRW * trw)
{
	this->selected_waypoint  = wp;
	this->containing_trw     = trw;
	/* Clear others */
	this->selected_trw       = NULL;
	this->selected_track     = NULL;
	this->selected_tracks    = NULL;
	this->selected_waypoints = NULL;
}

bool vik_window_clear_highlight_cb(Window * window)
{
	return window->clear_highlight();
}

bool Window::clear_highlight()
{
	bool need_redraw = false;
	if (this->selected_trw != NULL) {
		this->selected_trw = NULL;
		need_redraw = true;
	}
	if (this->selected_track != NULL) {
		this->selected_track = NULL;
		need_redraw = true;
	}
	if (this->selected_tracks != NULL) {
		this->selected_tracks = NULL;
		need_redraw = true;
	}
	if (this->selected_waypoint != NULL) {
		this->selected_waypoint = NULL;
		need_redraw = true;
	}
	if (this->selected_waypoints != NULL) {
		this->selected_waypoints = NULL;
		need_redraw = true;
	}
	return need_redraw;
}

/**
 * May return NULL if the window no longer exists
 */
GThread * Window::get_thread()
{
	if (this->vw) {
		return this->thread;
	}
	return NULL;
}



Window::Window()
{
	this->selected_trw = NULL;
	this->selected_tracks = NULL;
	this->selected_track = NULL;
	this->selected_waypoints = NULL;
	this->selected_waypoint = NULL;
	this->containing_trw = NULL;

	this->viewport = NULL;

	this->filename = NULL;

	strcpy(this->type_string, "The WINDOW");


	this->vw = (VikWindow *) g_object_new(VIK_WINDOW_TYPE, NULL);
	((VikWindow *) this->vw)->window = this;



	this->action_group = NULL;
	this->viewport = new Viewport();
	this->layers_panel = new LayersPanel();
	this->layers_panel->set_viewport(this->viewport);
	this->viking_vs = vik_statusbar_new();

	this->vt = toolbox_create(this);
	this->viking_vtb = vik_toolbar_new();
	window_create_ui(this);
	this->set_filename(NULL);

	this->busy_cursor = gdk_cursor_new(GDK_WATCH);

	this->loaded_type = LOAD_TYPE_READ_FAILURE; //AKA none
	this->modified = false;
	this->only_updating_coord_mode_ui = false;

	this->select_move = false;
	this->pan_move = false;
	this->pan_x = this->pan_y = -1;
	this->single_click_pending = false;

	int draw_image_width;
	if (a_settings_get_integer(VIK_SETTINGS_WIN_SAVE_IMAGE_WIDTH, &draw_image_width)) {
		this->draw_image_width = draw_image_width;
	} else {
		this->draw_image_width = DRAW_IMAGE_DEFAULT_WIDTH;
	}
	int draw_image_height;
	if (a_settings_get_integer(VIK_SETTINGS_WIN_SAVE_IMAGE_HEIGHT, &draw_image_height)) {
		this->draw_image_height = draw_image_height;
	} else {
		this->draw_image_height = DRAW_IMAGE_DEFAULT_HEIGHT;
	}
	bool draw_image_save_as_png;
	if (a_settings_get_boolean(VIK_SETTINGS_WIN_SAVE_IMAGE_PNG, &draw_image_save_as_png)) {
		this->draw_image_save_as_png = draw_image_save_as_png;
	} else {
		this->draw_image_save_as_png = DRAW_IMAGE_DEFAULT_SAVE_AS_PNG;
	}

	this->main_vbox = gtk_vbox_new(false, 1);
	gtk_container_add(GTK_CONTAINER (this->vw), this->main_vbox);
	this->menu_hbox = gtk_hbox_new(false, 1);
	GtkWidget *menu_bar = gtk_ui_manager_get_widget(this->uim, "/MainMenu");
	gtk_box_pack_start(GTK_BOX(this->menu_hbox), menu_bar, false, true, 0);
	gtk_box_pack_start(GTK_BOX(this->main_vbox), this->menu_hbox, false, true, 0);

	toolbar_init(this->viking_vtb,
		     &((VikWindow *) this->vw)->gtkwindow,
		     this->main_vbox,
		     this->menu_hbox,
		     toolbar_tool_cb,
		     toolbar_reload_cb,
		     (void *) this); // This auto packs toolbar into the vbox
	// Must be performed post toolbar init
	for (int i = 0; i < VIK_LAYER_NUM_TYPES; i++) {
		for (int j = 0; j < vik_layer_get_interface((VikLayerTypeEnum) i)->tools_count; j++) {
			toolbar_action_set_sensitive(this->viking_vtb, vik_layer_get_interface((VikLayerTypeEnum) i)->tools[j].radioActionEntry.name, false);
		}
	}

	vik_ext_tool_datasources_add_menu_items(this, this->uim);

	GtkWidget * zoom_levels = gtk_ui_manager_get_widget(this->uim, "/MainMenu/View/SetZoom");
	GtkWidget * zoom_levels_menu = create_zoom_menu_all_levels(this->viewport->get_zoom());
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(zoom_levels), zoom_levels_menu);
	g_signal_connect(G_OBJECT(zoom_levels_menu), "selection-done", G_CALLBACK(zoom_changed_cb), this);
	g_signal_connect_swapped(G_OBJECT(this->viking_vs), "clicked", G_CALLBACK(zoom_popup_handler), zoom_levels_menu);

	g_signal_connect(G_OBJECT (this->vw), "delete_event", G_CALLBACK (delete_event), NULL);

	// Own signals
	g_signal_connect_swapped(G_OBJECT(this->viewport->vvp), "updated_center", G_CALLBACK(center_changed_cb), this);
	// Signals from GTK
	g_signal_connect_swapped(G_OBJECT(this->viewport->vvp), "expose_event", G_CALLBACK(draw_sync_cb), this);
	g_signal_connect_swapped(G_OBJECT(this->viewport->vvp), "configure_event", G_CALLBACK(window_configure_event), this);
	gtk_widget_add_events(GTK_WIDGET(this->viewport->vvp), GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK);
	g_signal_connect_swapped(G_OBJECT(this->viewport->vvp), "scroll_event", G_CALLBACK(draw_scroll_cb), this);
	int a = g_signal_connect_swapped(G_OBJECT(this->viewport->vvp), "button_press_event", G_CALLBACK(draw_click_cb), this);
	g_signal_connect_swapped(G_OBJECT(this->viewport->vvp), "button_release_event", G_CALLBACK(draw_release_cb), this);
	g_signal_connect_swapped(G_OBJECT(this->viewport->vvp), "motion_notify_event", G_CALLBACK(draw_mouse_motion_cb), this);

	g_signal_connect_swapped(G_OBJECT(this->layers_panel->gob), "update", G_CALLBACK(draw_update_cb), this);
	g_signal_connect_swapped(G_OBJECT(this->layers_panel->gob), "delete_layer", G_CALLBACK(vik_window_clear_highlight_cb), this);

	// Allow key presses to be processed anywhere
	g_signal_connect_swapped(G_OBJECT (this->vw), "key_press_event", G_CALLBACK (key_press_event_cb), this);

	// Set initial button sensitivity
	center_changed_cb(this);

	this->hpaned = gtk_hpaned_new();
	gtk_paned_pack1(GTK_PANED(this->hpaned), GTK_WIDGET (this->layers_panel->gob), false, true);
	gtk_paned_pack2(GTK_PANED(this->hpaned), GTK_WIDGET (this->viewport->vvp), true, true);

	/* This packs the button into the window (a gtk container). */
	gtk_box_pack_start(GTK_BOX(this->main_vbox), this->hpaned, true, true, 0);

	gtk_box_pack_end(GTK_BOX(this->main_vbox), GTK_WIDGET(this->viking_vs), false, true, 0);

	a_background_add_window(this);

	window_list.push_front(this);

	int height = VIKING_WINDOW_HEIGHT;
	int width = VIKING_WINDOW_WIDTH;

	if (a_vik_get_restore_window_state()) {
		if (a_settings_get_integer(VIK_SETTINGS_WIN_HEIGHT, &height)) {
			// Enforce a basic minimum size
			if (height < 160) {
				height = 160;
			}
		} else {
			// No setting - so use default
			height = VIKING_WINDOW_HEIGHT;
		}

		if (a_settings_get_integer(VIK_SETTINGS_WIN_WIDTH, &width)) {
			// Enforce a basic minimum size
			if (width < 320) {
				width = 320;
			}
		} else {
			// No setting - so use default
			width = VIKING_WINDOW_WIDTH;
		}

		bool maxed;
		if (a_settings_get_boolean(VIK_SETTINGS_WIN_MAX, &maxed)) {
			if (maxed) {
				gtk_window_maximize(GTK_WINDOW(this->vw));
			}
		}

		bool full;
		if (a_settings_get_boolean(VIK_SETTINGS_WIN_FULLSCREEN, &full)) {
			if (full) {
				this->show_full_screen = true;
				gtk_window_fullscreen(GTK_WINDOW(this->vw));
				GtkWidget *check_box = gtk_ui_manager_get_widget(this->uim, "/ui/MainMenu/View/FullScreen");
				if (check_box) {
					gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_box), true);
				}
			}
		}

		int position = -1; // Let GTK determine default positioning
		if (!a_settings_get_integer(VIK_SETTINGS_WIN_PANE_POSITION, &position)) {
			position = -1;
		}
		gtk_paned_set_position(GTK_PANED(this->hpaned), position);
	}

	gtk_window_set_default_size(GTK_WINDOW(this->vw), width, height);

	this->show_side_panel = true;
	this->show_statusbar = true;
	this->show_toolbar = true;
	this->show_main_menu = true;

	// Only accept Drag and Drop of files onto the viewport
	gtk_drag_dest_set(GTK_WIDGET(this->viewport->vvp), GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
	gtk_drag_dest_add_uri_targets(GTK_WIDGET(this->viewport->vvp));
	g_signal_connect(GTK_WIDGET(this->viewport->vvp), "drag-data-received", G_CALLBACK(drag_data_received_cb), NULL);

	// Store the thread value so comparisons can be made to determine the gdk update method
	// Hopefully we are storing the main thread value here :)
	//  [ATM any window initialization is always be performed by the main thread]
	this->thread = g_thread_self();

	// Set the default tool + mode
	gtk_action_activate(gtk_action_group_get_action(this->action_group, "Pan"));
	gtk_action_activate(gtk_action_group_get_action(this->action_group, "ModeMercator"));

	char *accel_file_name = g_build_filename(a_get_viking_dir(), VIKING_ACCELERATOR_KEY_FILE, NULL);
	gtk_accel_map_load(accel_file_name);
	free(accel_file_name);


}





Window::~Window()
{
	a_background_remove_window(this);

	window_list.remove(this);

	gdk_cursor_unref(this->busy_cursor);
	for (int tt = 0; tt < this->vt->n_tools; tt++) {
		if (this->vt->tools[tt].ti.destroy) {
			this->vt->tools[tt].ti.destroy(this->vt->tools[tt].state);
		}
	}
	free(this->vt->tools);
	free(this->vt);

	vik_toolbar_finalize(this->viking_vtb);

	delete this->viewport;
	delete this->layers_panel;

	window_finalize((GObject *) this->vw);
	this->vw = NULL;
}





VikWindow * vik_window_from_layer(Layer * layer)
{
	return (VikWindow *) GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(layer->vt)));
}

Window * window_from_layer(Layer * layer)
{
	return vik_window_from_layer(layer)->window;
}

Window * window_from_widget(void * widget)
{
	return ((VikWindow *) gtk_widget_get_toplevel(GTK_WIDGET(widget)))->window;
}

Window * window_from_vik_window(VikWindow * vw)
{
	return (Window *) vw->window;
}
