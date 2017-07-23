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

#include <vector>

#include <cassert>
#include <unistd.h>

#include <QtWidgets>
#include <QFileDialog>

#include "window_layer_tools.h"
#include "window.h"
#include "viewport.h"
#include "viewport_zoom.h"
#include "layer.h"
#include "layer_defaults.h"
#include "layers_panel.h"
#include "layer_toolbox.h"
#include "layer_aggregate.h"
#include "layer_map.h"
#include "globals.h"
#include "ui_builder.h"
#include "settings.h"
#include "background.h"
#include "dialog.h"
#include "util.h"
#include "vikutils.h"
#include "file.h"
#include "fileutils.h"
#include "datasources.h"
#include "goto.h"
#include "print.h"
#include "kmz.h"
#include "external_tools.h"
#include "vikexttool_datasources.h"
#include "preferences.h"




using namespace SlavGPS;




/* This seems rather arbitary, quite large and pointless.
   I mean, if you have a thousand windows open;
   why not be allowed to open a thousand more... */
#define MAX_WINDOWS 1024
static unsigned int window_count = 0;
static std::list<Window *> window_list;

#define VIKING_WINDOW_WIDTH      1000
#define VIKING_WINDOW_HEIGHT     800
#define DRAW_IMAGE_DEFAULT_WIDTH 1280
#define DRAW_IMAGE_DEFAULT_HEIGHT 1024
#define DRAW_IMAGE_DEFAULT_SAVE_AS_PNG true




extern VikDataSourceInterface vik_datasource_gps_interface;
extern VikDataSourceInterface vik_datasource_file_interface;
extern VikDataSourceInterface vik_datasource_routing_interface;
#ifdef VIK_CONFIG_OPENSTREETMAP
extern VikDataSourceInterface vik_datasource_osm_interface;
extern VikDataSourceInterface vik_datasource_osm_my_traces_interface;
#endif
#ifdef VIK_CONFIG_GEOCACHES
extern VikDataSourceInterface vik_datasource_gc_interface;
#endif
#ifdef VIK_CONFIG_GEOTAG
extern VikDataSourceInterface vik_datasource_geotag_interface;
#endif
#ifdef VIK_CONFIG_GEONAMES
extern VikDataSourceInterface vik_datasource_wikipedia_interface;
#endif
extern VikDataSourceInterface vik_datasource_url_interface;
extern VikDataSourceInterface vik_datasource_geojson_interface;




/* The last used directories. */
static QUrl last_folder_files_url;




#define VIK_SETTINGS_WIN_SIDEPANEL "window_sidepanel"
#define VIK_SETTINGS_WIN_STATUSBAR "window_statusbar"
#define VIK_SETTINGS_WIN_TOOLBAR "window_toolbar"
/* Menubar setting to off is never auto saved in case it's accidentally turned off.
   It's not so obvious so to recover the menu visibility.
   Thus this value is for setting manually via editting the settings file directly. */
#define VIK_SETTINGS_WIN_MENUBAR "window_menubar"




static QMenu * create_zoom_submenu(double mpp, QString const & label, QMenu * parent);




Window::Window()
{

	strcpy(this->type_string, "SG QT WINDOW");

	QIcon::setThemeName("default");
	this->create_layout();
	this->create_actions();


	this->layer_toolbox = new LayerToolbox(this);

	this->create_ui();


	/* Own signals. */
	connect(this->viewport, SIGNAL(updated_center(void)), this, SLOT(center_changed_cb(void)));
	connect(this->layers_panel, SIGNAL(update()), this, SLOT(draw_update_cb()));


#if 0
	this->set_filename(NULL);

	this->busy_cursor = gdk_cursor_new(GDK_WATCH);
#endif

	int draw_image_width_;
	if (a_settings_get_integer(VIK_SETTINGS_WIN_SAVE_IMAGE_WIDTH, &draw_image_width_)) {
		this->draw_image_width = draw_image_width_;
	} else {
		this->draw_image_width = DRAW_IMAGE_DEFAULT_WIDTH;
	}
	int draw_image_height_;
	if (a_settings_get_integer(VIK_SETTINGS_WIN_SAVE_IMAGE_HEIGHT, &draw_image_height_)) {
		this->draw_image_height = draw_image_height_;
	} else {
		this->draw_image_height = DRAW_IMAGE_DEFAULT_HEIGHT;
	}
	bool draw_image_save_as_png_;
	if (a_settings_get_boolean(VIK_SETTINGS_WIN_SAVE_IMAGE_PNG, &draw_image_save_as_png_)) {
		this->draw_image_save_as_png = draw_image_save_as_png_;
	} else {
		this->draw_image_save_as_png = DRAW_IMAGE_DEFAULT_SAVE_AS_PNG;
	}



#ifdef K
	QObject::connect(this, SIGNAL("delete_event"), NULL, SLOT (delete_event));


	// Signals from GTK
	QObject::connect(this->viewport, SIGNAL("expose_event"), this, SLOT (draw_sync_cb));
	QObject::connect(this->viewport, SIGNAL("configure_event"), this, SLOT (window_configure_event));
	gtk_widget_add_events(this->viewport, GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK);
	QObject::connect(this->viewport, SIGNAL("scroll_event"), this, SLOT (draw_scroll_cb));
	QObject::connect(this->viewport, SIGNAL("button_press_event"), this, SLOT (draw_click_cb));
	QObject::connect(this->viewport, SIGNAL("button_release_event"), this, SLOT (draw_release_cb));



	QObject::connect(this->layers_panel, SIGNAL("delete_layer"), this, SLOT (vik_window_clear_highlight_cb));

	// Allow key presses to be processed anywhere
	QObject::connect(this, SIGNAL("key_press_event"), this, SLOT (key_press_event_cb));

	// Set initial button sensitivity
	center_changed_cb(this);

	this->hpaned = gtk_hpaned_new();
	gtk_paned_pack1(GTK_PANED(this->hpaned), this->layers_panel, false, true);
	gtk_paned_pack2(GTK_PANED(this->hpaned), this->viewport, true, true);

	/* This packs the button into the window (a gtk container). */
	this->main_vbox->addWidget(this->hpaned);

	gtk_box_pack_end(GTK_BOX(this->main_vbox), GTK_WIDGET(this->viking_vs), false, true, 0);

	a_background_add_window(this);

	window_list.push_front(this);
#endif

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
#ifdef K
				gtk_window_maximize(this);
#endif
			}
		}

		bool full;
		if (a_settings_get_boolean(VIK_SETTINGS_WIN_FULLSCREEN, &full)) {
			if (full) {
#ifdef K
				this->show_full_screen = true;
				gtk_window_fullscreen(this);
				GtkWidget *check_box = gtk_ui_manager_get_widget(this->uim, "/ui/MainMenu/View/FullScreen");
				if (check_box) {
					gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_box), true);
				}
