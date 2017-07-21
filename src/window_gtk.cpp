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




using namespace SlavGPS;




/**
 * Returns the 'project' filename.
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




// Actual signal handlers
static void destroy_window(GtkWidget * widget, void * data)
{
	if (! --window_count) {
		free(last_folder_files_uri);
		gtk_main_quit();
	}
}




Window * vik_window_new_window(Window * w)
{
	return new Window();
}




Window * Window::new_window()
{
	if (window_count < MAX_WINDOWS) {
		Window * window = new Window();

		QObject::connect(window, SIGNAL("destroy"), NULL, SLOT (destroy_window));
		QObject::connect(window, SIGNAL("newwindow"), NULL, SLOT (vik_window_new_window));
		QObject::connect(window, SIGNAL("openwindow"), NULL, SLOT (open_window));

		gtk_widget_show_all(window);

		if (a_vik_get_restore_window_state()) {
			// These settings are applied after the show all as these options hide widgets
			bool sidepanel;
			if (a_settings_get_boolean(VIK_SETTINGS_WIN_SIDEPANEL, &sidepanel)) {
				if (! sidepanel) {
					window->layers_panel->set_visible(false);
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
				g_signal_emit(G_OBJECT (toolkit_window_from_widget(widget)), window_signals[VW_OPENWINDOW_SIGNAL], 0, filenames);
				/* NB: GSList & contents are freed by main.open_window. */
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




#define VIKING_ACCELERATOR_KEY_FILE "keys.rc"




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
	Layer * layer = this->layers_panel->get_top_layer()->get_top_visible_layer_of_type(LayerType::MAP);
	if (layer) {
		((LayerMap *) layer)->download(this->viewport, only_new);
	}
}




/**
 * Used to handle keys pressed in main UI, e.g. as hotkeys.
 *
 * This is the global key press handler
 *  Global shortcuts are available at any time and hence are not restricted to when a certain tool is enabled
 */
