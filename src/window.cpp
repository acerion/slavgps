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


#include <cassert>

#include <QtWidgets>

#include "window.h"
#include "viewport.h"
#include "layer.h"
#include "layers_panel.h"
#include "globals.h"
#include "uibuilder_qt.h"
#include "settings.h"
#include "background.h"




using namespace SlavGPS;




enum {
	TOOL_PAN = 0,
	TOOL_ZOOM,
	TOOL_RULER,
	TOOL_SELECT,
	TOOL_LAYER,
	NUMBER_OF_TOOLS
};




Window::Window()
{

	strcpy(this->type_string, "SG QT WINDOW");

	QIcon::setThemeName("Tango");
	this->create_layout();
	this->create_actions();


	this->tb = new LayerToolsBox(this);

	this->create_ui();


	// Own signals
	connect(this->viewport, SIGNAL(updated_center(void)), this, SLOT(center_changed_cb(void)));



#if 0
	this->init_toolkit_widget();

	this->viewport = new Viewport();
	this->layers_panel = new LayersPanel();
	this->layers_panel->set_viewport(this->viewport);
	this->viking_vs = vik_statusbar_new();

	this->viking_vtb = vik_toolbar_new();

	this->tb = new LayerToolsBox(this);

	window_create_ui(this);
	this->set_filename(NULL);

	this->busy_cursor = gdk_cursor_new(GDK_WATCH);


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
	gtk_container_add(GTK_CONTAINER (this->gtk_window_), this->main_vbox);
	this->menu_hbox = gtk_hbox_new(false, 1);
	GtkWidget *menu_bar = gtk_ui_manager_get_widget(this->uim, "/MainMenu");
	gtk_box_pack_start(GTK_BOX(this->menu_hbox), menu_bar, false, true, 0);
	gtk_box_pack_start(GTK_BOX(this->main_vbox), this->menu_hbox, false, true, 0);

	toolbar_init(this->viking_vtb,
		     this->gtk_window_,
		     this->main_vbox,
		     this->menu_hbox,
		     toolbar_tool_cb,
		     toolbar_reload_cb,
		     (void *) this); // This auto packs toolbar into the vbox
	// Must be performed post toolbar init
	for (LayerType i = LayerType::AGGREGATE; i < LayerType::NUM_TYPES; ++i) {
		for (int j = 0; j < Layer::get_interface(i)->tools_count; j++) {
			toolbar_action_set_sensitive(this->viking_vtb, Layer::get_interface(i)->layer_tools[j]->radioActionEntry.name, false);
		}
	}

	vik_ext_tool_datasources_add_menu_items(this, this->uim);

	GtkWidget * zoom_levels = gtk_ui_manager_get_widget(this->uim, "/MainMenu/View/SetZoom");
	GtkWidget * zoom_levels_menu = create_zoom_menu_all_levels(this->viewport->get_zoom());
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(zoom_levels), zoom_levels_menu);
	g_signal_connect(G_OBJECT(zoom_levels_menu), "selection-done", G_CALLBACK(zoom_changed_cb), this);
	g_signal_connect_swapped(G_OBJECT(this->viking_vs), "clicked", G_CALLBACK(zoom_popup_handler), zoom_levels_menu);

	g_signal_connect(this->get_toolkit_object(), "delete_event", G_CALLBACK (delete_event), NULL);


	// Signals from GTK
	g_signal_connect_swapped(G_OBJECT(this->viewport->get_toolkit_object()), "expose_event", G_CALLBACK(draw_sync_cb), this);
	g_signal_connect_swapped(G_OBJECT(this->viewport->get_toolkit_object()), "configure_event", G_CALLBACK(window_configure_event), this);
	gtk_widget_add_events(this->viewport->get_toolkit_widget(), GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK);
	g_signal_connect_swapped(G_OBJECT(this->viewport->get_toolkit_object()), "scroll_event", G_CALLBACK(draw_scroll_cb), this);
	g_signal_connect_swapped(G_OBJECT(this->viewport->get_toolkit_object()), "button_press_event", G_CALLBACK(draw_click_cb), this);
	g_signal_connect_swapped(G_OBJECT(this->viewport->get_toolkit_object()), "button_release_event", G_CALLBACK(draw_release_cb), this);
	g_signal_connect_swapped(G_OBJECT(this->viewport->get_toolkit_object()), "motion_notify_event", G_CALLBACK(draw_mouse_motion_cb), this);

	g_signal_connect_swapped(G_OBJECT(this->layers_panel->get_toolkit_widget()), "update", G_CALLBACK(draw_update_cb), this);
	g_signal_connect_swapped(G_OBJECT(this->layers_panel->get_toolkit_widget()), "delete_layer", G_CALLBACK(vik_window_clear_highlight_cb), this);

	// Allow key presses to be processed anywhere
	g_signal_connect_swapped(this->get_toolkit_object(), "key_press_event", G_CALLBACK (key_press_event_cb), this);

	// Set initial button sensitivity
	center_changed_cb(this);

	this->hpaned = gtk_hpaned_new();
	gtk_paned_pack1(GTK_PANED(this->hpaned), this->layers_panel->get_toolkit_widget(), false, true);
	gtk_paned_pack2(GTK_PANED(this->hpaned), this->viewport->get_toolkit_widget(), true, true);

	/* This packs the button into the window (a gtk container). */
	gtk_box_pack_start(GTK_BOX(this->main_vbox), this->hpaned, true, true, 0);

	gtk_box_pack_end(GTK_BOX(this->main_vbox), GTK_WIDGET(this->viking_vs), false, true, 0);

	a_background_add_window(this);

	window_list.push_front(this);

	int height = VIKING_WINDOW_HEIGHT;
	int width = VIKING_WINDOW_WIDTH;

	if (Preferences::get_restore_window_state()) {
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
				gtk_window_maximize(this->get_toolkit_window());
			}
		}

		bool full;
		if (a_settings_get_boolean(VIK_SETTINGS_WIN_FULLSCREEN, &full)) {
			if (full) {
				this->show_full_screen = true;
				gtk_window_fullscreen(this->get_toolkit_window());
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

	gtk_window_set_default_size(this->get_toolkit_window(), width, height);

	// Only accept Drag and Drop of files onto the viewport
	gtk_drag_dest_set(this->viewport->get_toolkit_widget(), GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
	gtk_drag_dest_add_uri_targets(this->viewport->get_toolkit_widget());
	g_signal_connect(this->viewport->get_toolkit_widget(), "drag-data-received", G_CALLBACK(drag_data_received_cb), NULL);

	// Store the thread value so comparisons can be made to determine the gdk update method
	// Hopefully we are storing the main thread value here :)
	//  [ATM any window initialization is always be performed by the main thread]
	this->thread = g_thread_self();

	// Set the default tool + mode
	gtk_action_activate(gtk_action_group_get_action(this->action_group, "Pan"));
	gtk_action_activate(gtk_action_group_get_action(this->action_group, "ModeMercator"));

	char *accel_file_name = g_build_filename(get_viking_dir(), VIKING_ACCELERATOR_KEY_FILE, NULL);
	gtk_accel_map_load(accel_file_name);
	free(accel_file_name);


#endif
}




void Window::create_layout()
{
	this->toolbar = new QToolBar("Main Toolbar");
	this->addToolBar(this->toolbar);


	this->viewport = new SlavGPS::Viewport(this);
	this->viewport->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	//layout->addWidget(viewport);
	struct LatLon ll = { 54.0, 14.0 };
	this->viewport->set_center_latlon(&ll, false);
	this->viewport->xmpp = 0.01;
	this->viewport->ympp = 0.01;
	//viewport->show();
	qDebug() << "II: Window: created Viewport with size:" << this->viewport->height() << this->viewport->width();


	this->setCentralWidget(viewport);


	this->layers_panel = new SlavGPS::LayersPanel(this);

	this->panel_dock = new QDockWidget(this);
	this->panel_dock->setWidget(this->layers_panel);
	this->panel_dock->setWindowTitle("Layers");
	this->addDockWidget(Qt::LeftDockWidgetArea, this->panel_dock);

	setStyleSheet("QMainWindow::separator { image: url(src/icons/handle_indicator.png); width: 8}");




	return;
}




void Window::create_actions(void)
{
	this->menu_file = new QMenu("&File");
	this->menu_edit = new QMenu("&Edit");
	this->menu_view = new QMenu("&View");
	this->menu_layers = new QMenu("&Layers");
	this->menu_tools = new QMenu("&Tools");
	this->menu_help = new QMenu("&Help");

	this->menu_bar = new QMenuBar();
	this->menu_bar->addMenu(this->menu_file);
	this->menu_bar->addMenu(this->menu_edit);
	this->menu_bar->addMenu(this->menu_view);
	this->menu_bar->addMenu(this->menu_layers);
	this->menu_bar->addMenu(this->menu_tools);
	this->menu_bar->addMenu(this->menu_help);
	setMenuBar(this->menu_bar);


	/* "File" menu. */
	QAction * qa_file_new = NULL;
	QAction * qa_file_exit = NULL;
	{
		qa_file_new = new QAction("&New file...", this);
		qa_file_new->setShortcut(Qt::CTRL + Qt::Key_N);
		qa_file_new->setIcon(QIcon::fromTheme("document-new"));

		qa_file_exit = new QAction("E&xit", this);
		qa_file_exit->setShortcut(Qt::CTRL + Qt::Key_X);
		qa_file_exit->setIcon(QIcon::fromTheme("application-exit"));
		connect(qa_file_exit, SIGNAL (triggered(bool)), this, SLOT (close(void)));

		this->menu_file->addAction(qa_file_new);
		this->menu_file->addAction(qa_file_exit);
	}


	/* "Edit" menu. */
	{
		QAction * qa_edit_preferences = new QAction("&Preferences", this);
		qa_edit_preferences->setIcon(QIcon::fromTheme("preferences-other"));
		connect(qa_edit_preferences, SIGNAL (triggered(bool)), this, SLOT (preferences_cb(void)));

		this->menu_edit->addAction(qa_edit_preferences);
	}


	/* "View" menu. */
	QAction * qa_view_full_screen = NULL;

	QAction * qa_view_show_draw_scale = NULL;
	QAction * qa_view_show_draw_centermark = NULL;
	QAction * qa_view_show_draw_highlight = NULL;
	QAction * qa_view_show_side_panel = NULL;
	QAction * qa_view_show_statusbar = NULL;
	QAction * qa_view_show_toolbar = NULL;
	QAction * qa_view_show_main_menu = NULL;

	QAction * qa_view_zoom_in = NULL;
	QAction * qa_view_zoom_out = NULL;
	QAction * qa_view_zoom_to = NULL;
	{
		qa_view_full_screen = new QAction("&Full Screen", this);
		qa_view_full_screen->setShortcut(Qt::Key_F11);
		qa_view_full_screen->setCheckable(true);
		qa_view_full_screen->setChecked(this->view_full_screen);
		connect(qa_view_full_screen, SIGNAL(triggered(bool)), this, SLOT(view_full_screen_cb(bool)));

		this->menu_view->addAction(qa_view_full_screen);


		QMenu * show_submenu = new QMenu("&Show", this);
		this->menu_view->addMenu(show_submenu);

		qa_view_show_draw_scale = new QAction("Show &Scale", this);
		qa_view_show_draw_scale->setShortcut(Qt::SHIFT + Qt::Key_F5);
		qa_view_show_draw_scale->setCheckable(true);
		qa_view_show_draw_scale->setChecked(this->draw_scale);
		connect(qa_view_show_draw_scale, SIGNAL(triggered(bool)), this, SLOT(draw_scale_cb(bool)));

		qa_view_show_draw_centermark = new QAction("Show &Center Mark", this);
		qa_view_show_draw_centermark->setShortcut(Qt::Key_F6);
		qa_view_show_draw_centermark->setCheckable(true);
		qa_view_show_draw_centermark->setChecked(this->draw_centermark);
		connect(qa_view_show_draw_centermark, SIGNAL(triggered(bool)), this, SLOT(draw_centermark_cb(bool)));

		qa_view_show_draw_highlight = new QAction("Show &Highlight", this);
		qa_view_show_draw_highlight->setShortcut(Qt::Key_F7);
		qa_view_show_draw_highlight->setCheckable(true);
		qa_view_show_draw_highlight->setChecked(this->draw_highlight);
		connect(qa_view_show_draw_highlight, SIGNAL(triggered(bool)), this, SLOT(draw_highlight_cb(bool)));

		qa_view_show_side_panel = this->panel_dock->toggleViewAction(); /* Existing action! */
		qa_view_show_side_panel->setText("Show Side &Panel");
		qa_view_show_side_panel->setShortcut(Qt::Key_F9);
		qa_view_show_side_panel->setCheckable(true);
		qa_view_show_side_panel->setChecked(this->view_side_panel);
		connect(qa_view_show_side_panel, SIGNAL(triggered(bool)), this, SLOT(view_side_panel_cb(bool)));

		qa_view_show_statusbar = new QAction("Show Status&bar", this);
		qa_view_show_statusbar->setShortcut(Qt::Key_F12);
		qa_view_show_statusbar->setCheckable(true);
		qa_view_show_statusbar->setChecked(this->view_statusbar);
		connect(qa_view_show_statusbar, SIGNAL(triggered(bool)), this, SLOT(view_statusbar_cb(bool)));

		qa_view_show_toolbar = this->toolbar->toggleViewAction(); /* Existing action! */
		qa_view_show_toolbar->setText("Show &Toolbar");
		qa_view_show_toolbar->setShortcut(Qt::Key_F3);
		qa_view_show_toolbar->setCheckable(true);
		qa_view_show_toolbar->setChecked(this->view_toolbar);
		/* No signal connection needed, we have toggleViewAction(). */

		qa_view_show_main_menu = new QAction("Show &Menu", this);
		qa_view_show_main_menu->setShortcut(Qt::Key_F4);
		qa_view_show_main_menu->setCheckable(true);
		qa_view_show_main_menu->setChecked(this->view_main_menu);
		connect(qa_view_show_main_menu, SIGNAL(triggered(bool)), this, SLOT(view_main_menu_cb(bool)));


		show_submenu->addAction(qa_view_show_draw_scale);
		show_submenu->addAction(qa_view_show_draw_centermark);
		show_submenu->addAction(qa_view_show_draw_highlight);
		show_submenu->addAction(qa_view_show_side_panel);
		show_submenu->addAction(qa_view_show_statusbar);
		show_submenu->addAction(qa_view_show_toolbar);
		show_submenu->addAction(qa_view_show_main_menu);


		this->menu_view->addSeparator();


		qa_view_zoom_in = new QAction("Zoom &In", this);
		qa_view_zoom_in->setShortcut(Qt::CTRL + Qt::Key_Plus);
		qa_view_zoom_in->setIcon(QIcon::fromTheme("zoom-in"));
		connect(qa_view_zoom_in, SIGNAL(triggered(bool)), this, SLOT(zoom_cb(void)));

		qa_view_zoom_out = new QAction("Zoom &Out", this);
		qa_view_zoom_out->setShortcut(Qt::CTRL + Qt::Key_Minus);
		qa_view_zoom_out->setIcon(QIcon::fromTheme("zoom-out"));
		connect(qa_view_zoom_out, SIGNAL(triggered(bool)), this, SLOT(zoom_cb(void)));

		qa_view_zoom_to = new QAction("Zoom &To...", this);
		qa_view_zoom_to->setShortcut(Qt::CTRL + Qt::Key_Z);
		qa_view_zoom_to->setIcon(QIcon::fromTheme("zoom-fit-best"));
		connect(qa_view_zoom_to, SIGNAL(triggered(bool)), this, SLOT(zoom_to_cb(void)));


		this->menu_view->addAction(qa_view_zoom_in);
		this->menu_view->addAction(qa_view_zoom_out);
		this->menu_view->addAction(qa_view_zoom_to);


		this->menu_view->addSeparator();


		QAction * qa = new QAction("Background &Jobs", this);
		qa->setIcon(QIcon::fromTheme("emblem-system"));
		connect(qa, SIGNAL(triggered(bool)), this, SLOT(show_background_jobs_window_cb(void)));


		this->menu_view->addAction(qa);
	}



	/* "Layers" menu. */
	{
		this->qa_layer_properties = new QAction("Properties...", this);
		this->menu_layers->addAction(this->qa_layer_properties);
		connect (this->qa_layer_properties, SIGNAL (triggered(bool)), this->layers_panel, SLOT (properties(void)));

		for (SlavGPS::LayerType i = SlavGPS::LayerType::AGGREGATE; i < SlavGPS::LayerType::NUM_TYPES; ++i) {

			QVariant variant((int) i);
			QAction * qa = new QAction("new layer", this);
			qa->setData(variant);
			qa->setIcon(*Layer::get_interface(i)->icon);
			connect(qa, SIGNAL(triggered(bool)), this, SLOT(menu_layer_new_cb(void)));

			this->menu_layers->addAction(qa);
		}
	}


	/* "Help" menu. */
	{
		QAction * qa_help_help = new QAction("&Help", this);
		qa_help_help->setIcon(QIcon::fromTheme("help-contents"));
		qa_help_help->setShortcut(Qt::Key_F1);

		QAction * qa_help_about = new QAction("&About", this);
		qa_help_about->setIcon(QIcon::fromTheme("help-about"));

		this->menu_help->addAction(qa_help_help);
		this->menu_help->addAction(qa_help_about);
	}




	this->toolbar->addAction(qa_file_new);



	this->status_bar = new QStatusBar();
	setStatusBar(this->status_bar);

	return;
}




void Window::draw_update()
{
	qDebug() << "II: Window: redraw + sync begin" << __FUNCTION__ << __LINE__;
	this->draw_redraw();
	//this->draw_sync();
	qDebug() << "II: Window: redraw + sync end" << __FUNCTION__ << __LINE__;
}




static void draw_sync_cb(Window * window)
{
	window->draw_sync();
}




void Window::draw_sync()
{
	qDebug() << "II: Window: sync begin" << __FUNCTION__ << __LINE__;
	this->viewport->sync();
	qDebug() << "II: Window: sync end" << __FUNCTION__ << __LINE__;
	this->draw_status();
}




void Window::draw_status()
{
	static char zoom_level[22];
	double xmpp = this->viewport->get_xmpp();
	double ympp = this->viewport->get_ympp();
	char *unit = this->viewport->get_coord_mode() == VIK_COORD_UTM ? (char *) "mpp" : (char *) "pixelfact";
	if (xmpp != ympp) {
		snprintf(zoom_level, 22, "%.3f/%.3f %s", xmpp, ympp, unit);
	} else {
		if ((int)xmpp - xmpp < 0.0) {
			snprintf(zoom_level, 22, "%.3f %s", xmpp, unit);
		} else {
			/* xmpp should be a whole number so don't show useless .000 bit. */
			snprintf(zoom_level, 22, "%d %s", (int)xmpp, unit);
		}
	}
#if 0
	vik_statusbar_set_message(this->viking_vs, VIK_STATUSBAR_ZOOM, zoom_level);

	draw_status_tool(this);
#endif
}




void Window::menu_layer_new_cb(void) /* Slot. */
{
	QAction * qa = (QAction *) QObject::sender();
	SlavGPS::LayerType layer_type = (SlavGPS::LayerType) qa->data().toInt();

	qDebug() << "Window: clicked \"layer new\" for layer type" << (int) layer_type << Layer::get_interface(layer_type)->fixed_layer_name;

	if (this->layers_panel->new_layer(layer_type)) {
		qDebug() << "II: Window: new layer, call draw_update()" << __FUNCTION__ << __LINE__;
		this->draw_update();
		this->modified = true;
	}

}




void Window::draw_redraw()
{
	VikCoord old_center = this->trigger_center;
	this->trigger_center = *(this->viewport->get_center());
	SlavGPS::Layer * new_trigger = this->trigger;
	this->trigger = NULL;
	SlavGPS::Layer * old_trigger = this->viewport->get_trigger();

	if (!new_trigger) {
		; /* Do nothing -- have to redraw everything. */
	} else if ((old_trigger != new_trigger) || !vik_coord_equals(&old_center, &this->trigger_center) || (new_trigger->type == SlavGPS::LayerType::AGGREGATE)) {
		this->viewport->set_trigger(new_trigger); /* todo: set to half_drawn mode if new trigger is above old */
	} else {
		this->viewport->set_half_drawn(true);
	}

	/* Actually draw. */
	this->viewport->clear();
	/* Main layer drawing. */
	this->layers_panel->draw_all();
	/* Draw highlight (possibly again but ensures it is on top - especially for when tracks overlap). */
#if 0
	if (this->viewport->get_draw_highlight()) {
		if (this->containing_trw && (this->selected_tracks || this->selected_waypoints)) {
			this->containing_trw->draw_highlight_items(this->selected_tracks, this->selected_waypoints, this->viewport);

		} else if (this->containing_trw && (this->selected_track || this->selected_waypoint)) {
			this->containing_trw->draw_highlight_item((Track *) this->selected_track, this->selected_waypoint, this->viewport);

		} else if (this->selected_trw) {
			this->selected_trw->draw_highlight(this->viewport);
		}
	}
#endif
	/* Other viewport decoration items on top if they are enabled/in use. */
	this->viewport->draw_scale();
	this->viewport->draw_copyright();
	this->viewport->draw_centermark();
	this->viewport->draw_logo();

	this->viewport->set_half_drawn(false); /* Just in case. */
}




void Window::draw_layer_cb(sg_uid_t uid) /* Slot. */
{
	qDebug() << "SLOT: Window: draw_layer" << (qulonglong) uid;
	/* TODO: draw only one layer, not all of them. */
	this->draw_redraw();
}




void Window::selected_layer(Layer * layer)
{
	QString layer_type(QString(layer->get_interface(layer->type)->fixed_layer_name));
	qDebug() << "Window: selected layer type" << layer_type;

	bool window_tool_still_active = false;
	QAction * qa = this->tb->set_other_groups_disabled(layer_type);
	if (qa) {
		if (qa->actionGroup()->objectName() == "window") {
			window_tool_still_active = true;
		} else {
			/* qa is still-active button in non-window, not-this-layer group. Deactivate. */
			qa->setChecked(false);
		}
	}


	qa = this->tb->set_group_enabled(layer_type);
	if (qa) {
		if (window_tool_still_active) {
			/* There is already one button active (in 'window' tools group).
			   No need to have more than one button checked. */
		} else {
			/* We have switched from one layer-specific group to another.
			   No button in "window" group is active. Some button needs to be active, though. */
			qa->setChecked(true);
		}
	} else {
		if (layer->type != LayerType::AGGREGATE) {
			qDebug() << "ERROR: WINDOW: can't find any action in newly selected layer group" << layer_type;
		}
	}

#if 0
	if (!this->action_group) {
		return;
	}

	for (LayerType type = LayerType::AGGREGATE; type < LayerType::NUM_TYPES; ++type) {
		VikLayerInterface * layer_interface = Layer::get_interface(type);
		int tool_count = layer_interface->tools_count;

		for (int tool = 0; tool < tool_count; tool++) {
			GtkAction * action = gtk_action_group_get_action(this->action_group,
									 layer_interface->layer_tools[tool]->radioActionEntry.name);
			g_object_set(action, "sensitive", type == layer->type, NULL);
			toolbar_action_set_sensitive(this->viking_vtb, Layer::get_interface(type)->layer_tools[tool]->radioActionEntry.name, type == layer->type);
		}
	}
#endif
}




Viewport * Window::get_viewport()
{
	return this->viewport;
}



LayersPanel * Window::get_layers_panel()
{
	return this->layers_panel;
}


LayerToolsBox * Window::get_layer_tools_box(void)
{
	return this->tb;
}




/**
 * center_changed_cb:
 */
void Window::center_changed_cb(void) /* Slot. */
{
	fprintf(stderr, "---- handling updated_center signal (%s:%d)\n", __FUNCTION__, __LINE__);
#if 0
	// ATM Keep back always available, so when we pan - we can jump to the last requested position
	/*
	  GtkAction* action_back = gtk_action_group_get_action(window->action_group, "GoBack");
	  if (action_back) {
	  gtk_action_set_sensitive(action_back, vik_viewport_back_available(window->viewport));
	  }
	*/
	GtkAction* action_forward = gtk_action_group_get_action(this->action_group, "GoForward");
	if (action_forward) {
		gtk_action_set_sensitive(action_forward, this->viewport->forward_available());
	}

	toolbar_action_set_sensitive(this->viking_vtb, "GoForward", this->viewport->forward_available());
#endif
}




QMenu * Window::get_layer_menu(QMenu * menu)
{
	menu->addAction(this->qa_layer_properties);
	return menu;
}






void Window::create_ui(void)
{
#if 0
	GtkUIManager * uim = gtk_ui_manager_new();
	window->uim = uim;
#endif

	{
		QActionGroup * group = new QActionGroup(this);
		group->setObjectName("window");
		QAction * qa = NULL;

		this->toolbar->addSeparator();


		qa = this->tb->add_tool(selecttool_create(this, this->viewport));
		group->addAction(qa);
		qa->setChecked(true);
		this->tb->activate_tool(qa);

		qa = this->tb->add_tool(ruler_create(this, this->viewport));
		group->addAction(qa);

		qa = this->tb->add_tool(zoomtool_create(this, this->viewport));
		group->addAction(qa);

		qa = this->tb->add_tool(pantool_create(this, this->viewport));
		group->addAction(qa);


		this->toolbar->addActions(group->actions());
		this->menu_tools->addActions(group->actions());
		this->tb->add_group(group);

		connect(group, SIGNAL(triggered(QAction *)), this, SLOT(layer_tools_cb(QAction *)));
	}


	{
		for (LayerType i = LayerType::AGGREGATE; i < LayerType::NUM_TYPES; ++i) {

			if (!Layer::get_interface(i)->tools_count) {
				continue;
			}
			this->toolbar->addSeparator();
			this->menu_tools->addSeparator();

			QActionGroup * group = new QActionGroup(this);
			QString name(Layer::get_interface(i)->name);
			group->setObjectName(name);

			unsigned int j = 0;
			for (j = 0; j < Layer::get_interface(i)->tools_count; j++) {

				LayerTool * layer_tool = Layer::get_interface(i)->layer_tool_constructors[j](this, this->viewport);
				QAction * qa = this->tb->add_tool(layer_tool);
				group->addAction(qa);

				assert (layer_tool->layer_type == i);
			}
			this->toolbar->addActions(group->actions());
			this->menu_tools->addActions(group->actions());
			this->tb->add_group(group);
			this->tb->set_group_disabled(name);

			connect(group, SIGNAL(triggered(QAction *)), this, SLOT(layer_tools_cb(QAction *)));
		}
	}

#if 0
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

	/* GeoJSON import capability. */
	if (g_find_program_in_path(geojson_program_import())) {
		if (gtk_ui_manager_add_ui_from_string(uim,
						      "<ui><menubar name='MainMenu'><menu action='File'><menu action='Acquire'><menuitem action='AcquireGeoJSON'/></menu></menu></menubar></ui>",
						      -1, &error)) {
			gtk_action_group_add_actions(action_group, entries_geojson, G_N_ELEMENTS (entries_geojson), window);
		}
	}

	GtkIconFactory * icon_factory = gtk_icon_factory_new();
	gtk_icon_factory_add_default(icon_factory);

	register_vik_icons(icon_factory);

	/* Copy the tool RadioActionEntries out of the main Window structure into an extending array 'tools'
	   so that it can be applied to the UI in one action group add function call below. */
	GtkRadioActionEntry * radio_actions = NULL;
	unsigned int n_radio_actions = 0;
	for (unsigned int i = 0; i < window->tb->n_tools; i++) {
		radio_actions = (GtkRadioActionEntry *) realloc(radio_actions, (n_radio_actions + 1) * sizeof (GtkRadioActionEntry));
		radio_actions[n_radio_actions] = window->tb->tools[i]->radioActionEntry;
		++n_radio_actions;
		radio_actions[n_radio_actions].value = n_radio_actions;
	}

	for (LayerType i = LayerType::AGGREGATE; i < LayerType::NUM_TYPES; ++i) {
		GtkActionEntry action;
		gtk_ui_manager_add_ui(uim, mid,  "/ui/MainMenu/Layers/",
				      Layer::get_interface(i)->name,
				      Layer::get_interface(i)->name,
				      GTK_UI_MANAGER_MENUITEM, false);

		GtkIconSet * icon_set = gtk_icon_set_new_from_pixbuf(gdk_pixbuf_from_pixdata(Layer::get_interface(i)->icon, false, NULL));
		gtk_icon_factory_add(icon_factory, Layer::get_interface(i)->name, icon_set);
		gtk_icon_set_unref(icon_set);

		action.name = Layer::get_interface(i)->name;
		action.stock_id = Layer::get_interface(i)->name;
		action.label = g_strdup_printf(_("New _%s Layer"), Layer::get_interface(i)->name);
		action.accelerator = Layer::get_interface(i)->accelerator;
		action.tooltip = NULL;
		action.callback = (GCallback)menu_layer_new_cb;
		gtk_action_group_add_actions(action_group, &action, 1, window);

		free((char*)action.label);

		if (Layer::get_interface(i)->tools_count) {
			gtk_ui_manager_add_ui(uim, mid,  "/ui/MainMenu/Tools/", Layer::get_interface(i)->name, NULL, GTK_UI_MANAGER_SEPARATOR, false);
		}

#if 0 // Added to QT.
		// Further tool copying for to apply to the UI, also apply menu UI setup
		for (unsigned int j = 0; j < Layer::get_interface(i)->tools_count; j++) {

			LayerTool * layer_tool = Layer::get_interface(i)->layer_tool_constructors[j](window, window->viewport);
			window->tb->add_tool(layer_tool);
			assert (layer_tool->layer_type == i);

			gtk_ui_manager_add_ui(uim, mid,  "/ui/MainMenu/Tools",
					      layer_tool->radioActionEntry.label,
					      layer_tool->radioActionEntry.name,
					      GTK_UI_MANAGER_MENUITEM, false);


			radio_actions = (GtkRadioActionEntry *) realloc(radio_actions, (n_radio_actions + 1) * sizeof (GtkRadioActionEntry));
			radio_actions[n_radio_actions] = layer_tool->radioActionEntry;
			/* Overwrite with actual number to use. */
			++n_radio_actions;
			radio_actions[n_radio_actions].value = n_radio_actions;
		}
#endif

		GtkActionEntry action_dl;
		char *layername = g_strdup_printf("Layer%s", Layer::get_interface(i)->fixed_layer_name);
		gtk_ui_manager_add_ui(uim, mid,  "/ui/MainMenu/Edit/LayerDefaults",
				      Layer::get_interface(i)->name,
				      layername,
				      GTK_UI_MANAGER_MENUITEM, false);
		free(layername);

		// For default layers use action names of the form 'Layer<LayerName>'
		// This is to avoid clashing with just the layer name used above for the tool actions
		action_dl.name = g_strconcat("Layer", Layer::get_interface(i)->fixed_layer_name, NULL);
		action_dl.stock_id = NULL;
		action_dl.label = g_strconcat("_", Layer::get_interface(i)->name, "...", NULL); // Prepend marker for keyboard accelerator
		action_dl.accelerator = NULL;
		action_dl.tooltip = NULL;
		action_dl.callback = (GCallback)layer_defaults_cb;
		gtk_action_group_add_actions(action_group, &action_dl, 1, window);
		free((char*)action_dl.name);
		free((char*)action_dl.label);
	}
	g_object_unref(icon_factory);

	gtk_action_group_add_radio_actions(action_group, radio_actions, n_radio_actions, 0, (GCallback) menu_cb, window);
	free(radio_actions);

	gtk_ui_manager_insert_action_group(uim, action_group, 0);

	for (LayerType i = LayerType::AGGREGATE; i < LayerType::NUM_TYPES; ++i) {
		for (unsigned int j = 0; j < Layer::get_interface(i)->tools_count; j++) {
			GtkAction * action = gtk_action_group_get_action(action_group,
									 Layer::get_interface(i)->layer_tools[j]->radioActionEntry.name);
			g_object_set(action, "sensitive", false, NULL);
		}
	}

	// This is done last so we don't need to track the value of mid anymore
	vik_ext_tools_add_action_items(window, window->uim, action_group, mid);

	window->action_group = action_group;

	GtkAccelGroup * accel_group = gtk_ui_manager_get_accel_group(uim);
	gtk_window_add_accel_group(GTK_WINDOW (window->get_toolkit_window()), accel_group);
	gtk_ui_manager_ensure_update(uim);

	window->setup_recent_files();
#endif

	a_background_post_init_window(this);
}



void Window::layer_tools_cb(QAction * qa)
{
	QString tool_name = qa->objectName();
	QString group_name = qa->actionGroup()->objectName();

	if (qa->actionGroup()->objectName() == "window") {
		/* User selected tool in "window" group, but that is not a good reason to
		   disable whole layer-specific group. Only deactivate the old tool. */
	} else {
		/* This can happen only of we are switching from tool in
		   "window" group to tool in a layer group. */
		qDebug() << "Window: switching from \"window\" tool to" << group_name << "tool";
	}


	QAction * old_action = this->tb->get_active_tool();
	assert (old_action);
	if (old_action) {
		qDebug() << "Window: deactivating old action" << old_action;
		this->tb->deactivate_tool(old_action);
	} else {
		qDebug() << "ERROR: Window: no old action found";
		return;
	}


	QAction * first_action = this->tb->set_group_enabled(group_name);
	if (first_action) {
		this->tb->activate_tool(first_action);
	} else {
		this->tb->activate_tool(qa);
	}

	/* White Magic, my friends ... White Magic... */
	int tool_id;

	this->viewport->setCursor(*this->tb->get_cursor_release(tool_name));

	if (group_name == "Pan") {
		this->current_tool = TOOL_PAN;
	} else if (tool_name == "Zoom") {
		this->current_tool = TOOL_ZOOM;
	} else if (tool_name == "Ruler") {
		this->current_tool = TOOL_RULER;
	} else if (tool_name == "Select") {
		this->current_tool = TOOL_SELECT;
	} else {
		for (LayerType layer_type = LayerType::AGGREGATE; layer_type < LayerType::NUM_TYPES; ++layer_type) {
			for (tool_id = 0; tool_id < Layer::get_interface(layer_type)->tools_count; tool_id++) {
				if (Layer::get_interface(layer_type)->layer_tools[tool_id]->radioActionEntry.name == tool_name) {
					this->current_tool = TOOL_LAYER;
					this->tool_layer_type = layer_type;
					this->tool_tool_id = tool_id;
				}
			}
		}
	}
#if 0
	draw_status_tool(window);
#endif
}


void Window::pan_click(QMouseEvent * event)
{
	fprintf(stderr, "WINDOW: pan click\n");
	/* Set panning origin. */
	this->pan_move_flag = false;
	this->pan_x = event->x();
	this->pan_y = event->y();
}




void Window::pan_move(QMouseEvent * event)
{
	fprintf(stderr, "WINDOW: pan move\n");
	if (this->pan_x != -1) {
		this->viewport->set_center_screen(this->viewport->get_width() / 2 - event->x() + this->pan_x,
						  this->viewport->get_height() / 2 - event->y() + this->pan_y);
		this->pan_move_flag = true;
		this->pan_x = event->x();
		this->pan_y = event->y();
		this->draw_update();
	}
}




void Window::pan_release(QMouseEvent * event)
{
	fprintf(stderr, "WINDOW: pan release\n");
	bool do_draw = true;

	if (this->pan_move_flag == false) {
		this->single_click_pending = !this->single_click_pending;
#if 0
		if (this->single_click_pending) {
			// Store offset to use
			this->delayed_pan_x = this->pan_x;
			this->delayed_pan_y = this->pan_y;
			// Get double click time
			GtkSettings *gs = gtk_widget_get_settings(this->get_toolkit_widget());
			GValue dct = { 0 }; // = G_VALUE_INIT; // GLIB 2.30+ only
			g_value_init(&dct, G_TYPE_INT);
			g_object_get_property(G_OBJECT(gs), "gtk-double-click-time", &dct);
			// Give chance for a double click to occur
			int timer = g_value_get_int(&dct) + 50;
			g_timeout_add(timer, (GSourceFunc) vik_window_pan_timeout, this);
			do_draw = false;
		} else {
#endif
			this->viewport->set_center_screen(this->pan_x, this->pan_y);
#if 0
		}
#endif
	} else {
		this->viewport->set_center_screen(this->viewport->get_width() / 2 - event->x() + this->pan_x,
						  this->viewport->get_height() / 2 - event->y() + this->pan_y);
	}

	this->pan_move_flag = false;
	this->pan_x = -1;
	this->pan_y = -1;
	if (do_draw) {
		this->draw_update();
	}
}



void Window::preferences_cb(void) /* Slot. */
{
#if 0
	bool wp_icon_size = a_vik_get_use_large_waypoint_icons();
#endif
	a_preferences_show_window((QWindow *) this);
#if 0
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
#endif
}




void Window::closeEvent(QCloseEvent * event)
{
#if 0
#ifdef VIKING_PROMPT_IF_MODIFIED
	if (window->modified)
#else
	if (0)
#endif
#endif
	{
		QMessageBox::StandardButton reply = QMessageBox::question(this, "SlavGPS",
									  QString("Changes in file '%1' are not saved and will be lost if you don't save them.\n\n"
										  "Do you want to save the changes?").arg("some file"), // window->get_filename()
									  QMessageBox::Cancel | QMessageBox::No | QMessageBox::Yes,
									  QMessageBox::Yes);
		if (reply == QMessageBox::No) {
			event->accept();
		} else if (reply == QMessageBox::Yes) {
			//save_file(NULL, window);
			event->accept();
		} else {
			event->ignore();
		}
	}


#if 0
	if (window_count == 1)
#endif
	{

		// On the final window close - save latest state - if it's wanted...
		if (Preferences::get_restore_window_state()) {

			const Qt::WindowStates states = this->windowState();



			bool state_max = states.testFlag(Qt::WindowMaximized);
			a_settings_set_boolean(VIK_SETTINGS_WIN_MAX, state_max);

			bool state_fullscreen = states.testFlag(Qt::WindowFullScreen);
			a_settings_set_boolean(VIK_SETTINGS_WIN_FULLSCREEN, state_fullscreen);

#if 0

			a_settings_set_boolean(VIK_SETTINGS_WIN_SIDEPANEL, window->layers_panel->get_visible());

			a_settings_set_boolean(VIK_SETTINGS_WIN_STATUSBAR, GTK_WIDGET_VISIBLE (GTK_WIDGET(window->viking_vs)));

			a_settings_set_boolean(VIK_SETTINGS_WIN_TOOLBAR, GTK_WIDGET_VISIBLE (toolbar_get_widget(window->viking_vtb)));



			// If supersized - no need to save the enlarged width+height values
			if (! (state_fullscreen || state_max)) {
				int width, height;
				gtk_window_get_size(gtk_window, &width, &height);
				a_settings_set_integer(VIK_SETTINGS_WIN_WIDTH, width);
				a_settings_set_integer(VIK_SETTINGS_WIN_HEIGHT, height);
			}

			a_settings_set_integer(VIK_SETTINGS_WIN_PANE_POSITION, gtk_paned_get_position(GTK_PANED(window->hpaned)));
#endif
		}
#if 0
		a_settings_set_integer(VIK_SETTINGS_WIN_SAVE_IMAGE_WIDTH, window->draw_image_width);
		a_settings_set_integer(VIK_SETTINGS_WIN_SAVE_IMAGE_HEIGHT, window->draw_image_height);
		a_settings_set_boolean(VIK_SETTINGS_WIN_SAVE_IMAGE_PNG, window->draw_image_save_as_png);

		char *accel_file_name = g_build_filename(get_viking_dir(), VIKING_ACCELERATOR_KEY_FILE, NULL);
		gtk_accel_map_save(accel_file_name);
		free(accel_file_name);
#endif

	}
}




void Window::view_full_screen_cb(bool new_state)
{
	assert (new_state != this->view_full_screen);
	if (new_state != this->view_full_screen) {
		this->toggle_full_screen();
	}
}




void Window::draw_scale_cb(bool new_state)
{
	assert (new_state != this->draw_scale);
	if (new_state != this->draw_scale) {
		this->viewport->set_draw_scale(new_state);
		this->draw_update();
		this->draw_scale = !this->draw_scale;
	}
}




void Window::draw_centermark_cb(bool new_state)
{
	assert (new_state != this->draw_centermark);
	if (new_state != this->draw_centermark) {
		this->viewport->set_draw_centermark(new_state);
		this->draw_update();
		this->draw_centermark = !this->draw_centermark;
	}
}



void Window::draw_highlight_cb(bool new_state)
{
	assert (new_state != this->draw_highlight);
	if (new_state != this->draw_highlight) {
		this->viewport->set_draw_highlight(new_state);
		this->draw_update();
		this->draw_highlight = !this->draw_highlight;
	}
}



void Window::view_side_panel_cb(bool new_state)
{
	assert (new_state != this->view_side_panel);
	if (new_state != this->view_side_panel) {
		this->toggle_side_panel();
	}
}




void Window::view_statusbar_cb(bool new_state)
{
	assert (new_state != this->view_statusbar);
	if (new_state != this->view_statusbar) {
		this->toggle_statusbar();
	}
}




void Window::view_main_menu_cb(bool new_state)
{
	assert (new_state != this->view_main_menu);
	if (new_state != this->view_main_menu) {
		this->toggle_main_menu();
	}
}



void Window::toggle_full_screen()
{
	this->view_full_screen = !this->view_full_screen;
	const Qt::WindowStates state = this->windowState();

	if (this->view_full_screen) {
		this->setWindowState(state | Qt::WindowFullScreen);
	} else {
		this->setWindowState(state & (~Qt::WindowFullScreen));
	}
}




void Window::toggle_side_panel()
{
	this->view_side_panel = !this->view_side_panel;
	QAction * qa = this->panel_dock->toggleViewAction();
	qDebug() << "II: Window: setting panel dock visible:" << this->view_side_panel;
	qa->setChecked(this->view_side_panel);
	if (this->view_side_panel) {
		this->panel_dock->show();
	} else {
		this->panel_dock->hide();
	}
}




void Window::toggle_statusbar()
{
	this->view_statusbar = !this->view_statusbar;
	if (this->view_statusbar) {
		//gtk_widget_show(GTK_WIDGET(this->viking_vs));
	} else {
		//gtk_widget_hide(GTK_WIDGET(this->viking_vs));
	}
}




void Window::toggle_main_menu()
{
	this->view_main_menu = !this->view_main_menu;
	if (this->view_main_menu) {
		//gtk_widget_show(gtk_ui_manager_get_widget(this->uim, "/ui/MainMenu"));
	} else {
		//gtk_widget_hide(gtk_ui_manager_get_widget(this->uim, "/ui/MainMenu"));
	}
}




void Window::zoom_cb(void)
{
	QAction * qa = (QAction *) QObject::sender();
	QKeySequence seq = qa->shortcut();

	if (seq == (Qt::CTRL + Qt::Key_Plus)) {
		this->viewport->zoom_in();
	} else if (seq == (Qt::CTRL + Qt::Key_Minus)) {
		this->viewport->zoom_out();
	} else {
		qDebug() << "EE: Window: unhandled case";
		return;
	}

#if 0
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
#endif
	this->draw_update();
}




void Window::zoom_to_cb(void)
{
	double xmpp = this->viewport->get_xmpp();
	double ympp = this->viewport->get_ympp();
#if 0
	if (a_dialog_custom_zoom(window->get_toolkit_window(), &xmpp, &ympp)) {
		window->viewport->set_xmpp(xmpp);
		window->viewport->set_ympp(ympp);
		window->draw_update();
	}
#endif
}





/**
 * Display the background jobs window.
 */
void Window::show_background_jobs_window_cb(void)
{
	a_background_show_window();
}
