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

#include "window_layer_tools.h"
#include "window.h"
#include "viewport.h"
#include "layer.h"
#include "layer_defaults.h"
#include "layers_panel.h"
#include "layer_toolbox.h"
#include "globals.h"
#include "uibuilder_qt.h"
#include "settings.h"
#include "background.h"
#include "dialog.h"
#include "util.h"
#include "file.h"
#include "fileutils.h"




using namespace SlavGPS;




/* The last used directories. */
static QUrl last_folder_files_url;




Window::Window()
{

	strcpy(this->type_string, "SG QT WINDOW");

	QIcon::setThemeName("Tango");
	this->create_layout();
	this->create_actions();


	this->layer_toolbox = new LayerToolbox(this);

	this->create_ui();


	/* Own signals. */
	connect(this->viewport, SIGNAL(updated_center(void)), this, SLOT(center_changed_cb(void)));
	connect(this->layers_panel, SIGNAL(update()), this, SLOT(draw_update_cb()));


#if 0
	this->viewport = new Viewport();
	this->layers_panel = new LayersPanel();
	this->layers_panel->set_viewport(this->viewport);
	this->viking_vs = vik_statusbar_new();

	this->viking_vtb = vik_toolbar_new();

	this->layer_toolbox = new LayerToolbox(this);

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
			toolbar_action_set_sensitive(this->viking_vtb, Layer::get_interface(i)->layer_tools[j]->id_string, false);
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



	g_signal_connect_swapped(G_OBJECT(this->layers_panel), "delete_layer", G_CALLBACK(vik_window_clear_highlight_cb), this);

	// Allow key presses to be processed anywhere
	g_signal_connect_swapped(this->get_toolkit_object(), "key_press_event", G_CALLBACK (key_press_event_cb), this);

	// Set initial button sensitivity
	center_changed_cb(this);

	this->hpaned = gtk_hpaned_new();
	gtk_paned_pack1(GTK_PANED(this->hpaned), this->layers_panel, false, true);
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



	this->status_bar = new StatusBar(this);
	this->setStatusBar(this->status_bar);


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
	QAction * qa_file_open = NULL;
	QAction * qa_file_exit = NULL;
	{
		QAction * qa = NULL;

		qa_file_new = this->menu_file->addAction(QIcon::fromTheme("document-new"), _("&New file..."));
		qa_file_new->setShortcut(Qt::CTRL + Qt::Key_N);
		qa_file_new->setIcon(QIcon::fromTheme("document-new"));
		qa_file_new->setToolTip("Open a file");

		qa_file_open = this->menu_file->addAction(QIcon::fromTheme("document-open"), _("&Open..."));
		qa_file_open->setShortcut(Qt::CTRL + Qt::Key_O);
		qa_file_open->setData(QVariant((int) 12)); /* kamilFIXME: magic number. */
		connect(qa_file_open, SIGNAL (triggered(bool)), this, SLOT (open_file_cb()));

		/* This submenu will be populated by Window::update_recent_files(). */
		this->submenu_recent_files = this->menu_file->addMenu(QString("Open &Recent File"));

		qa = this->menu_file->addAction(QIcon::fromTheme("list-add"), _("Append &File..."));
		qa->setData(QVariant((int) 21)); /* kamilFIXME: magic number. */
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (open_file_cb()));
		qa->setToolTip("Append data from a different file");

		qa_file_exit = this->menu_file->addAction(QIcon::fromTheme("application-exit"), _("E&xit"));
		qa_file_exit->setShortcut(Qt::CTRL + Qt::Key_X);
		connect(qa_file_exit, SIGNAL (triggered(bool)), this, SLOT (close(void)));
	}


	/* "Edit" menu. */
	{
		QAction * qa = NULL;
		qa = new QAction("&Preferences", this);
		qa->setIcon(QIcon::fromTheme("preferences-other"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (preferences_cb(void)));
		this->menu_edit->addAction(qa);

		{
			QMenu * defaults_submenu = this->menu_edit->addMenu(QIcon::fromTheme("document-properties"), QString("&Layer Defaults"));

			for (LayerType i = LayerType::AGGREGATE; i < LayerType::NUM_TYPES; ++i) {
				qa = defaults_submenu->addAction("&" + QString(Layer::get_interface(i)->layer_name) + "...");
				qa->setData(QVariant((int) i));
				qa->setIcon(Layer::get_interface(i)->action_icon);
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (show_layer_defaults_cb()));
			}
		}
	}


	/* "View" menu. */
	QAction * qa_view_zoom_in = NULL;
	QAction * qa_view_zoom_out = NULL;
	QAction * qa_view_zoom_to = NULL;
	{
		this->qa_view_full_screen = new QAction("&Full Screen", this);
		this->qa_view_full_screen->setShortcut(Qt::Key_F11);
		this->qa_view_full_screen->setCheckable(true);
		this->qa_view_full_screen->setChecked(this->view_full_screen);
		this->qa_view_full_screen->setToolTip("Activate full screen mode");
		connect(this->qa_view_full_screen, SIGNAL(triggered(bool)), this, SLOT(view_full_screen_cb(bool)));
		/* TODO: icon: GTK_STOCK_FULLSCREEN */

		this->menu_view->addAction(this->qa_view_full_screen);


		QMenu * show_submenu = new QMenu("&Show", this);
		this->menu_view->addMenu(show_submenu);

		this->qa_view_show_draw_scale = new QAction("Show &Scale", this);
		this->qa_view_show_draw_scale->setShortcut(Qt::SHIFT + Qt::Key_F5);
		this->qa_view_show_draw_scale->setCheckable(true);
		this->qa_view_show_draw_scale->setChecked(this->draw_scale);
		this->qa_view_show_draw_scale->setToolTip("Show Scale");
		connect(this->qa_view_show_draw_scale, SIGNAL(triggered(bool)), this, SLOT(draw_scale_cb(bool)));

		this->qa_view_show_draw_centermark = new QAction("Show &Center Mark", this);
		this->qa_view_show_draw_centermark->setShortcut(Qt::Key_F6);
		this->qa_view_show_draw_centermark->setCheckable(true);
		this->qa_view_show_draw_centermark->setChecked(this->draw_centermark);
		this->qa_view_show_draw_centermark->setToolTip("Show Center Mark");
		connect(this->qa_view_show_draw_centermark, SIGNAL(triggered(bool)), this, SLOT(draw_centermark_cb(bool)));

		this->qa_view_show_draw_highlight = new QAction("Show &Highlight", this);
		this->qa_view_show_draw_highlight->setShortcut(Qt::Key_F7);
		this->qa_view_show_draw_highlight->setCheckable(true);
		this->qa_view_show_draw_highlight->setChecked(this->draw_highlight);
		this->qa_view_show_draw_highlight->setToolTip("Show Highlight");
		connect(this->qa_view_show_draw_highlight, SIGNAL(triggered(bool)), this, SLOT(draw_highlight_cb(bool)));
		/* TODO: icon: GTK_STOCK_UNDERLINE */

		this->qa_view_show_side_panel = this->panel_dock->toggleViewAction(); /* Existing action! */
		this->qa_view_show_side_panel->setText("Show Side &Panel");
		this->qa_view_show_side_panel->setShortcut(Qt::Key_F9);
		this->qa_view_show_side_panel->setCheckable(true);
		this->qa_view_show_side_panel->setChecked(this->view_side_panel);
		this->qa_view_show_side_panel->setToolTip("Show Side Panel");
		connect(this->qa_view_show_side_panel, SIGNAL(triggered(bool)), this, SLOT(view_side_panel_cb(bool)));
		/* TODO: icon: GTK_STOCK_INDEX */

		this->qa_view_show_statusbar = new QAction("Show Status&bar", this);
		this->qa_view_show_statusbar->setShortcut(Qt::Key_F12);
		this->qa_view_show_statusbar->setCheckable(true);
		this->qa_view_show_statusbar->setChecked(this->view_statusbar);
		this->qa_view_show_statusbar->setToolTip("Show Statusbar");
		connect(this->qa_view_show_statusbar, SIGNAL(triggered(bool)), this, SLOT(view_statusbar_cb(bool)));

		this->qa_view_show_toolbar = this->toolbar->toggleViewAction(); /* Existing action! */
		this->qa_view_show_toolbar->setText("Show &Toolbar");
		this->qa_view_show_toolbar->setShortcut(Qt::Key_F3);
		this->qa_view_show_toolbar->setCheckable(true);
		this->qa_view_show_toolbar->setChecked(this->view_toolbar);
		this->qa_view_show_toolbar->setToolTip("Show Toolbar");
		/* No signal connection needed, we have toggleViewAction(). */

		this->qa_view_show_main_menu = new QAction("Show &Menu", this);
		this->qa_view_show_main_menu->setShortcut(Qt::Key_F4);
		this->qa_view_show_main_menu->setCheckable(true);
		this->qa_view_show_main_menu->setChecked(this->view_main_menu);
		this->qa_view_show_main_menu->setToolTip("Show Menu");
		connect(qa_view_show_main_menu, SIGNAL(triggered(bool)), this, SLOT(view_main_menu_cb(bool)));


		show_submenu->addAction(this->qa_view_show_draw_scale);
		show_submenu->addAction(this->qa_view_show_draw_centermark);
		show_submenu->addAction(this->qa_view_show_draw_highlight);
		show_submenu->addAction(this->qa_view_show_side_panel);
		show_submenu->addAction(this->qa_view_show_statusbar);
		show_submenu->addAction(this->qa_view_show_toolbar);
		show_submenu->addAction(this->qa_view_show_main_menu);


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

		qa = new QAction("Show Centers", this);
		connect(qa, SIGNAL(triggered(bool)), this, SLOT(show_centers_cb(void)));
		this->menu_view->addAction(qa);
	}



	/* "Layers" menu. */
	{
		this->qa_layer_properties = new QAction("Properties...", this);
		this->menu_layers->addAction(this->qa_layer_properties);
		connect (this->qa_layer_properties, SIGNAL (triggered(bool)), this->layers_panel, SLOT (properties_cb(void)));

		this->new_layers_submenu_add_actions(this->menu_layers);
	}


	/* "Help" menu. */
	{
		QAction * qa_help_help = new QAction("&Help", this);
		qa_help_help->setIcon(QIcon::fromTheme("help-contents"));
		qa_help_help->setShortcut(Qt::Key_F1);
		connect(qa_help_help, SIGNAL (triggered(bool)), this, SLOT (help_help_cb(void)));

		QAction * qa_help_about = new QAction("&About", this);
		qa_help_about->setIcon(QIcon::fromTheme("help-about"));
		connect(qa_help_about, SIGNAL (triggered(bool)), this, SLOT (help_about_cb(void)));

		this->menu_help->addAction(qa_help_help);
		this->menu_help->addAction(qa_help_about);
	}


	this->toolbar->addAction(qa_file_new);


	return;
}