static bool key_press_event_cb(Window * window, QKeyEvent * event, void * data)
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
	if (ev->keyval == GDK_r && (ev->state & modifiers) == GDK_CONTROL_MASK) {
		map_download = true;
		map_download_only_new = true;
	}
	// Full cache reload with Ctrl+F5 or Ctrl+Shift+r [This is not in the menu system]
	// Note the use of uppercase R here since shift key has been pressed
	else if ((ev->keyval == GDK_F5 && (ev->state & modifiers) == GDK_CONTROL_MASK) ||
		 (ev->keyval == GDK_R && (ev->state & modifiers) == (GDK_CONTROL_MASK + GDK_SHIFT_MASK))) {
		map_download = true;
		map_download_only_new = false;
	}
	// Standard Ctrl+KP+ / Ctrl+KP- to zoom in/out respectively
	else if (ev->keyval == GDK_KEY_KP_Add && (ev->state & modifiers) == GDK_CONTROL_MASK) {
		window->viewport->zoom_in();
		window->draw_update();
		return true; // handled keypress
	}
	else if (ev->keyval == GDK_KEY_KP_Subtract && (ev->state & modifiers) == GDK_CONTROL_MASK) {
		window->viewport->zoom_out();
		window->draw_update();
		return true; // handled keypress
	}

	if (map_download) {
		window->simple_map_update(map_download_only_new);
		return true; // handled keypress
	}

	Layer * layer = window->layers_panel->get_selected_layer();
	if (layer && window->tb->active_tool && window->tb->active_tool->key_press) {
		LayerType ltype = window->tb->active_tool->layer_type;
		if (layer && ltype == layer->type) {
			return window->tb->active_tool->key_press(layer, ev, window->tb->active_tool);
		}
	}

	// Ensure called only on window tools (i.e. not on any of the Layer tools since the layer is NULL)
	if (window->current_tool < TOOL_LAYER) {
		// No layer - but enable window tool keypress processing - these should be able to handle a NULL layer
		if (window->tb->active_tool->key_press) {
			return window->tb->active_tool->key_press(layer, ev, window->tb->active_tool);
		}
	}

	/* Restore Main Menu via Escape key if the user has hidden it */
	/* This key is more likely to be used as they may not remember the function key */
	if (ev->keyval == GDK_Escape) {
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




/* Drawing stuff. */
static void newwindow_cb(GtkAction *a, Window * window)
{
	g_signal_emit(window, window_signals[VW_NEWWINDOW_SIGNAL], 0);
}




static void window_configure_event(Window * window)
{
	static int first = 1;
	window->draw_redraw();
	if (first) {
		// This is a hack to set the cursor corresponding to the first tool
		// FIXME find the correct way to initialize both tool and its cursor
		first = 0;
		window->viewport_cursor = (QCursor *) window->tb->get_cursor("Pan");
		/* We set cursor, even if it is NULL: it resets to default */
		gdk_window_set_cursor(gtk_widget_get_window(window->viewport), window->viewport_cursor);
	}
}




/* Mouse event handlers ************************************************************************/

static void draw_click_cb(Window * window, GdkEventButton * event)
{
	gtk_widget_grab_focus(window->viewport);

	/* middle button pressed.  we reserve all middle button and scroll events
	 * for panning and zooming; tools only get left/right/movement
	 */
	if (event->button() == Qt::MiddleButton) {
		if (window->tb->active_tool->pan_handler) {
			// Tool still may need to do something (such as disable something)
			window->tb->click(event);
		}
		window->pan_click(event);
	} else {
		window->tb->click(event);
	}
}




/**
 * Action the single click after a small timeout.
 * If a double click has occurred then this will do nothing.
 */
static bool vik_window_pan_timeout(Window * window)
{
	if (!window->single_click_pending) {
		/* Double click happened, so don't do anything. */
		return false;
	}

	/* Set panning origin. */
	window->pan_move_flag = false;
	window->single_click_pending = false;
	window->viewport->set_center_screen(window->delayed_pan_x, window->delayed_pan_y);
	window->draw_update();

	/* Really turn off the pan moving!! */
	window->pan_x = window->pan_y = -1;
	return false;
}




static void draw_release_cb(Window * window, GdkEventButton * event)
{
	gtk_widget_grab_focus(window->viewport);

	if (event->button() == Qt::MiddleButton) {  /* move / pan */
		if (window->tb->active_tool->pan_handler) {
			// Tool still may need to do something (such as reenable something)
			window->tb->release(event);
		}
		window->pan_release(event);
	} else {
		window->tb->release(event);
	}
}




static void draw_scroll_cb(Window * window, GdkEventScroll * event)
{
	window->draw_scroll(event);
}




static void draw_pan_cb(GtkAction * a, Window * window)
{
	// Since the treeview cell editting intercepts standard keyboard handlers, it means we can receive events here
	// Thus if currently editting, ensure we don't move the viewport when Ctrl+<arrow> is received
	Layer * sel = window->layers_panel->get_selected_layer();
	if (sel && sel->tree_view->get_editing()) {
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




/**
 * center_changed_cb:
 */
static void center_changed_cb(Window * window)
{
	fprintf(stderr, "=========== handling updated center signal\n");

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
 * Refresh maps displayed.
 */
static void draw_refresh_cb(GtkAction * a, Window * window)
{
	// Only get 'new' maps
	window->simple_map_update(true);
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
		Dialog::info(tr("You must select a layer to show its properties."), window);
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




static void help_cache_info_cb(GtkAction * a, Window * window)
{
	// NB: No i18n as this is just for debug
	int byte_size = map_cache_get_size();
	char *msg_sz = NULL;
#if GLIB_CHECK_VERSION(2,30,0)
	msg_sz = g_format_size_full(byte_size, G_FORMAT_SIZE_LONG_FORMAT);
#else
	msg_sz = g_format_size_for_display(byte_size);
#endif
	char * msg = g_strdup_printf("Map Cache size is %s with %d items", msg_sz, map_cache_get_count());
	Dialog::info(msg, window);
	free(msg_sz);
	free(msg);
}




static void menu_delete_layer_cb(GtkAction * a, Window * window)
{
	if (window->layers_panel->get_selected_layer()) {
		window->layers_panel->delete_selected();
		window->modified = true;
	} else {
		Dialog::info(tr("You must select a layer to delete."), window);
	}
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
			g_signal_emit(window, window_signals[VW_OPENWINDOW_SIGNAL], 0, filenames);
			// NB: GSList & contents are freed by main.open_window
		} else {
			window->open_file(path, true);
			free(path);
		}
	}

	free(filename);
}




static bool save_file_as(GtkAction * a, Window * window)
{
	bool rv = false;
	char const * fn;

	GtkWidget * dialog = gtk_file_chooser_dialog_new(_("Save as Viking File."),
							 window,
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

	gtk_window_set_transient_for(GTK_WINDOW(dialog), window);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), true);


	// Auto append / replace extension with '.vik' to the suggested file name as it's going to be a Viking File
	char * auto_save_name = g_strdup(window->get_filename());
	if (!a_file_check_ext(auto_save_name, ".vik")) {
		auto_save_name = g_strconcat(auto_save_name, ".vik", NULL);
	}

	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), auto_save_name);

	while (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		if (0 != access(fn, F_OK) || Dialog::yes_or_no(tr("The file \"%1\" exists, do you wish to overwrite it?").arg(file_base_name(fn)), GTK_WINDOW(dialog))) {
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
		Dialog::error(tr("The filename you requested could not be opened for writing."), this->get_window());
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
			if (0 == access(fn, F_OK)) {
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
				this->status_bar->set_message(StatusBarField::INFO, QString("Exporting to file: %1").arg(fn));
				while (gtk_events_pending()) {
					gtk_main_iteration();
				}
			}

			success = success && this_success;
		}

		free(fn);
	}

	this->clear_busy_cursor();

	/* Confirm what happened. */
	this->status_bar->set_message(StatusBarField::INFO, QString("Exported files: %1").arg(export_count));

	return success;
}