#endif
			}
		}

		int position = -1; // Let GTK determine default positioning
		if (!a_settings_get_integer(VIK_SETTINGS_WIN_PANE_POSITION, &position)) {
			position = -1;
		}
#ifdef K
		gtk_paned_set_position(GTK_PANED(this->hpaned), position);
#endif
	}

#ifdef K
	gtk_window_set_default_size(this, width, height);

	// Only accept Drag and Drop of files onto the viewport
	gtk_drag_dest_set(this->viewport, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
	gtk_drag_dest_add_uri_targets(this->viewport);
	QObject::connect(this->viewport, SIGNAL("drag-data-received"), NULL, SLOT (drag_data_received_cb));

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


	this->viewport = new Viewport(this);
	qDebug() << "II: Window: created Viewport with size:" << this->viewport->height() << this->viewport->width();


	this->setCentralWidget(viewport);


	this->panel_dock = new QDockWidget(tr("Layers"), this);
	this->panel_dock->setAllowedAreas(Qt::TopDockWidgetArea);
	this->layers_panel = new LayersPanel(this->panel_dock, this);
	this->panel_dock->setWidget(this->layers_panel);
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

	QAction * qa = NULL;
	QActionGroup * group = NULL;

	/* "File" menu. */
	QAction * qa_file_new = NULL;
	QAction * qa_file_open = NULL;
	QAction * qa_file_exit = NULL;
	{
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


		this->submenu_file_acquire = this->menu_file->addMenu(QIcon::fromTheme("TODO"), QString("A&cquire"));

		{
			qa = this->submenu_file_acquire->addAction(tr("Import File With GPS&Babel..."));
			qa->setToolTip(tr("Import File With GPS&Babel..."));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_file_cb(void)));

#ifdef VIK_CONFIG_OPENSTREETMAP
			qa = this->submenu_file_acquire->addAction(tr("&OSM Traces..."));
			qa->setToolTip(tr("Get traces from OpenStreetMap"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_osm_cb(void)));


			qa = this->submenu_file_acquire->addAction(tr("&My OSM Traces..."));
			qa->setToolTip(tr("Get Your Own Traces from OpenStreetMap"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_my_osm_cb(void)));
#endif

#ifdef VIK_CONFIG_GEONAMES
			qa = this->submenu_file_acquire->addAction(tr("From &Wikipedia Waypoints"));
			qa->setToolTip(tr("Create waypoints from Wikipedia items in the current view"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_wikipedia_cb(void)));
#endif

			vik_ext_tool_datasources_add_menu_items(this->submenu_file_acquire, this);
		}


		this->menu_file->addSeparator();


		qa = this->menu_file->addAction(tr("&Generate Image File..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (draw_viewport_to_image_file_cb()));
		qa->setToolTip("Save current viewport to image file");

		qa = this->menu_file->addAction(tr("Generate &Directory of Images..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (draw_viewport_to_image_dir_cb()));
		qa->setToolTip("Generate &Directory of Images");

#ifdef HAVE_ZIP_H
		qa = this->menu_file->addAction(tr("Generate &KMZ Map File..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (draw_viewport_to_kmz_file_cb()));
		qa->setToolTip("Generate a KMZ file with an overlay of the current view");
#endif

		qa = this->menu_file->addAction(tr("&Print..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (print_cb()));
		qa->setToolTip("Print");


		this->menu_file->addSeparator();


		qa_file_exit = this->menu_file->addAction(QIcon::fromTheme("application-exit"), _("E&xit"));
		qa_file_exit->setShortcut(Qt::CTRL + Qt::Key_X);
		connect(qa_file_exit, SIGNAL (triggered(bool)), this, SLOT (close(void)));
	}


	/* "Edit" menu. */
	{
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
		this->menu_view->addSeparator();


		group = new QActionGroup(this);

		/* kamilFIXME: select initial value in the group based on... */

		this->qa_drawmode_utm = new QAction(tr("&UTM Mode"), this);
		this->qa_drawmode_utm->setData(QVariant((int) ViewportDrawMode::UTM));
		this->qa_drawmode_utm->setCheckable(true);
		this->qa_drawmode_utm->setChecked(true);
		group->addAction(this->qa_drawmode_utm);
		this->menu_view->addAction(this->qa_drawmode_utm);

#ifdef VIK_CONFIG_EXPEDIA
		this->qa_drawmode_expedia = new QAction(tr("&Expedia Mode"), this);
		this->qa_drawmode_expedia->setData(QVariant((int) ViewportDrawMode::EXPEDIA));
		this->qa_drawmode_expedia->setCheckable(true);
		group->addAction(this->qa_drawmode_expedia);
		this->menu_view->addAction(this->qa_drawmode_expedia);
#endif

		this->qa_drawmode_mercator = new QAction(tr("&Mercator Mode"), this);
		this->qa_drawmode_mercator->setData(QVariant((int) ViewportDrawMode::MERCATOR));
		this->qa_drawmode_mercator->setCheckable(true);
		group->addAction(this->qa_drawmode_mercator);
		this->menu_view->addAction(this->qa_drawmode_mercator);

		this->qa_drawmode_latlon = new QAction(tr("&Lat/Lon Mode"), this);
		this->qa_drawmode_latlon->setData(QVariant((int) ViewportDrawMode::LATLON));
		this->qa_drawmode_latlon->setCheckable(true);
		group->addAction(this->qa_drawmode_latlon);
		this->menu_view->addAction(this->qa_drawmode_latlon);

		connect(group, SIGNAL (triggered(QAction *)), this, SLOT (change_coord_mode_cb(QAction *)));
#if 0
		for (unsigned int i = 0; i < G_N_ELEMENTS (mode_entries); i++) {
			toolbar_action_mode_entry_register(window->viking_vtb, &mode_entries[i]);
		}
#endif


		this->menu_view->addSeparator();


		qa = new QAction(tr("Go to the &Default Location"), this);
		qa->setToolTip("Go to the default location");
		qa->setIcon(QIcon::fromTheme("go-home"));
		connect(qa, SIGNAL(triggered(bool)), this, SLOT(goto_default_location_cb(void)));
		this->menu_view->addAction(qa);

		qa = new QAction(tr("Go to &Location..."), this);
		qa->setToolTip("Go to address/place using text search");
		qa->setIcon(QIcon::fromTheme("go-jump"));
		connect(qa, SIGNAL(triggered(bool)), this, SLOT(goto_location_cb(void)));
		this->menu_view->addAction(qa);

		qa = new QAction(tr("&Go to Lat/Lon..."), this);
		qa->setToolTip("Go to arbitrary lat/lon coordinate");
		qa->setIcon(QIcon::fromTheme("go-jump"));
		connect(qa, SIGNAL(triggered(bool)), this, SLOT(goto_latlon_cb(void)));
		this->menu_view->addAction(qa);

		qa = new QAction(tr("Go to UTM..."), this);
		qa->setToolTip("Go to arbitrary UTM coordinate");
		qa->setIcon(QIcon::fromTheme("go-jump"));
		connect(qa, SIGNAL(triggered(bool)), this, SLOT(goto_utm_cb(void)));
		this->menu_view->addAction(qa);

		qa = new QAction(tr("Go to the Pre&vious Location"), this);
		qa->setToolTip("Go to the previous location");
		qa->setIcon(QIcon::fromTheme("go-previous"));
		connect(qa, SIGNAL(triggered(bool)), this, SLOT(goto_previous_location_cb(void)));
		this->menu_view->addAction(qa);

		qa = new QAction(tr("Go to the &Next Location"), this);
		qa->setToolTip("Go to the next location");
		qa->setIcon(QIcon::fromTheme("go-next"));
		connect(qa, SIGNAL(triggered(bool)), this, SLOT(goto_next_location_cb(void)));
		this->menu_view->addAction(qa);


		this->menu_view->addSeparator();


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


		QMenu * zoom_submenu = create_zoom_submenu(this->viewport->get_zoom(), "Zoom", this->menu_view);
		this->menu_view->addMenu(zoom_submenu);
		connect(zoom_submenu, SIGNAL(triggered(QAction *)), this, SLOT(zoom_level_selected_cb(QAction *)));


		this->menu_view->addSeparator();


		qa = new QAction("Background &Jobs", this);
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
	QString zoom_level;
	const double xmpp = this->viewport->get_xmpp();
	const double ympp = this->viewport->get_ympp();
	const QString unit = this->viewport->get_coord_mode() == CoordMode::UTM ? "mpp" : "pixelfact";
	if (xmpp != ympp) {
		zoom_level = QString(tr("%1/%2f %3"))
			.arg(xmpp, 0, 'f', SG_VIEWPORT_ZOOM_PRECISION)
			.arg(ympp, 0, 'f', SG_VIEWPORT_ZOOM_PRECISION)
			.arg(unit);
	} else {
		if ((int)xmpp - xmpp < 0.0) {
			zoom_level = QString(tr("%1 %2")).arg(xmpp, 0, 'f', SG_VIEWPORT_ZOOM_PRECISION).arg(unit);
		} else {
			/* xmpp should be a whole number so don't show useless .000 bit. */
			zoom_level = QString(tr("%1 %2")).arg((int) xmpp).arg(unit);
		}
	}

	qDebug() << "II: Window: zoom level is" << zoom_level;
	this->status_bar->set_message(StatusBarField::ZOOM, zoom_level);
	this->display_tool_name();
}




void Window::menu_layer_new_cb(void) /* Slot. */
{
	QAction * qa = (QAction *) QObject::sender();
	LayerType layer_type = (LayerType) qa->data().toInt();

	qDebug() << "II: Window: clicked \"layer new\" for layer type" << Layer::get_interface(layer_type)->layer_type_string;

	if (this->layers_panel->new_layer(layer_type)) {
		qDebug() << "II: Window: new layer, call draw_update_cb()" << __FUNCTION__ << __LINE__;
		this->draw_update();
		this->modified = true;
	}

}




void Window::draw_redraw()
{
	Coord old_center = this->trigger_center;
	this->trigger_center = *(this->viewport->get_center());
	Layer * new_trigger = this->trigger;
	this->trigger = NULL;
	Layer * old_trigger = this->viewport->get_trigger();

	if (!new_trigger) {
		; /* Do nothing -- have to redraw everything. */
	} else if ((old_trigger != new_trigger)
		   || (old_center != this->trigger_center)
		   || (new_trigger->type == LayerType::AGGREGATE)) {
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
	for (LayerType i = LayerType::AGGREGATE; i < LayerType::NUM_TYPES; ++i) {

		const LayerInterface * iface = Layer::get_interface(i);

		QVariant variant((int) i);
		QAction * qa = new QAction(iface->ui_labels.new_layer, this);
		qa->setData(variant);
		qa->setIcon(iface->action_icon);
		connect(qa, SIGNAL(triggered(bool)), this, SLOT(menu_layer_new_cb(void)));

		menu->addAction(qa);
	}

	return menu;
}




void Window::create_ui(void)
{
	/* Menu Tools -> Webtools. */
	{
		QActionGroup * group = new QActionGroup(this);
		group->setObjectName("webtools");
		external_tools_add_action_items(group, this);

		QMenu * submenu_webtools = this->menu_tools->addMenu(tr("&Webtools"));
		submenu_webtools->addActions(group->actions());
	}

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
		LayerInterface * interface = NULL;
		for (LayerType i = LayerType::AGGREGATE; i < LayerType::NUM_TYPES; ++i) {

			interface = Layer::get_interface(i);

			if (interface->layer_tool_constructors.empty()) {
				continue;
			}
			this->toolbar->addSeparator();
			this->menu_tools->addSeparator();

			QActionGroup * group = new QActionGroup(this);
			group->setObjectName(interface->layer_name);

			unsigned int j = 0;
			for (j = 0; j < interface->layer_tool_constructors.size(); j++) {

				LayerTool * layer_tool = interface->layer_tool_constructors[j](this, this->viewport);
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
		for (unsigned int j = 0; j < Layer::get_interface(i)->layer_tools.size(); j++) {
			GtkAction * action = gtk_action_group_get_action(action_group,
									 Layer::get_interface(i)->layer_tools[j]->id_string);
			g_object_set(action, "sensitive", false, NULL);
		}
	}



	window->action_group = action_group;

	GtkAccelGroup * accel_group = gtk_ui_manager_get_accel_group(uim);
	gtk_window_add_accel_group(GTK_WINDOW (window), accel_group);
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




void Window::activate_layer_tool(LayerType layer_type, int tool_id)
{
	this->layer_toolbox->deactivate_current_tool();
	this->layer_toolbox->activate_tool(layer_type, tool_id);

	return;
}




void Window::pan_click(QMouseEvent * ev)
{
	qDebug() << "II: Window: pan click";
	/* Set panning origin. */
	this->pan_move_flag = false;
	this->pan_x = ev->x();
	this->pan_y = ev->y();
}




void Window::pan_move(QMouseEvent * ev)
{
	qDebug() << "II: Window: pan move";
	if (this->pan_x != -1) {
		this->viewport->set_center_screen(this->viewport->get_width() / 2 - ev->x() + this->pan_x,
						  this->viewport->get_height() / 2 - ev->y() + this->pan_y);
		this->pan_move_flag = true;
		this->pan_x = ev->x();
		this->pan_y = ev->y();
		this->draw_update();
	}
}




void Window::pan_release(QMouseEvent * ev)
{
	qDebug() << "II: Window: pan release";
	bool do_draw = true;

	if (this->pan_move_flag == false) {
		this->single_click_pending = !this->single_click_pending;
		if (this->single_click_pending) {
#if 0
			// Store offset to use
			this->delayed_pan_x = this->pan_x;
			this->delayed_pan_y = this->pan_y;
			// Get double click time
			GtkSettings *gs = gtk_widget_get_settings(this);
			GValue dct = { 0 }; // = G_VALUE_INIT; // GLIB 2.30+ only
			g_value_init(&dct, G_TYPE_INT);
			g_object_get_property(G_OBJECT(gs), "gtk-double-click-time", &dct);
			// Give chance for a double click to occur
			int timer = g_value_get_int(&dct) + 50;
			g_timeout_add(timer, (GSourceFunc) vik_window_pan_timeout, this);
			do_draw = false;
#endif
		} else {
			this->viewport->set_center_screen(this->pan_x, this->pan_y);
		}
	} else {
		this->viewport->set_center_screen(this->viewport->get_width() / 2 - ev->x() + this->pan_x,
						  this->viewport->get_height() / 2 - ev->y() + this->pan_y);
	}

	this->pan_off();
	if (do_draw) {
		this->draw_update();
	}
}




void Window::pan_off(void)
{
	this->pan_move_flag = false;
	this->pan_x = -1;
	this->pan_y = -1;
}




/**
 * Retrieves window's pan_move_flag.
 *
 * Should be removed as soon as possible.
 *
 * Returns: window's pan_move
 **/
bool Window::get_pan_move(void)
{
	return this->pan_move_flag;
}




void Window::preferences_cb(void) /* Slot. */
{
#if 0
	bool wp_icon_size = Preferences::get_use_large_waypoint_icons();
#endif
	preferences_show_window(this);
#if 0
	// Has the waypoint size setting changed?
	if (wp_icon_size != Preferences::get_use_large_waypoint_icons()) {
		// Delete icon indexing 'cache' and so automatically regenerates with the new setting when changed
		clear_garmin_icon_syms();

		// Update all windows
		for (auto i = window_list.begin(); i != window_list.end(); i++) {
			preferences_change_update(*i);
		}
	}


	// Ensure TZ Lookup initialized
	if (Preferences::get_time_ref_frame() == VIK_TIME_REF_WORLD) {
		vu_setup_lat_lon_tz_lookup();
	}

	toolbar_apply_settings(window->viking_vtb, window->main_vbox, window->menu_hbox, true);
#endif
}




void Window::closeEvent(QCloseEvent * ev)
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
			ev->accept();
		} else if (reply == QMessageBox::Yes) {
			//save_file(NULL, window);
			ev->accept();
		} else {
			ev->ignore();
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

			a_settings_set_boolean(VIK_SETTINGS_WIN_SIDEPANEL, this->view_side_panel);
			a_settings_set_boolean(VIK_SETTINGS_WIN_STATUSBAR, this->view_statusbar);
			a_settings_set_boolean(VIK_SETTINGS_WIN_TOOLBAR, this->view_toolbar);

			/* If supersized - no need to save the enlarged width+height values. */
			if (! (state_fullscreen || state_max)) {
				a_settings_set_integer(VIK_SETTINGS_WIN_WIDTH, this->width());
				a_settings_set_integer(VIK_SETTINGS_WIN_HEIGHT, this->height());
			}
#ifdef K
			a_settings_set_integer(VIK_SETTINGS_WIN_PANE_POSITION, gtk_paned_get_position(GTK_PANED(window->hpaned)));
#endif
		}
#ifdef K
		a_settings_set_integer(VIK_SETTINGS_WIN_SAVE_IMAGE_WIDTH, window->draw_image_width);
		a_settings_set_integer(VIK_SETTINGS_WIN_SAVE_IMAGE_HEIGHT, window->draw_image_height);
		a_settings_set_boolean(VIK_SETTINGS_WIN_SAVE_IMAGE_PNG, window->draw_image_save_as_png);

		char *accel_file_name = g_build_filename(get_viking_dir(), VIKING_ACCELERATOR_KEY_FILE, NULL);
		gtk_accel_map_save(accel_file_name);
		free(accel_file_name);
#endif

	}
}




void Window::goto_default_location_cb(void)
{
	struct LatLon ll;
	ll.lat = Preferences::get_default_lat();
	ll.lon = Preferences::get_default_lon();
	this->viewport->set_center_latlon(&ll, true);
	this->layers_panel->emit_update_cb();
}




void Window::goto_location_cb()
{
	goto_location(this, this->viewport);
	this->layers_panel->emit_update_cb();
}




void Window::goto_latlon_cb(void)
{
	/* TODO: call draw_update() conditionally? */
	goto_latlon(this, this->viewport);
	this->draw_update();
}




void Window::goto_utm_cb(void)
{
	/* TODO: call draw_update() conditionally? */
	goto_utm(this, this->viewport);
	this->draw_update();
}




void Window::goto_previous_location_cb(void)
{
	bool changed = this->viewport->go_back();

	/* Recheck buttons sensitivities, as the center changed signal
	   is not sent on back/forward changes (otherwise we would get
	   stuck in an infinite loop!). */
	this->center_changed_cb();

	if (changed) {
		this->draw_update();
	}
}




void Window::goto_next_location_cb(void)
{
	bool changed = this->viewport->go_forward();

	/* Recheck buttons sensitivities, as the center changed signal
	   is not sent on back/forward changes (otherwise we would get
	   stuck in an infinite loop!). */
	this->center_changed_cb();

	if (changed) {
		this->draw_update();
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




void Window::show_side_panel(bool visible)
{
	this->view_side_panel = visible;
	QAction * qa = this->panel_dock->toggleViewAction();
	qDebug() << "II: Window: setting panel dock visible:" << this->view_side_panel;
	qa->setChecked(visible);
	if (visible) {
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

	if (a_dialog_custom_zoom(&xmpp, &ympp, this)) {
		this->viewport->set_xmpp(xmpp);
		this->viewport->set_ympp(ympp);
		this->draw_update();
	}
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
	LayerType layer_type = (LayerType) qa->data().toInt();

	qDebug() << "II: Window: clicked \"layer defaults\" for layer type" << Layer::get_interface(layer_type)->layer_type_string;

	if (Layer::get_interface(layer_type)->params_count == 0) {
		Dialog::info(tr("This layer type has no configurable properties."), this);
	} else {
		layer_defaults_show_window(layer_type, this);
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
			g_signal_emit(window, window_signals[VW_OPENWINDOW_SIGNAL], 0, gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog)));
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
		Dialog::error(tr("The file you requested could not be opened."), this);
		break;
	case LOAD_TYPE_GPSBABEL_FAILURE:
		Dialog::error(tr("GPSBabel is required to load files of this type or GPSBabel encountered problems."), this);
		break;
	case LOAD_TYPE_GPX_FAILURE:
		Dialog::error(tr("Unable to load malformed GPX file %1").arg(QString(new_filename)), this);
		break;
	case LOAD_TYPE_UNSUPPORTED_FAILURE:
		Dialog::error(tr("Unsupported file type for %1").arg(QString(new_filename)), this);
		break;
	case LOAD_TYPE_VIK_FAILURE_NON_FATAL:
		{
			/* Since we can process .vik files with issues just show a warning in the status bar.
			   Not that a user can do much about it... or tells them what this issue is yet... */
			this->get_statusbar()->set_message(StatusBarField::INFO, QString("WARNING: issues encountered loading %1").arg(file_base_name(new_filename)));
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
			QAction * drawmode_action = this->grepget_drawmode_action(this->viewport->get_drawmode());
			this->only_updating_coord_mode_ui = true; /* if we don't set this, it will change the coord to UTM if we click Lat/Lon. I don't know why. */
			gtk_check_menu_item_set_active(drawmode_action, true);
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

	unsigned int limit = Preferences::get_recent_number_files();

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




void Window::update_recently_used_document(char const * file_name)
{
#ifdef K
	/* Update Recently Used Document framework */
	GtkRecentManager *manager = gtk_recent_manager_get_default();
	GtkRecentData * recent_data = g_slice_new(GtkRecentData);
	char *groups[] = { (char *) "viking", NULL};
	GFile * file = g_file_new_for_commandline_arg(file_name);
	char * uri = g_file_get_uri(file);
	char * basename = g_path_get_basename(file_name);
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
	gdk_window_set_cursor(gtk_widget_get_window(this), NULL);
	// Restore viewport cursor
	gdk_window_set_cursor(gtk_widget_get_window(this->viewport), this->viewport_cursor);
#endif
}




void Window::set_filename(char const * file_name)
{
	if (this->filename) {
		free(this->filename);
		this->filename = NULL;
	}
	if (filename) {
		this->filename = strdup(file_name);
	}

	/* Refresh window's title */
	this->setWindowTitle(QString("%1 - SlavGPS").arg(this->get_filename()));
}




char const * Window::get_filename()
{
	return this->filename ? file_basename(this->filename) : _("Untitled");
}





QAction * Window::get_drawmode_action(ViewportDrawMode mode)
{
	QAction * qa = NULL;
	switch (mode) {
#ifdef VIK_CONFIG_EXPEDIA
	case ViewportDrawMode::EXPEDIA:
		qa = this->qa_drawmode_expedia;
		break;
#endif
	case ViewportDrawMode::MERCATOR:
		qa = this->qa_drawmode_mercator;
		break;

	case ViewportDrawMode::LATLON:
		qa = this->qa_drawmode_latlon;
		break;

	default:
		qa = this->qa_drawmode_utm;
		break;
	}

	assert(qa);
	return qa;
}




static int determine_location_thread(BackgroundJob * bg_job);




class LocatorJob : public BackgroundJob {
public:
	LocatorJob(Window * window_);
	Window * window = NULL;
};




LocatorJob::LocatorJob(Window * window_)
{
	this->thread_fn = determine_location_thread;
	this->n_items = 1; /* There is only one location to determine. */

	this->window = window_;
}




/**
 * @bg_job: Data used by our background thread mechanism
 *
 * Use the features in goto module to determine where we are
 * Then set up the viewport:
 *  1. To goto the location
 *  2. Set an appropriate level zoom for the location type
 *  3. Some statusbar message feedback
 */
int determine_location_thread(BackgroundJob * bg_job)
{
	LocatorJob * locator = (LocatorJob *) bg_job;

	struct LatLon ll;
	char * name = NULL;
	int ans = a_vik_goto_where_am_i(locator->window->viewport, &ll, &name);

	int result = a_background_thread_progress(bg_job, 1.0);
	if (result != 0) {
		locator->window->statusbar_update(StatusBarField::INFO, QString("Location lookup aborted"));
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

		locator->window->viewport->set_zoom(zoom);
		locator->window->viewport->set_center_latlon(&ll, false);

		locator->window->statusbar_update(StatusBarField::INFO, QString("Location found: %1").arg(name));
		free(name);

		// Signal to redraw from the background
		locator->window->layers_panel->emit_update_cb();
	} else {
		locator->window->statusbar_update(StatusBarField::INFO, QString("Unable to determine location"));
	}

	return 0;
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

	if (Preferences::get_startup_method() == VIK_STARTUP_METHOD_SPECIFIED_FILE) {
		this->open_file(Preferences::get_startup_file(), true);
		if (this->filename) {
			return;
		}
	}

	/* Maybe add a default map layer. */
	if (Preferences::get_add_default_map_layer()) {
		LayerMap * layer = new LayerMap();
		layer->rename(QObject::tr("Default Map"));

		this->layers_panel->get_top_layer()->add_layer(layer, true);

		this->draw_update();
	}

	/* If not loaded any file, maybe try the location lookup. */
	if (this->loaded_type == LOAD_TYPE_READ_FAILURE) {
		if (Preferences::get_startup_method() == VIK_STARTUP_METHOD_AUTO_LOCATION) {

			this->status_bar->set_message(StatusBarField::INFO, _("Trying to determine location..."));
			LocatorJob * locator = new LocatorJob(this);
			a_background_thread(locator, ThreadPoolType::REMOTE, tr("Determining location"));
		}
	}
}




void Window::open_window(void)
{
	GSList *files = NULL;

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
		Dialog::error(tr("The help system is not available."), window);
	else if (error) {
		/* Main error path. */
		Dialog::error(tr("Help is not available because: %1.\nEnsure a Mime Type ghelp handler program is installed (e.g. yelp).").arg(QString(error->message)), window);
		g_error_free(error);
	}
	free(uri);
#endif /* WINDOWS */
#endif
}




void Window::help_about_cb(void) /* Slot. */
{
	Dialog::about(this);
}




void Window::my_acquire(VikDataSourceInterface * datasource)
{
	DatasourceMode mode = datasource->mode;
	if (mode == DatasourceMode::AUTO_LAYER_MANAGEMENT) {
		mode = DatasourceMode::CREATENEWLAYER;
	}
	a_acquire(this, this->layers_panel, this->viewport, mode, datasource, NULL, NULL);
}




void Window::acquire_from_gps_cb(void)
{
	this->my_acquire(&vik_datasource_gps_interface);
}




void Window::acquire_from_file_cb(void)
{
	this->my_acquire(&vik_datasource_file_interface);
}




void Window::acquire_from_geojson_cb(void)
{
	this->my_acquire(&vik_datasource_geojson_interface);
}




void Window::acquire_from_routing_cb(void)
{
	this->my_acquire(&vik_datasource_routing_interface);
}




#ifdef VIK_CONFIG_OPENSTREETMAP
void Window::acquire_from_osm_cb(void)
{
	this->my_acquire(&vik_datasource_osm_interface);
}




void Window::acquire_from_my_osm_cb(void)
{
	this->my_acquire(&vik_datasource_osm_my_traces_interface);
}
#endif




#ifdef VIK_CONFIG_GEOCACHES
void Window::acquire_from_gc_cb(void)
{
	this->my_acquire(&vik_datasource_gc_interface);
}
#endif




#ifdef VIK_CONFIG_GEOTAG
void Window::acquire_from_geotag_cb(void)
{
	this->my_acquire(&vik_datasource_geotag_interface);
}
#endif




#ifdef VIK_CONFIG_GEONAMES
void Window::acquire_from_wikipedia_cb(void)
{
	this->my_acquire(&vik_datasource_wikipedia_interface);
}
#endif




void Window::acquire_from_url_cb(void)
{
	this->my_acquire(&vik_datasource_url_interface);
}




void Window::draw_viewport_to_image_file_cb(void)
{
	this->draw_viewport_to_image_file(ViewportToImageMode::SINGLE_IMAGE);
}




void Window::draw_viewport_to_image_dir_cb(void)
{
	this->draw_viewport_to_image_file(ViewportToImageMode::DIRECTORY_OF_IMAGES);
}




#ifdef HAVE_ZIP_H
void Window::draw_viewport_to_kmz_file_cb(void)
{
	if (this->viewport->get_coord_mode() == CoordMode::UTM) {
		Dialog::error(tr("This feature is not available in UTM mode"));
		return;
	}

	/* ATM This only generates a KMZ file with the current
	   viewport image - intended mostly for map images [but will
	   include any lines/icons from track & waypoints that are
	   drawn] (it does *not* include a full KML dump of every
	   track, waypoint etc...). */
	this->draw_viewport_to_image_file(ViewportToImageMode::KMZ_FILE);
}
#endif




void Window::print_cb(void)
{
	a_print(this, this->viewport);
}




void Window::save_image_file(const QString & file_path, unsigned int w, unsigned int h, double zoom, bool save_as_png, bool save_kmz)
{
	double old_xmpp;
	double old_ympp;

	/* more efficient way: stuff draws directly to pixbuf (fork viewport) */
	QPixmap *pixmap_to_save;

#ifdef K

	GtkWidget * msgbox = gtk_message_dialog_new(this,
						    (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
						    GTK_MESSAGE_INFO,
						    GTK_BUTTONS_NONE,
						    _("Generating image file..."));

	QObject::connect(msgbox, SIGNAL("response"), msgbox, SLOT (gtk_widget_destroy));
	// Ensure dialog shown
	gtk_widget_show_all(msgbox);
#endif
	/* Try harder... */
	this->status_bar->set_message(StatusBarField::INFO, QString("Generating image file..."));
#ifdef K
	while (gtk_events_pending()) {
		gtk_main_iteration();
	}
	// Despite many efforts & variations, GTK on my Linux system doesn't show the actual msgbox contents :(
	// At least the empty box can give a clue something's going on + the statusbar msg...
	// Windows version under Wine OK!
#endif

	/* backup old zoom & set new */
	old_xmpp = this->viewport->get_xmpp();
	old_ympp = this->viewport->get_ympp();
	this->viewport->set_zoom(zoom);

	/* reset width and height: */
	this->viewport->configure_manually(w, h);

	/* draw all layers */
	this->draw_redraw();

	/* Save buffer as file. */
	QPixmap * pixmap = this->viewport->get_pixmap();
	//pixmap_to_save = gdk_pixbuf_get_from_drawable(NULL, GDK_DRAWABLE(this->viewport->get_pixmap()), NULL, 0, 0, 0, 0, w, h);

	if (!pixmap) {
		fprintf(stderr, "EE: Viewport: Failed to generate internal pixmap size: %d x %d\n", w, h);

		this->status_bar->set_message(StatusBarField::INFO, QString(""));
		Dialog::error(tr("Failed to generate internal image.\n\nTry creating a smaller image."), this);

		this->viewport->set_xmpp(old_xmpp);
		this->viewport->set_ympp(old_ympp);
		this->viewport->configure();
		this->draw_update();

		return;
	}

	bool success = true;

	if (save_kmz) {
		double north, east, south, west;
		this->viewport->get_min_max_lat_lon(&south, &north, &west, &east);
#ifdef K
		ans = kmz_save_file(pixmap_to_save, file_path, north, east, south, west);
#endif
	} else {
		qDebug() << "II: Viewport: Save to Image: Saving pixmap";
		if (!pixmap->save(file_path, save_as_png ? "png" : "jpeg")) {
			qDebug() << "WW: Viewport: Save to Image: Unable to write to file" << file_path;
			success = false;
		}
	}

#ifdef K
	g_object_unref(G_OBJECT(pixmap));
#endif

	QString message;
	if (success) {
		message = tr("Image file generated.");
	} else {
		message = tr("Failed to generate image file.");
	}

	/* Cleanup. */

	this->status_bar->set_message(StatusBarField::INFO, QString(""));
	Dialog::info(message, this);

	this->viewport->set_xmpp(old_xmpp);
	this->viewport->set_ympp(old_ympp);
	this->viewport->configure();
	this->draw_update();
}




void Window::save_image_dir(const QString & file_path, unsigned int w, unsigned int h, double zoom, bool save_as_png, unsigned int tiles_w, unsigned int tiles_h)
{
	unsigned long a_size = sizeof(char) * (file_path.length() + 15);
	char *name_of_file = (char *) malloc(a_size);
	struct UTM utm;

	/* *** copied from above *** */


	/* backup old zoom & set new */
	double old_xmpp = this->viewport->get_xmpp();
	double old_ympp = this->viewport->get_ympp();
	this->viewport->set_zoom(zoom);

	/* reset width and height: do this only once for all images (same size) */
	this->viewport->configure_manually(w, h);
	/* *** end copy from above *** */

	assert (this->viewport->get_coord_mode() == CoordMode::UTM);
#ifdef K
	if (g_mkdir(file_path.toUtf8.constData(), 0777) != 0) {
		qDebug() << "WW: Window: Save Viewport to Image: failed to create directory" << file_path;
	}

	struct UTM utm_orig = this->viewport->get_center()->utm;

	for (unsigned int y = 1; y <= tiles_h; y++) {
		for (unsigned int x = 1; x <= tiles_w; x++) {
			snprintf(name_of_file, a_size, "%s%cy%d-x%d.%s", file_path, G_DIR_SEPARATOR, y, x, save_as_png ? "png" : "jpg");
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
			QPixmap * pixmap_to_save = gdk_pixbuf_get_from_drawable(NULL, GDK_DRAWABLE (this->viewport->get_pixmap()), NULL, 0, 0, 0, 0, w, h);
			if (!pixmap_to_save->save(name_of_file, save_as_png ? "png" : "jpeg")) {
				this->status_bar->set_message(StatusBarField::INFO, QString("Unable to write to file %1").arg(name_of_file));
			}

			g_object_unref(G_OBJECT(pixmap_to_save));
		}
	}

	this->viewport->set_center_utm(&utm_orig, false);
	this->viewport->set_xmpp(old_xmpp);
	this->viewport->set_ympp(old_ympp);
	this->viewport->configure();
	this->draw_update();

	free(name_of_file);
#endif
}




/*
 * Get an allocated filename (or directory as specified)
 */
QString Window::draw_viewport_full_path(ViewportToImageMode mode)
{
	QString result;
	if (mode == ViewportToImageMode::DIRECTORY_OF_IMAGES) {

		/* A directory.
		   For some reason this method is only written to work in UTM... */
		if (this->viewport->get_coord_mode() != CoordMode::UTM) {
			Dialog::error(tr("You must be in UTM mode to use this feature"), this);
			return result;
		}

		QFileDialog dialog(this, tr("Choose a directory to hold images"));
		dialog.setFileMode(QFileDialog::Directory);
		dialog.setOption(QFileDialog::ShowDirsOnly);
		dialog.setAcceptMode(QFileDialog::AcceptSave);

		if (last_folder_images_url.toString().size()) {
			dialog.setDirectoryUrl(last_folder_images_url);
		}

		if (QDialog::Accepted == dialog.exec()) {
			last_folder_images_url = dialog.directoryUrl();
			qDebug() << "II: Viewport: Save to Directory of Images: last directory saved as:" << last_folder_images_url;

			result = dialog.selectedFiles().at(0);
			qDebug() << "II: Viewport: Save to Directory of Images: target directory:" << result;
		}
	} else {
		QFileDialog dialog(this, tr("Save Image"));
		dialog.setFileMode(QFileDialog::AnyFile); /* Specify new or select existing file. */
		dialog.setAcceptMode(QFileDialog::AcceptSave);

		QStringList mime;
		mime << "application/octet-stream"; /* "All files (*)" */
		if (mode == ViewportToImageMode::KMZ_FILE) {
			mime << "vnd.google-earth.kmz"; /* "KMZ" / "*.kmz"; */
		} else {
			if (!this->draw_image_save_as_png) {
				mime << "image/jpeg";
			}
			if (this->draw_image_save_as_png) {
				mime << "image/png";
			}
		}
		dialog.setMimeTypeFilters(mime);

		if (last_folder_images_url.toString().size()) {
			dialog.setDirectoryUrl(last_folder_images_url);
		}


		if (QDialog::Accepted == dialog.exec()) {
			last_folder_images_url = dialog.directoryUrl();
			qDebug() << "II: Viewport: Save to Image: last directory saved as:" << last_folder_images_url;

			result = dialog.selectedFiles().at(0);
			qDebug() << "II: Viewport: Save to Image: target file:" << result;

			if (0 == access(result.toUtf8().constData(), F_OK)) {
				if (!Dialog::yes_or_no(tr("The file \"%1\" exists, do you wish to overwrite it?").arg(file_base_name(result)), this, "")) {
					result.resize(0);
				}
			}
		}
	}
	qDebug() << "DD: Viewport: Save to Image: result:" << result << "strdup:" << strdup(result.toUtf8().constData());
	if (result.length()) {
		return strdup(result.toUtf8().constData());
	} else {
		qDebug() << "DD: Viewport: Save to Image: returning NULL\n";
		return NULL;
	}
}




void Window::draw_viewport_to_image_file(ViewportToImageMode mode)
{
	ViewportToImageDialog dialog(tr("Save to Image File"), this->get_viewport(), NULL);
	dialog.build_ui(mode);
	if (QDialog::Accepted != dialog.exec()) {
		return;
	}

	QString full_path = this->draw_viewport_full_path(mode);
	if (!full_path.size()) {
		return;
	}

	int active_z = dialog.zoom_combo->currentIndex();
	double zoom = pow(2, active_z - 2);
	qDebug() << "II: Viewport: Save: zoom index:" << active_z << ", zoom value:" << zoom;

	if (mode == ViewportToImageMode::SINGLE_IMAGE) {
		this->save_image_file(full_path,
				      this->draw_image_width = dialog.width_spin->value(),
				      this->draw_image_height = dialog.height_spin->value(),
				      zoom,
				      this->draw_image_save_as_png, // = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(png_radio)), // kamilTODO
				      false);
	} else if (mode == ViewportToImageMode::KMZ_FILE) {

		/* Remove some viewport overlays as these aren't useful in KMZ file. */
		bool restore_xhair = this->viewport->get_draw_centermark();
		if (restore_xhair) {
			this->viewport->set_draw_centermark(false);
		}
		bool restore_scale = this->viewport->get_draw_scale();
		if (restore_scale) {
			this->viewport->set_draw_scale(false);
		}

		this->save_image_file(full_path,
				      dialog.width_spin->value(),
				      dialog.height_spin->value(),
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
		/* UTM mode ATM. */
		this->save_image_dir(full_path,
				     this->draw_image_width = dialog.width_spin->value(),
				     this->draw_image_height = dialog.height_spin->value(),
				     zoom,
				     this->draw_image_save_as_png, // = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(png_radio)), // kamilTODO
				     dialog.tiles_width_spin->value(),
				     dialog.tiles_height_spin->value());
	}
}





/* Menu View -> Zoom -> Value. */
void Window::zoom_level_selected_cb(QAction * qa) /* Slot. */
{
	int level = qa->data().toInt();
	qDebug() << "SLOT: Window: 'Zoom Changed' callback" << qa->text() << level;

	double zoom_request = pow(2, level - 5);

	/* But has it really changed? */
	double current_zoom = this->viewport->get_zoom();
	if (current_zoom != 0.0 && zoom_request != current_zoom) {
		this->viewport->set_zoom(zoom_request);
		/* Force drawing update. */
		this->draw_update();
	}
}



std::vector<QAction *> zoom_actions;
const char * zoom_action_labels[] = {
	"0.031",   /* 0 */
	"0.063",   /* 1 */
	"0.125",   /* 2 */
	"0.25",    /* 3 */
	"0.5",     /* 4 */
	"1",       /* 5 */
	"2",
	"4",
	"8",
	"16",
	"32",
	"64",
	"128",
	"256",
	"512",
	"1024",
	"2048",
	"4096",
	"8192",
	"16384",
	"32768",
	NULL
};



bool create_zoom_actions(void)
{
	int i = 0;
	QAction * qa = NULL;
	while (zoom_action_labels[i]) {
		QString const label(zoom_action_labels[i]);
		qa = new QAction(label, NULL);
		qa->setData(i);
		zoom_actions.push_back(qa);
		i++;
	}

	return true;
}




/**
 * @mpp: The initial zoom level.
 */
static QMenu * create_zoom_submenu(double mpp, QString const & label, QMenu * parent)
{
	QMenu * menu = NULL;
	if (parent) {
		qDebug() << "II: Window: Zoom: creating zoom menu with parent...";
		menu = parent->addMenu(label);
		qDebug() << "II: Window: Zoom:       ... success";
	} else {
		qDebug() << "II: Window: Zoom: creating zoom menu without parent...";
		menu = new QMenu(label);
		qDebug() << "II: Window: Zoom:       ... success";
	}

	if (!zoom_actions.size()) {
		create_zoom_actions();
	}

	for (auto iter = zoom_actions.begin(); iter != zoom_actions.end(); iter++) {
		menu->addAction(*iter);
	}

#ifdef K
	for (int i = 0 ; i < G_N_ELEMENTS(itemLabels) ; i++) {
		QAction * action = QAction(QString(itemLabels[i]), this);
		menu->addAction(action);
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
#endif

	return menu;
}




QComboBox * SlavGPS::create_zoom_combo_all_levels(QWidget * parent)
{
	QComboBox * combo = new QComboBox(parent);

	int i = 0;
	while (zoom_action_labels[i]) {
		combo->addItem(zoom_action_labels[i]);
		i++;
	}

	combo->setToolTip(QObject::tr("Select zoom level"));

	return combo;
}




static int zoom_popup_handler(GtkWidget * widget)
{
#if 0
	if (!widget) {
		return false;
	}

	if (!GTK_IS_MENU (widget)) {
		return false;
	}

	/* The "widget" is the menu that was supplied when
	 * QObject::connect() was called.
	 */
	QMenu * menu = GTK_MENU (widget);

	menu->exec(QCursor::pos());
#endif
	return true;
}




/* Really a misnomer: changes coord mode (actual coordinates) AND/OR draw mode (viewport only). */
void Window::change_coord_mode_cb(QAction * qa)
{
	ViewportDrawMode drawmode = (ViewportDrawMode) qa->data().toInt();

	qDebug() << "DD: Window: Coordinate mode changed to" << qa->text() << (int) drawmode;

	/* kamilTODO: verify that this function changes mode in all the places that need to be updated. */

	if (this->only_updating_coord_mode_ui) {
		return;
	}

	ViewportDrawMode olddrawmode = this->viewport->get_drawmode();
	if (olddrawmode != drawmode) {
		/* this takes care of coord mode too */
		this->viewport->set_drawmode(drawmode);
		if (drawmode == ViewportDrawMode::UTM) {
			this->layers_panel->change_coord_mode(CoordMode::UTM);
		} else if (olddrawmode == ViewportDrawMode::UTM) {
			this->layers_panel->change_coord_mode(CoordMode::LATLON);
		}
		this->draw_update();
	}
}