void Window::draw_update_cb()
{
	qDebug() << "SLOT: Window: received 'update' signal from Layers Panel";
	this->draw_update();
}




void Window::draw_update()
{
	qDebug() << "II: Window: redraw + sync begin" << __FUNCTION__ << __LINE__;
	this->draw_redraw();
	this->draw_sync();
	qDebug() << "II: Window: redraw + sync end" << __FUNCTION__ << __LINE__;
}




static void draw_sync_cb(Window * window)
{
	window->draw_sync();
}




void Window::draw_sync()
{
	qDebug() << "II: Window: sync begin" << __FUNCTION__ << __LINE__;
	//this->viewport->sync();
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

	qDebug() << "II: Window: zoom level is" << zoom_level;
	QString message(zoom_level);
	this->status_bar->set_message(StatusBarField::ZOOM, message);
	this->display_tool_name();
}




void Window::menu_layer_new_cb(void) /* Slot. */
{
	QAction * qa = (QAction *) QObject::sender();
	SlavGPS::LayerType layer_type = (SlavGPS::LayerType) qa->data().toInt();

	qDebug() << "II: Window: clicked \"layer new\" for layer type" << (int) layer_type << Layer::get_interface(layer_type)->layer_type_string;

	if (this->layers_panel->new_layer(layer_type)) {
		qDebug() << "II: Window: new layer, call draw_update_cb()" << __FUNCTION__ << __LINE__;
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

	if (this->viewport->get_draw_highlight()) {
		if (this->containing_trw && (this->selected_tracks || this->selected_waypoints)) {
			this->containing_trw->draw_highlight_items(this->selected_tracks, this->selected_waypoints, this->viewport);

		} else if (this->containing_trw && (this->selected_track || this->selected_waypoint)) {
			this->containing_trw->draw_highlight_item((Track *) this->selected_track, this->selected_waypoint, this->viewport);

		} else if (this->selected_trw) {
			this->selected_trw->draw_highlight(this->viewport);
		}
	}

	/* Other viewport decoration items on top if they are enabled/in use. */
	this->viewport->draw_scale();
	this->viewport->draw_copyrights();
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




/* Called when user selects a layer in tree view. */
void Window::selected_layer(Layer * layer)
{
	QString layer_type(layer->get_interface(layer->type)->layer_type_string);
	qDebug() << "II: Window: selected layer type" << layer_type;

	this->layer_toolbox->selected_layer(layer_type);
}




Viewport * Window::get_viewport()
{
	return this->viewport;
}




LayersPanel * Window::get_layers_panel()
{
	return this->layers_panel;
}




LayerToolbox * Window::get_layer_tools_box(void)
{
	return this->layer_toolbox;
}




/**
 * Returns the statusbar for the window
 */
StatusBar * Window::get_statusbar()
{
	return this->status_bar;
}




/**
 * @field:   The part of the statusbar to be updated.
 * @message: The string to be displayed. This is copied.

 * This updates any part of the statusbar with the new string.
 */
void Window::statusbar_update(StatusBarField field, QString const & message)
{
	this->status_bar->set_message(field, message);
}









/**
 * center_changed_cb:
 */
void Window::center_changed_cb(void) /* Slot. */
{
	qDebug() << "SLOT: Window: center changed";
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




QMenu * Window::new_layers_submenu_add_actions(QMenu * menu)
{
	for (SlavGPS::LayerType i = SlavGPS::LayerType::AGGREGATE; i < SlavGPS::LayerType::NUM_TYPES; ++i) {

		QVariant variant((int) i);
		QAction * qa = new QAction("new layer", this);
		qa->setData(variant);
		qa->setIcon(Layer::get_interface(i)->action_icon);
		connect(qa, SIGNAL(triggered(bool)), this, SLOT(menu_layer_new_cb(void)));

		menu->addAction(qa);
	}

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
		group->setObjectName("generic");
		QAction * qa = NULL;
		QAction * default_qa = NULL;

		this->toolbar->addSeparator();


		qa = this->layer_toolbox->add_tool(selecttool_create(this, this->viewport));
		group->addAction(qa);
		default_qa = qa;

		qa = this->layer_toolbox->add_tool(ruler_create(this, this->viewport));
		group->addAction(qa);

		qa = this->layer_toolbox->add_tool(zoomtool_create(this, this->viewport));
		group->addAction(qa);

		qa = this->layer_toolbox->add_tool(pantool_create(this, this->viewport));
		group->addAction(qa);


		this->toolbar->addActions(group->actions());
		this->menu_tools->addActions(group->actions());
		this->layer_toolbox->add_group(group);

		/* The same callback for all layer tools. */
		connect(group, SIGNAL(triggered(QAction *)), this, SLOT(layer_tool_cb(QAction *)));
		default_qa->setChecked(true);
		default_qa->trigger();
		this->layer_toolbox->activate_tool(default_qa);
	}


	{
		for (LayerType i = LayerType::AGGREGATE; i < LayerType::NUM_TYPES; ++i) {

			if (!Layer::get_interface(i)->tools_count) {
				continue;
			}
			this->toolbar->addSeparator();
			this->menu_tools->addSeparator();

			QActionGroup * group = new QActionGroup(this);
			group->setObjectName(Layer::get_interface(i)->layer_name);

			unsigned int j = 0;
			for (j = 0; j < Layer::get_interface(i)->tools_count; j++) {

				LayerTool * layer_tool = Layer::get_interface(i)->layer_tool_constructors[j](this, this->viewport);
				QAction * qa = this->layer_toolbox->add_tool(layer_tool);
				group->addAction(qa);

				assert (layer_tool->layer_type == i);
			}
			this->toolbar->addActions(group->actions());
			this->menu_tools->addActions(group->actions());
			this->layer_toolbox->add_group(group);
			group->setEnabled(false); /* A layer-specific tool group is disabled by default, until a specific layer is selected in tree view. */

			/* The same callback for all layer tools. */
			connect(group, SIGNAL (triggered(QAction *)), this, SLOT (layer_tool_cb(QAction *)));
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
	for (unsigned int i = 0; i < window->layer_toolbox->n_tools; i++) {
		radio_actions = (GtkRadioActionEntry *) realloc(radio_actions, (n_radio_actions + 1) * sizeof (GtkRadioActionEntry));
		radio_actions[n_radio_actions] = window->layer_toolbox->tools[i]->radioActionEntry;
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
		action.action_icon_path = Layer::get_interface(i)->name;
		action.action_label = g_strdup_printf(_("New _%s Layer"), Layer::get_interface(i)->name);
		action.action_tooltip = NULL;
		action.action_accelerator = Layer::get_interface(i)->accelerator;
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
			window->layer_toolbox->add_tool(layer_tool);
			assert (layer_tool->layer_type == i);

			gtk_ui_manager_add_ui(uim, mid,  "/ui/MainMenu/Tools",
					      layer_tool->radioActionEntry.label,
					      layer_tool->id_string,
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
		action_dl.action_icon_path = NULL;
		action_dl.action_label = g_strconcat("_", Layer::get_interface(i)->name, "...", NULL); // Prepend marker for keyboard accelerator
		action_dl.action_tooltip = NULL;
		// action_dl.action_accelerator = ...; /* Empty accelerator. */
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
									 Layer::get_interface(i)->layer_tools[j]->id_string);
			g_object_set(action, "sensitive", false, NULL);
		}
	}

	// This is done last so we don't need to track the value of mid anymore
	vik_ext_tools_add_action_items(window, window->uim, action_group, mid);

	window->action_group = action_group;

	GtkAccelGroup * accel_group = gtk_ui_manager_get_accel_group(uim);
	gtk_window_add_accel_group(GTK_WINDOW (window->get_toolkit_window()), accel_group);
	gtk_ui_manager_ensure_update(uim);
#endif

	a_background_post_init_window(this);
}




/* Callback common for all layer tool actions. */
void Window::layer_tool_cb(QAction * qa)
{
	/* Handle old tool first. */
	QAction * old_qa = this->layer_toolbox->get_active_tool_action();
	if (old_qa) {
		qDebug() << "II: Window: deactivating old tool" << old_qa->objectName();
		this->layer_toolbox->deactivate_tool(old_qa);
	} else {
		/* The only valid situation when it happens is only during start up of application. */
		qDebug() << "WW: Window: no old action found";
	}


	/* Now handle newly selected tool. */
	if (qa) {
		this->layer_toolbox->activate_tool(qa);

		QString tool_name = qa->objectName();
		qDebug() << "II: Window: setting 'release' cursor for" << tool_name;
		this->viewport->setCursor(*this->layer_toolbox->get_cursor_release(tool_name));
		this->current_tool = this->layer_toolbox->get_tool(tool_name);
		this->display_tool_name();
	}
}




void Window::pan_click(QMouseEvent * event)
{
	qDebug() << "II: Window: pan click";
	/* Set panning origin. */
	this->pan_move_flag = false;
	this->pan_x = event->x();
	this->pan_y = event->y();
}




void Window::pan_move(QMouseEvent * event)
{
	qDebug() << "II: Window: pan move";
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
	qDebug() << "II: Window: pan release";
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
	preferences_show_window(this);
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




void Window::display_tool_name(void)
{
	this->status_bar->set_message(StatusBarField::TOOL, this->current_tool->get_description());
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




void Window::set_redraw_trigger(Layer * layer)
{
	Window * window = layer->get_window();
	if (window) {
		window->trigger = layer;
	}
}




void Window::show_layer_defaults_cb(void)
{
	QAction * qa = (QAction *) QObject::sender();
	LayerType layer_type = (SlavGPS::LayerType) qa->data().toInt();

	qDebug() << "II: Window: clicked \"layer defaults\" for layer type" << (int) layer_type << Layer::get_interface(layer_type)->layer_type_string;

	if (!layer_defaults_show_window(layer_type, this)) {
		dialog_info("This layer has no configurable properties.", this);
	}

	/* No update needed. */
}




void Window::open_file_cb(void)
{
	QAction * qa = (QAction *) QObject::sender();

	bool newwindow;
	if (qa->data().toInt() == 12) {
		newwindow = true;
	} else if (qa->data().toInt() == 21) {
		newwindow = false;
	} else {
		qDebug() << "EE: Window: unrecognized Open/Append action value:" << qa->data().toInt();
		return;
	}

	//GSList *files = NULL;
	//GSList *cur_file = NULL;


	QFileDialog dialog(this, "Please select a GPS data file to open.");

	if (last_folder_files_url.isValid()) {
		dialog.setDirectoryUrl(last_folder_files_url);
	}

	QStringList filter;

	/* File filters are listed this way for alphabetical ordering. */
#ifdef VIK_CONFIG_GEOCACHES
	filter << _("Geocaching (*.loc)");
#endif

#ifdef K
	gtk_file_filter_set_name(filter, _("Google Earth"));
	gtk_file_filter_add_mime_type(filter, "application/vnd.google-earth.kml+xml");
#endif

	filter << _("GPX (*.gpx)");

#ifdef K
	gtk_file_filter_set_name(filter, _("JPG"));
	gtk_file_filter_add_mime_type(filter, "image/jpeg");
#endif

	filter << _("Viking (*.vik *.viking)");


	/* Could have filters for gpspoint (*.gps,*.gpsoint?) + gpsmapper (*.gsm,*.gpsmapper?).
	   However assume this are barely used and thus not worthy of inclusion
	   as they'll just make the options too many and have no clear file pattern.
	   One can always use the all option. */
	filter << _("All (*)");

	dialog.setNameFilters(filter);

#ifdef K
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), true);
#endif


	int dialog_code = dialog.exec();
	if (dialog_code == QDialog::Accepted) {
		last_folder_files_url = dialog.directoryUrl();
#ifdef K

#ifdef VIKING_PROMPT_IF_MODIFIED
		if ((window->modified || window->filename) && newwindow) {
#else
		if (window->filename && newwindow) {
#endif
			g_signal_emit(window->get_toolkit_object(), window_signals[VW_OPENWINDOW_SIGNAL], 0, gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog)));
		} else {
#endif

			QStringList files = dialog.selectedFiles();
			bool change_fn = newwindow && (files.size() == 1); /* Only change fn if one file. */
			bool first_vik_file = true;
			auto iter = files.begin();
			while (iter != files.end()) {

				QString file_name = *iter;
				if (newwindow && check_file_magic_vik(file_name.toUtf8().data())) {
					/* Load first of many .vik files in current window. */
					if (first_vik_file) {
						this->open_file(file_name.toUtf8().data(), true);
						first_vik_file = false;
					} else {
#ifdef K
						/* Load each subsequent .vik file in a separate window. */
						Window * new_window = Window::new_window();
						if (new_window) {
							new_window->open_file(file_name.toUtf8().data(), true);
						}
#endif
					}
				} else {
					/* Other file types. */
					this->open_file(file_name.toUtf8().data(), change_fn);
				}

				iter++;
			}
#ifdef K
		}
#endif
	}
}




void Window::open_file(char const * new_filename, bool change_filename)
{
	this->set_busy_cursor();

	/* Enable the *new* filename to be accessible by the Layers code. */
	char * original_filename = this->filename ? strdup(this->filename) : NULL;
	free(this->filename);
	this->filename = strdup(new_filename);
	bool success = false;
	bool restore_original_filename = false;

	LayerAggregate * agg = this->layers_panel->get_top_layer();
	this->loaded_type = a_file_load(agg, this->viewport, new_filename);
	switch (this->loaded_type) {
	case LOAD_TYPE_READ_FAILURE:
		dialog_error("The file you requested could not be opened.", this);
		break;
	case LOAD_TYPE_GPSBABEL_FAILURE:
		dialog_error("GPSBabel is required to load files of this type or GPSBabel encountered problems.", this);
		break;
	case LOAD_TYPE_GPX_FAILURE:
		dialog_error(QString("Unable to load malformed GPX file %1").arg(QString(new_filename)), this);
		break;
	case LOAD_TYPE_UNSUPPORTED_FAILURE:
		dialog_error(QString("Unsupported file type for %1").arg(QString(new_filename)), this);
		break;
	case LOAD_TYPE_VIK_FAILURE_NON_FATAL:
		{
			/* Since we can process .vik files with issues just show a warning in the status bar.
			   Not that a user can do much about it... or tells them what this issue is yet... */
			this->get_statusbar()->set_message(StatusBarField::INFO, QString("WARNING: issues encountered loading %1").arg(file_basename(new_filename)));
		}
		/* No break, carry on to show any data. */
	case LOAD_TYPE_VIK_SUCCESS:
		{

			restore_original_filename = true; /* Will actually get inverted by the 'success' component below. */
			/* Update UI */
			if (change_filename) {
				this->set_filename(new_filename);
			}
#ifdef K
			GtkWidget * mode_button = this->get_drawmode_button(this->viewport->get_drawmode());
			this->only_updating_coord_mode_ui = true; /* if we don't set this, it will change the coord to UTM if we click Lat/Lon. I don't know why. */
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mode_button), true);
			this->only_updating_coord_mode_ui = false;
#endif

			this->layers_panel->change_coord_mode(this->viewport->get_coord_mode());

			/* Slightly long winded methods to align loaded viewport settings with the UI. */
			bool vp_state_scale = this->viewport->get_draw_scale();
			bool ui_state_scale = this->qa_view_show_draw_scale->isChecked();
			if (vp_state_scale != ui_state_scale) {
				this->viewport->set_draw_scale(!vp_state_scale);
				this->draw_scale_cb(!vp_state_scale);
			}
			bool vp_state_centermark = this->viewport->get_draw_centermark();
			bool ui_state_centermark = this->qa_view_show_draw_centermark->isChecked();
			if (vp_state_centermark != ui_state_centermark) {
				this->viewport->set_draw_centermark(!vp_state_centermark);
				this->draw_centermark_cb(!vp_state_centermark);
			}
			bool vp_state_highlight = this->viewport->get_draw_highlight();
			bool ui_state_highlight = this->qa_view_show_draw_highlight->isChecked();
			if (vp_state_highlight != ui_state_highlight) {
				this->viewport->set_draw_highlight(!vp_state_highlight);
				this->draw_highlight_cb(!vp_state_highlight);
			}
		}
		/* No break, carry on to redraw. */
		//case LOAD_TYPE_OTHER_SUCCESS:
	default:
		success = true;
		/* When LOAD_TYPE_OTHER_SUCCESS *only*, this will maintain the existing Viking project. */
		restore_original_filename = ! restore_original_filename;
		this->update_recently_used_document(new_filename);
		this->update_recent_files(QString(new_filename));
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




void Window::update_recent_files(QString const & path)
{
	/*
	  TODO
	  - add file type filter? gtk_recent_filter_add_group(filter, "viking");
	  - consider different sorting orders? gtk_recent_chooser_set_sort_type(GTK_RECENT_CHOOSER (menu), GTK_RECENT_SORT_MRU);
	*/

	/* Remove existing duplicate. */
	for (auto iter = this->recent_files.begin(); iter != this->recent_files.end(); iter++) {
		if (*iter == path) { /* This test will become more complicated as elements stored in ::recent_files become more complex. */
			this->recent_files.erase(iter);
			break;
		}
	}

	this->recent_files.push_front(path);

	unsigned int limit = a_vik_get_recent_number_files();

	/* Remove "oldest" files from the list. */
	while (this->recent_files.size() > limit) {
		this->recent_files.pop_back();
	}

	/* Clear and regenerate "recent files" menu. */
	this->submenu_recent_files->clear();
	for (auto iter = this->recent_files.begin(); iter != this->recent_files.end(); iter++) {
		QAction * qa = this->submenu_recent_files->addAction(*iter);
		qa->setToolTip(*iter);
		/* TODO: connect the action to slot. */
	}
}




void Window::update_recently_used_document(char const * filename)
{
#ifdef K
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
		this->get_statusbar()->set_message(StatusBarField::INFO, QString("Unable to add '%s' to the list of recently used documents").arg(uri));
	}

	free(uri);
	free(basename);
	free(recent_data->app_exec);
	g_slice_free(GtkRecentData, recent_data);
#endif
}





 /**
 * Call this before doing things that may take a long time and otherwise not show any other feedback
 * such as loading and saving files.
 */
void Window::set_busy_cursor()
{
#ifdef K
	gdk_window_set_cursor(gtk_widget_get_window(this->get_toolkit_widget()), this->busy_cursor);
	// Viewport has a separate cursor
	gdk_window_set_cursor(gtk_widget_get_window(this->viewport->get_toolkit_widget()), this->busy_cursor);
	// Ensure cursor updated before doing stuff
	while (gtk_events_pending()) {
		gtk_main_iteration();
	}
#endif
}




void Window::clear_busy_cursor()
{
#ifdef K
	gdk_window_set_cursor(gtk_widget_get_window(this->get_toolkit_widget()), NULL);
	// Restore viewport cursor
	gdk_window_set_cursor(gtk_widget_get_window(this->viewport->get_toolkit_widget()), this->viewport_cursor);
#endif
}




void Window::set_filename(char const * filename)
{
	if (this->filename) {
		free(this->filename);
		this->filename = NULL;
	}
	if (filename) {
		this->filename = strdup(filename);
	}

	/* Refresh window's title */
	this->setWindowTitle(QString("%1 - SlavGPS").arg(this->get_filename()));
}




char const * Window::get_filename()
{
	return this->filename ? file_basename(this->filename) : _("Untitled");
}





GtkWidget * Window::get_drawmode_button(ViewportDrawMode mode)
{
	GtkWidget *mode_button;
	char *buttonname;
	switch (mode) {
#ifdef VIK_CONFIG_EXPEDIA
	case ViewportDrawMode::EXPEDIA:
		buttonname = (char *) "/ui/MainMenu/View/ModeExpedia";
		break;
#endif
	case ViewportDrawMode::MERCATOR:
		buttonname = (char *) "/ui/MainMenu/View/ModeMercator";
		break;
	case ViewportDrawMode::LATLON:
		buttonname = (char *) "/ui/MainMenu/View/ModeLatLon";
		break;
	default:
		buttonname = (char *) "/ui/MainMenu/View/ModeUTM";
	}
#ifdef K
	mode_button = gtk_ui_manager_get_widget(this->uim, buttonname);
#endif
	assert(mode_button);
	return mode_button;
}




/**
 * Steps to be taken once initial loading has completed.
 */
void Window::finish_new(void)
{
	/* Don't add a map if we've loaded a Viking file already. */
	if (this->filename) {
		return;
	}

	if (a_vik_get_startup_method() == VIK_STARTUP_METHOD_SPECIFIED_FILE) {
		this->open_file(a_vik_get_startup_file(), true);
		if (this->filename) {
			return;
		}
	}

	/* Maybe add a default map layer. */
	if (a_vik_get_add_default_map_layer()) {
#ifdef K
		LayerMaps * layer = new LayerMaps(this->viewport);
		layer->rename(_("Default Map"));

		this->layers_panel->get_top_layer()->add_layer(layer, true);
#endif

		this->draw_update();
	}

	/* If not loaded any file, maybe try the location lookup. */
	if (this->loaded_type == LOAD_TYPE_READ_FAILURE) {
		if (a_vik_get_startup_method() == VIK_STARTUP_METHOD_AUTO_LOCATION) {

			this->status_bar->set_message(StatusBarField::INFO, _("Trying to determine location..."));
#ifdef K
			a_background_thread(BACKGROUND_POOL_REMOTE,
					    _("Determining location"),
					    (vik_thr_func) determine_location_thread,
					    this,
					    NULL,
					    NULL,
					    1);
#endif
		}
	}
}




void Window::open_window(void)
{
	GSList *files;

	bool change_fn = (g_slist_length(files) == 1); /* Only change fn if one file. */
	GSList *cur_file = files;
	while (cur_file) {
		/* Only open a new window if a viking file. */
		char *file_name = (char *) cur_file->data;
		if (this->filename && check_file_magic_vik(file_name)) {
#ifdef K
			Window * new_window = Window::new_window();
			if (new_window) {
				new_window->open_file(file_name, true);
			}
#endif
		} else {
			this->open_file(file_name, change_fn);
		}
		free(file_name);
		cur_file = g_slist_next(cur_file);
	}
	g_slist_free(files);
}




void Window::show_centers_cb() /* Slot. */
{
	this->viewport->show_centers(this);
}




void Window::help_help_cb(void)
{
#ifdef K
#ifdef WINDOWS
	ShellExecute(NULL, "open", "" PACKAGE".pdf", NULL, NULL, SW_SHOWNORMAL);
#else /* WINDOWS */
	char * uri = g_strdup_printf("ghelp:%s", PACKAGE);
	GError *error = NULL;
	bool show = gtk_show_uri(NULL, uri, GDK_CURRENT_TIME, &error);
	if (!show && !error)
		/* No error to show, so unlikely this will get called. */
		dialog_error("The help system is not available.", window);
	else if (error) {
		/* Main error path. */
		dialog_error(QString("Help is not available because: %1.\nEnsure a Mime Type ghelp handler program is installed (e.g. yelp).").arg(QString(error->message)), window);
		g_error_free(error);
	}
	free(uri);
#endif /* WINDOWS */
#endif
}




void Window::help_about_cb(void) /* Slot. */
{
	a_dialog_about(this);
}