void Window::export_to_common(VikFileType_t vft, char const * extension)
{
	std::list<Layer const *> * layers = this->layers_panel->get_all_layers_of_type(LayerType::TRW, true);

	if (!layers || layers->empty()) {
		Dialog::info(tr("Nothing to Export!"), this->get_window());
		/* kamilFIXME: delete layers? */
		return;
	}

	GtkWidget *dialog = gtk_file_chooser_dialog_new(tr("Export to directory"),
							this,
							GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
							GTK_STOCK_CANCEL,
							GTK_RESPONSE_REJECT,
							GTK_STOCK_OK,
							GTK_RESPONSE_ACCEPT,
							NULL);
	gtk_window_set_transient_for(GTK_WINDOW(dialog), this);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), true);
	gtk_window_set_modal(GTK_WINDOW(dialog), true);

	gtk_widget_show_all(dialog);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		char *dir = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		gtk_widget_destroy(dialog);
		if (dir) {
			if (!this->export_to(layers, vft, dir, extension)) {
				Dialog::error(tr("Could not convert all files"), this->get_window());
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
		if (0 == access(window->filename, F_OK)) {
			// Get some timestamp information of the file
			struct stat stat_buf;
			if (stat(window->filename, &stat_buf) == 0) {
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

	/* Show the info. */
	Dialog::info(message, window);
	free(message);
}




static void map_cache_flush_cb(GtkAction * a, Window * window)
{
	map_cache_flush();
}




static void menu_copy_centre_cb(GtkAction * a, Window * window)
{
	QString first;
	QString second;

	const Coord coord = window->viewport->get_center();

	bool full_format = false;
	(void) a_settings_get_boolean(VIK_SETTINGS_WIN_COPY_CENTRE_FULL_FORMAT, &full_format);

	if (full_format) {
		/* Bells & Whistles - may include degrees, minutes and second symbols. */
		coord->to_strings(first, second);
	} else {
		/* Simple x.xx y.yy format. */
		struct LatLon ll;
		a_coords_utm_to_latlon(&ll, &utm);
		first = g_strdup_printf("%.6f", ll.lat);
		second = g_strdup_printf("%.6f", ll.lon);
	}

	const QString message = QString("%1 %2").arg(firsts).arg(second);

	a_clipboard_copy(VIK_CLIPBOARD_DATA_TEXT, LayerType::AGGREGATE, SublayerType::NONE, 0, message, NULL);
}




static void preferences_change_update(Window * window)
{
	// Want to update all TrackWaypoint layers
	std::list<Layer const *> * layers = window->layers_panel->get_all_layers_of_type(LayerType::TRW, true);
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



static void default_location_cb(GtkAction * a, Window * window)
{
	/* Simplistic repeat of preference setting
	   Only the name & type are important for setting the preference via this 'external' way */
	Parameter pref_lat[] = {
		{ LayerType::NUM_TYPES,
		  VIKING_PREFERENCES_NAMESPACE "default_latitude",
		  SGVariantType::DOUBLE,
		  VIK_LAYER_GROUP_NONE,
		  NULL,
		  WidgetType::SPINBUTTON,
		  NULL,
		  NULL,
		  NULL,
		  NULL,
		  NULL,
		  NULL,
		},
	};
	Parameter pref_lon[] = {
		{ LayerType::NUM_TYPES,
		  VIKING_PREFERENCES_NAMESPACE "default_longitude",
		  SGVariantType::DOUBLE,
		  VIK_LAYER_GROUP_NONE,
		  NULL,
		  WidgetType::SPINBUTTON,
		  NULL,
		  NULL,
		  NULL,
		  NULL,
		  NULL,
		  NULL,
		},
	};

	/* Get current center */
	struct LatLon ll = window->viewport->get_center()->get_latlon();

	/* Apply to preferences */
	SGVariant vlp_data;
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
		if (Dialog::yes_or_no(tr("Are you sure you wish to delete all layers?"), window)) {
			window->layers_panel->clear();
			window->set_filename(NULL);
			window->draw_update();
		}
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




static void import_kmz_file_cb(GtkAction * a, Window * window)
{
	GtkWidget * dialog = gtk_file_chooser_dialog_new(_("Open File"),
							 window,
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
			Dialog::error(tr("Unable to import %1.").arg(QString(fn)), window);
		}

		window->draw_update();
	}
	gtk_widget_destroy(dialog);
}





static void set_bg_color(GtkAction * a, Window * window)
{
	GtkWidget * colorsd = gtk_color_selection_dialog_new(_("Choose a background color"));
	QColor * color = window->viewport->get_background_qcolor();
	gtk_color_selection_set_previous_color(GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), color);
	gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), color);
	if (gtk_dialog_run(GTK_DIALOG(colorsd)) == GTK_RESPONSE_OK) {
		gtk_color_selection_get_current_color(GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), color);
		window->viewport->set_background_qcolor(color);
		window->draw_update();
	}
	free(color);
	gtk_widget_destroy(colorsd);
}




static void set_highlight_color(GtkAction * a, Window * window)
{
	GtkWidget * colorsd = gtk_color_selection_dialog_new(_("Choose a track highlight color"));
	const QColor color = window->viewport->get_highlight_color();
	gtk_color_selection_set_previous_color(GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), color);
	gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), color);
	if (gtk_dialog_run(GTK_DIALOG(colorsd)) == GTK_RESPONSE_OK) {
		gtk_color_selection_get_current_color(GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(colorsd))), color);
		window->viewport->set_highlight_qcolor(color);
		window->draw_update();
	}
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
	{ "Export",              GTK_STOCK_CONVERT,        N_("_Export All"),                        NULL,               N_("Export All TrackWaypoint Layers"),                    (GCallback) NULL                      },
	{ "ExportGPX",           NULL,                     N_("_GPX..."),           	             NULL,               N_("Export as GPX"),                                      (GCallback) export_to_gpx_cb          },
	{ "Acquire",             GTK_STOCK_GO_DOWN,        N_("A_cquire"),                           NULL,               NULL,                                                     (GCallback) NULL                      },
	{ "AcquireGPS",          NULL,                     N_("From _GPS..."),                       NULL,               N_("Transfer data from a GPS device"),                    (GCallback) acquire_from_gps          },
	{ "AcquireGPSBabel",     NULL,                     N_("Import File With GPS_Babel..."),      NULL,               N_("Import file via GPSBabel converter"),                 (GCallback) acquire_from_file         },
	{ "AcquireRouting",      NULL,                     N_("_Directions..."),                     NULL,               N_("Get driving directions"),                             (GCallback) acquire_from_routing      },

#ifdef VIK_CONFIG_GEOCACHES
	{ "AcquireGC",           NULL,                     N_("Geo_caches..."),    	             NULL,               N_("Get Geocaches from geocaching.com"),                  (GCallback) acquire_from_gc           },
#endif
#ifdef VIK_CONFIG_GEOTAG
	{ "AcquireGeotag",       NULL,                     N_("From Geotagged _Images..."),          NULL,               N_("Create waypoints from geotagged images"),             (GCallback) acquire_from_geotag       },
#endif
	{ "AcquireURL",          NULL,                     N_("From _URL..."),                       NULL,               N_("Get a file from a URL"),                              (GCallback) acquire_from_url          },
	{ "Save",                GTK_STOCK_SAVE,           N_("_Save"),                              "<control>S",       N_("Save the file"),                                      (GCallback) save_file                 },
	{ "SaveAs",              GTK_STOCK_SAVE_AS,        N_("Save _As..."),                        NULL,               N_("Save the file under different name"),                 (GCallback) save_file_as              },
	{ "FileProperties",      NULL,                     N_("Properties..."),                      NULL,               N_("File Properties"),                                    (GCallback) file_properties_cb        },
#ifdef HAVE_ZIP_H
	{ "ImportKMZ",           GTK_STOCK_CONVERT,        N_("Import KMZ _Map File..."),            NULL,               N_("Import a KMZ file"), (GCallback)import_kmz_file_cb },
#endif


	{ "Exit",                GTK_STOCK_QUIT,           N_("E_xit"),                              "<control>W",       N_("Exit the program"),                                   (GCallback) window_close              },
	{ "SaveExit",            GTK_STOCK_QUIT,           N_("Save and Exit"),                      NULL,               N_("Save and Exit the program"),                          (GCallback) save_file_and_exit        },

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
	{ "MapCacheFlush",       NULL,                     N_("_Flush Map Cache"),                   NULL,               NULL,                                                     (GCallback) map_cache_flush_cb         },
	{ "SetDefaultLocation",  GTK_STOCK_GO_FORWARD,     N_("_Set the Default Location"),          NULL,               N_("Set the Default Location to the current position"),   (GCallback) default_location_cb       },
	{ "Preferences",         GTK_STOCK_PREFERENCES,    N_("_Preferences"),                       NULL,               N_("Program Preferences"),                                (GCallback) preferences_cb            },
	{ "LayerDefaults",       GTK_STOCK_PROPERTIES,     N_("_Layer Defaults"),                    NULL,               NULL,                                                     NULL                                  },
	{ "Properties",          GTK_STOCK_PROPERTIES,     N_("_Properties"),                        NULL,               N_("Layer Properties"),                                   (GCallback) menu_properties_cb        },
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




static void register_vik_icons(GtkIconFactory *icon_factory)
{
	GtkIconSet *icon_set;

	for (int i = 0; i < n_stock_icons; i++) {
		icon_set = gtk_icon_set_new_from_pixbuf(gdk_pixbuf_from_pixdata(stock_icons[i].data, false, NULL));
		gtk_icon_factory_add(icon_factory, stock_icons[i].stock_id, icon_set);
		gtk_icon_set_unref(icon_set);
	}
}




Window::~Window()
{
	a_background_remove_window(this);

	window_list.remove(this);

	gdk_cursor_unref(this->busy_cursor);

	delete this->tb;

	vik_toolbar_finalize(this->viking_vtb);

	delete this->viewport;
	delete this->layers_panel;
}
