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
 */

#include <vector>

#include <cassert>
#include <unistd.h>

/* stat() */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <QtWidgets>
#include <QFileDialog>

#include "generic_tools.h"
#include "window.h"
#include "viewport.h"
#include "viewport_zoom.h"
#include "layer.h"
#include "layer_defaults.h"
#include "layers_panel.h"
#include "toolbox.h"
#include "layer_trw.h"
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
#include "clipboard.h"
#include "map_cache.h"
#include "tree_view_internal.h"
#include "measurements.h"




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




Tree * g_tree = NULL;




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



enum WindowPan {
	PAN_NORTH = 0,
	PAN_EAST,
	PAN_SOUTH,
	PAN_WEST
};




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


	for (int i = 0; i < QIcon::themeSearchPaths().size(); i++) {
		qDebug() << "II: Window: XDG DATA FOLDER: " << QIcon::themeSearchPaths().at(i);
	}
	QIcon::setThemeName("default");
	qDebug() << "II: Window: Using icon theme " << QIcon::themeName();


	this->create_layout();
	this->create_actions();


	this->toolbox = new Toolbox(this);

	this->create_ui();


	/* Own signals. */
	connect(this->viewport, SIGNAL(updated_center(void)), this, SLOT(center_changed_cb(void)));
	connect(this->items_tree, SIGNAL(update_window()), this, SLOT(draw_update_cb()));

	g_tree = new Tree();
	g_tree->tree_view = this->get_items_tree()->get_tree_view();
	g_tree->window = this;
	g_tree->items_tree = this->items_tree;
	g_tree->viewport = this->viewport;

	connect(g_tree, SIGNAL(update_window()), this, SLOT(draw_update_cb()));



	this->set_current_document_full_path("");

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
	bool save_viewport_as_png_;
	if (a_settings_get_boolean(VIK_SETTINGS_WIN_SAVE_IMAGE_PNG, &save_viewport_as_png_)) {
		this->save_viewport_as_png = save_viewport_as_png_;
	} else {
		this->save_viewport_as_png = DRAW_IMAGE_DEFAULT_SAVE_AS_PNG;
	}



#ifdef K
	QObject::connect(this, SIGNAL("delete_event"), NULL, SLOT (delete_event));


	// Signals from GTK
	QObject::connect(this->viewport, SIGNAL("expose_event"), this, SLOT (draw_sync_cb));
	QObject::connect(this->viewport, SIGNAL("configure_event"), this, SLOT (window_configure_event));
	gtk_widget_add_events(this->viewport, GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK);

#ifdef K
	/* This signal appears to be already handled by Viewport::wheelEvent(). */
	QObject::connect(this->viewport, SIGNAL("scroll_event"), this, SLOT (draw_scroll_cb));
#endif

	QObject::connect(this->viewport, SIGNAL("button_press_event"), this, SLOT (draw_click_cb(QMouseEvent *)));
	QObject::connect(this->viewport, SIGNAL("button_release_event"), this, SLOT (draw_release_cb(QMouseEvent *));



	QObject::connect(this->layers_tree, SIGNAL("delete_layer"), this, SLOT (vik_window_clear_highlight_cb));

	// Allow key presses to be processed anywhere
	QObject::connect(this, SIGNAL("key_press_event"), this, SLOT (key_press_event_cb));

	/* Set initial button sensitivity. */
	this->center_changed_cb();

	this->hpaned = gtk_hpaned_new();
	gtk_paned_pack1(GTK_PANED(this->hpaned), this->layers_tree, false, true);
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

	const QString accel_file_full_path = get_viking_dir() + QDir::separator() + VIKING_ACCELERATOR_KEY_FILE;
	gtk_accel_map_load(accel_file_full_path.toUtf8().constData());
#endif
}




Window::~Window()
{
#ifdef K
	a_background_remove_window(this);

	window_list.remove(this);

	delete this->tb;

	vik_toolbar_finalize(this->viking_vtb);

	delete this->viewport;
	delete this->layers_tree;
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

	this->items_tree = new LayersPanel(this->panel_dock, this);

	this->panel_dock->setWidget(this->items_tree);
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
		qa_file_new = this->menu_file->addAction(QIcon::fromTheme("document-new"), tr("&New file..."));
		qa_file_new->setShortcut(Qt::CTRL + Qt::Key_N);
		qa_file_new->setIcon(QIcon::fromTheme("document-new"));
		qa_file_new->setToolTip("Open a file");
		connect(qa_file_open, SIGNAL (triggered(bool)), this, SLOT (new_window_cb()));

		qa_file_open = this->menu_file->addAction(QIcon::fromTheme("document-open"), tr("&Open..."));
		qa_file_open->setShortcut(Qt::CTRL + Qt::Key_O);
		qa_file_open->setData(QVariant((int) 12)); /* kamilFIXME: magic number. */
		connect(qa_file_open, SIGNAL (triggered(bool)), this, SLOT (open_file_cb()));

		/* This submenu will be populated by Window::update_recent_files(). */
		this->submenu_recent_files = this->menu_file->addMenu(tr("Open &Recent File"));

		qa = this->menu_file->addAction(QIcon::fromTheme("list-add"), tr("Append &File..."));
		qa->setData(QVariant((int) 21)); /* kamilFIXME: magic number. */
		qa->setToolTip("Append data from a different file");
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (open_file_cb()));

		qa = this->menu_file->addAction(tr("&Save"));
		qa->setIcon(QIcon::fromTheme("document-save"));
		qa->setToolTip("Save the file");
		qa->setShortcut(Qt::CTRL + Qt::Key_S);
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_file_save_cb()));

		qa = this->menu_file->addAction(tr("Save &As..."));
		qa->setIcon(QIcon::fromTheme("document-save-as"));
		qa->setToolTip("Save the file under different name");
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_file_save_as_cb()));

		qa = this->menu_file->addAction(tr("Properties..."));
		qa->setToolTip("Properties of current file");
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_file_properties_cb()));


		this->menu_file->addSeparator();


		this->submenu_file_acquire = this->menu_file->addMenu(QIcon::fromTheme("TODO"), QString("A&cquire"));

		{
			qa = this->submenu_file_acquire->addAction(tr("From &GPS..."));
			qa->setToolTip(tr("Transfer data from a GPS device"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_gps_cb(void)));

			qa = this->submenu_file_acquire->addAction(tr("Import File With GPS&Babel..."));
			qa->setToolTip(tr("Import File With GPS&Babel..."));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_file_cb(void)));

			qa = this->submenu_file_acquire->addAction(tr("&Directions..."));
			qa->setToolTip(tr("Get driving directions"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_routing_cb(void)));

			qa = this->submenu_file_acquire->addAction(tr("Import Geo&JSON File..."));
			qa->setToolTip(tr("Import GeoJSON file"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_geojson_cb(void)));

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


#ifdef VIK_CONFIG_GEOCACHES
			qa = this->submenu_file_acquire->addAction(tr("Geo&caches..."));
			qa->setToolTip(tr("Get Geocaches from geocaching.com"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_gc_cb(void)));
#endif

#ifdef VIK_CONFIG_GEOTAG
			qa = this->submenu_file_acquire->addAction(tr("From Geotagged &Images..."));
			qa->setToolTip(tr("Create waypoints from geotagged images"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_geotag_cb(void)));
#endif

			qa = this->submenu_file_acquire->addAction(tr("From &URL..."));
			qa->setToolTip(tr("Get a file from URL"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_url_cb(void)));

#ifdef HAVE_ZIP_H
			qa = this->submenu_file_acquire->addAction(QIcon::fromTheme("TODO-GTK_STOCK_CONVERT"), tr("Import KMZ &Map File..."));
			qa->setToolTip(tr("Import a KMZ file"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_kmz_file_cb(void)));
#endif

			vik_ext_tool_datasources_add_menu_items(this->submenu_file_acquire, this);
		}


		QMenu * submenu_file_export = this->menu_file->addMenu(QIcon::fromTheme("TODO GTK_STOCK_CONVERT"), tr("&Export All"));
		submenu_file_export->setToolTip(tr("Export All TrackWaypoint Layers"));

		{
			qa = submenu_file_export->addAction(tr("&GPX..."));
			qa->setToolTip(tr("Export as GPX"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (export_to_gpx_cb(void)));

			qa = submenu_file_export->addAction(tr("&KML..."));
			qa->setToolTip(tr("Export as KML"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (export_to_kml_cb(void)));
		}


		this->menu_file->addSeparator();


		qa = this->menu_file->addAction(tr("&Generate Image File..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (draw_viewport_to_image_file_cb()));
		qa->setToolTip(tr("Save current viewport to image file"));

		qa = this->menu_file->addAction(tr("Generate &Directory of Images..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (draw_viewport_to_image_dir_cb()));
		qa->setToolTip(tr("Generate Directory of Images"));

#ifdef HAVE_ZIP_H
		qa = this->menu_file->addAction(tr("Generate &KMZ Map File..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (draw_viewport_to_kmz_file_cb()));
		qa->setToolTip(tr("Generate a KMZ file with an overlay of the current view"));
#endif

		qa = this->menu_file->addAction(tr("&Print..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (print_cb()));
		qa->setToolTip("Print");


		this->menu_file->addSeparator();


		qa = this->menu_file->addAction(QIcon::fromTheme("application-exit"), tr("Save and Exit"));
		qa->setShortcut(Qt::CTRL + Qt::Key_Q);
		qa->setToolTip(tr("Save and Exit the program"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_file_save_and_exit_cb(void)));

		qa_file_exit = this->menu_file->addAction(QIcon::fromTheme("application-exit"), tr("E&xit"));
		qa_file_exit->setShortcut(Qt::CTRL + Qt::Key_W);
		qa_file_exit->setToolTip(tr("Exit the program"));
		connect(qa_file_exit, SIGNAL (triggered(bool)), this, SLOT (close(void)));
	}


	/* "Edit" menu. */
	{
		qa = new QAction(tr("Cu&t"), this);
		qa->setIcon(QIcon::fromTheme("edit-cut"));
		qa->setToolTip(tr("Cut selected layer"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_edit_cut_cb(void)));
		this->menu_edit->addAction(qa);

		qa = new QAction(tr("&Copy"), this);
		qa->setIcon(QIcon::fromTheme("edit-copy"));
		qa->setToolTip(tr("Copy selected layer"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_edit_copy_cb(void)));
		this->menu_edit->addAction(qa);

		qa = new QAction(tr("&Paste"), this);
		qa->setIcon(QIcon::fromTheme("edit-paste"));
		qa->setToolTip(tr("Paste layer into selected container layer or otherwise above selected layer"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_edit_paste_cb(void)));
		this->menu_edit->addAction(qa);

		qa = new QAction(tr("&Delete"), this);
		qa->setIcon(QIcon::fromTheme("edit-delete"));
		qa->setToolTip(tr("Remove selected layer"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_edit_delete_cb(void)));
		this->menu_edit->addAction(qa);

		qa = new QAction(tr("Delete All"), this);
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_edit_delete_all_cb(void)));
		this->menu_edit->addAction(qa);


		this->menu_edit->addSeparator();


		qa = new QAction("Copy Centre &Location", this);
		qa->setIcon(QIcon::fromTheme(""));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_copy_centre_cb(void)));
		qa->setShortcut(Qt::CTRL + Qt::Key_H);
		this->menu_edit->addAction(qa);

		qa = new QAction("&Flush Map Cache", this);
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (map_cache_flush_cb(void)));
		this->menu_edit->addAction(qa);

		qa = new QAction("&Set the Default Location", this);
		qa->setIcon(QIcon::fromTheme("go-next"));
		qa->setToolTip(tr("Set the Default Location to the current position"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (set_default_location_cb(void)));
		this->menu_edit->addAction(qa);

		qa = new QAction("&Preferences", this);
		qa->setIcon(QIcon::fromTheme("preferences-other"));
		qa->setToolTip(tr("Program Preferences"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (preferences_cb(void)));
		this->menu_edit->addAction(qa);

		{
			QMenu * defaults_submenu = this->menu_edit->addMenu(QIcon::fromTheme("document-properties"), QString("&Layer Defaults"));

			for (LayerType type = LayerType::AGGREGATE; type < LayerType::NUM_TYPES; ++type) {
				qa = defaults_submenu->addAction("&" + Layer::get_type_ui_label(type) + "...");
				qa->setData(QVariant((int) type));
				qa->setIcon(Layer::get_interface(type)->action_icon);
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


		qa = new QAction(tr("&Refresh"), this);
		qa->setShortcut(Qt::Key_F5);
		qa->setToolTip("Refresh any maps displayed");
		qa->setIcon(QIcon::fromTheme("view-refresh"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_view_refresh_cb(void)));
		this->menu_view->addAction(qa);


		this->menu_view->addSeparator();


		qa = new QAction(tr("Set &Highlight Color..."), this);
		qa->setToolTip("Set Highlight Color");
		qa->setIcon(QIcon::fromTheme("TODO GTK_STOCK_SELECT_COLOR"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_view_set_highlight_color_cb(void)));
		this->menu_view->addAction(qa);

		qa = new QAction(tr("Set Bac&kground Color..."), this);
		qa->setToolTip("Set Background Color");
		qa->setIcon(QIcon::fromTheme("TODO GTK_STOCK_SELECT_COLOR"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_view_set_bg_color_cb(void)));
		this->menu_view->addAction(qa);

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

		this->qa_view_show_draw_with_highlight = new QAction("Use &Highlighting", this);
		this->qa_view_show_draw_with_highlight->setShortcut(Qt::Key_F7);
		this->qa_view_show_draw_with_highlight->setCheckable(true);
		this->qa_view_show_draw_with_highlight->setChecked(this->draw_with_highlight);
		this->qa_view_show_draw_with_highlight->setToolTip("Use highlighting when drawing selected items");
		connect(this->qa_view_show_draw_with_highlight, SIGNAL(triggered(bool)), this, SLOT(draw_with_highlight_cb(bool)));
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
		show_submenu->addAction(this->qa_view_show_draw_with_highlight);
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


		QMenu * zoom_submenu = create_zoom_submenu(this->viewport->get_zoom(), tr("&Zoom"), this->menu_view);
		this->menu_view->addMenu(zoom_submenu);
		connect(zoom_submenu, SIGNAL(triggered (QAction *)), this, SLOT (zoom_level_selected_cb(QAction *)));


		/* "Pan" submenu. */
		{
			QMenu * pan_submenu = this->menu_view->addMenu(tr("&Pan"));

			qa = new QAction(tr("Pan &North"), this);
			qa->setData(PAN_NORTH);
			qa_view_zoom_to->setShortcut(Qt::CTRL + Qt::Key_Up);
			pan_submenu->addAction(qa);
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_view_pan_cb(void)));

			qa = new QAction(tr("Pan &East"), this);
			qa->setData(PAN_EAST);
			qa_view_zoom_to->setShortcut(Qt::CTRL + Qt::Key_Right);
			pan_submenu->addAction(qa);
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_view_pan_cb(void)));

			qa = new QAction(tr("Pan &South"), this);
			qa->setData(PAN_SOUTH);
			qa_view_zoom_to->setShortcut(Qt::CTRL + Qt::Key_Down);
			pan_submenu->addAction(qa);
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_view_pan_cb(void)));

			qa = new QAction(tr("Pan &West"), this);
			qa->setData(PAN_WEST);
			qa_view_zoom_to->setShortcut(Qt::CTRL + Qt::Key_Left);
			pan_submenu->addAction(qa);
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_view_pan_cb(void)));

			this->menu_view->addMenu(pan_submenu);
		}


		this->menu_view->addSeparator();


		qa = new QAction("Background &Jobs", this);
		qa->setIcon(QIcon::fromTheme("emblem-system"));
		connect(qa, SIGNAL(triggered(bool)), this, SLOT(show_background_jobs_window_cb(void)));
		this->menu_view->addAction(qa);

#if 1           /* This is only for debugging purposes (or is it not?). */
		qa = new QAction("Show Centers", this);
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (show_centers_cb(void)));
		this->menu_view->addAction(qa);

		qa = new QAction("&Map Cache Info", this);
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_view_cache_info_cb(void)));
		this->menu_view->addAction(qa);
#endif
	}



	/* "Layers" menu. */
	{
		this->qa_tree_item_properties = new QAction("Properties...", this);
		this->menu_layers->addAction(this->qa_tree_item_properties);
		connect(this->qa_tree_item_properties, SIGNAL (triggered(bool)), this->items_tree->get_tree_view(), SLOT (tree_item_properties_cb(void)));

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

	qDebug() << "II: Window: clicked \"layer new\" for layer type" << Layer::get_type_ui_label(layer_type);


	Layer * layer = Layer::construct_layer(layer_type, this->viewport, true);
	if (layer) {
		this->items_tree->add_layer(layer, this->viewport->get_coord_mode());

		this->viewport->configure();
		qDebug() << "II: Layers Panel: calling layer->draw() for new layer" << Layer::get_type_ui_label(layer_type);
		layer->draw(this->viewport);

		qDebug() << "II: Window: new layer, call draw_update_cb()" << __FUNCTION__ << __LINE__;
		this->draw_update();
		this->contents_modified = true;
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

	qDebug() << "II: Window:    selection: draw redraw";

	/* Actually draw. */
	this->viewport->clear();
	/* Main layer drawing. */
	this->items_tree->draw_all(this->viewport);

	/* Draw highlight (possibly again but ensures it is on top - especially for when tracks overlap). */
	if (this->viewport->get_draw_with_highlight()) {
		qDebug() << "II: Window:    selection: do draw with highlight";

		/* If there is a layer or layer's sublayers or items
		   that are selected in main tree, draw them with
		   highlight. */
		if (g_tree->selected_tree_item) {
			/* It is up to the selected item to see and
			   decide what exactly is selected and how to
			   draw it. Window class doesn't care about
			   such details. */
			g_tree->selected_tree_item->draw_tree_item(this->viewport, true, true);
		} else {
			;
		}
	} else {
		qDebug() << "II: Window:    selection: don't draw with highlight";
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




void Window::handle_selection_of_layer(Layer * layer)
{
	qDebug() << "II: Window: selected layer type" << layer->get_type_ui_label();

	this->toolbox->handle_selection_of_layer(layer->get_type_id_string());
}




Viewport * Window::get_viewport()
{
	return this->viewport;
}




LayersPanel * Window::get_items_tree()
{
	return this->items_tree;
}




Toolbox * Window::get_toolbox(void)
{
	return this->toolbox;
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
	menu->addAction(this->qa_tree_item_properties);
	return menu;
}




QMenu * Window::new_layers_submenu_add_actions(QMenu * menu)
{
	for (LayerType type = LayerType::AGGREGATE; type < LayerType::NUM_TYPES; ++type) {

		const LayerInterface * iface = Layer::get_interface(type);

		QVariant variant((int) type);
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


	/* Menu Tools -> Generic tools;
	   Toolbar -> Generic Tools. */
	{
		LayerToolContainer * tools = GenericTools::create_tools(this, this->viewport);

		QActionGroup * tools_group = this->toolbox->add_tools(tools);

		/* Tools should have been moved to toolbox. Delete container (but not the tools themselves). */
		tools->erase(tools->begin(), tools->end());
		delete tools;

		if (tools_group) {
			const QList <QAction *> actions = tools_group->actions();

			if (!actions.isEmpty()) {
				tools_group->setObjectName("generic");

				this->toolbar->addSeparator();
				this->toolbar->addActions(actions);

				this->menu_tools->addSeparator();
				this->menu_tools->addActions(actions);

				/* The same callback for all generic tools. */
				connect(tools_group, SIGNAL(triggered(QAction *)), this, SLOT(layer_tool_cb(QAction *)));

				/* We want some action in "generic tools"
				   group to be active by default. Let it be
				   the first tool in the group. */
				QAction * default_qa = actions.first();
				default_qa->setChecked(true);
				default_qa->trigger();
				this->toolbox->activate_tool(default_qa);
			}
		} else {
			qDebug() << "EE: Window: Create UI: NULL generic tools group";
		}
	}


	/* Menu Tools -> layer-specific tools;
	   Toolbar -> layer-specific tools. */
	{
		for (LayerType type = LayerType::AGGREGATE; type < LayerType::NUM_TYPES; ++type) {

			/* We can't build the layer tools when a layer
			   interface is constructed, because the layer
			   tools require Window and Viewport
			   variables, which may not be available at that time. */

			LayerToolContainer * tools = Layer::get_interface(type)->create_tools(this, this->viewport);
			if (!tools) {
				/* Either error, or given layer type has no layer-specific tools. */
				continue;
			}

			QActionGroup * tools_group = this->toolbox->add_tools(tools);

			/* Tools should have been moved to toolbox. Delete container (but not the tools themselves). */
			tools->erase(tools->begin(), tools->end());
			delete tools;

			if (!tools_group) {
				qDebug() << "EE: Window: Create UI: NULL layer tools group";
				continue;
			}

			const QList<QAction *> actions = tools_group->actions();

			if (!actions.isEmpty()) {
				tools_group->setObjectName(Layer::get_type_id_string(type));

				this->toolbar->addSeparator();
				this->toolbar->addActions(actions);

				this->menu_tools->addSeparator();
				this->menu_tools->addActions(actions);

				tools_group->setEnabled(false); /* A layer-specific tool group is disabled by default, until a specific layer is selected in tree view. */

				/* The same callback for all layer tools. */
				connect(tools_group, SIGNAL (triggered(QAction *)), this, SLOT (layer_tool_cb(QAction *)));
			}
		}
	}

	a_background_post_init_window(this);
}




/* Callback common for all layer tool actions. */
void Window::layer_tool_cb(QAction * qa)
{
	/* Handle old tool first. */
	QAction * old_qa = this->toolbox->get_active_tool_action();
	if (old_qa) {
		qDebug() << "II: Window: deactivating old tool" << old_qa->objectName();
		this->toolbox->deactivate_tool(old_qa);
	} else {
		/* The only valid situation when it happens is only during start up of application. */
		qDebug() << "WW: Window: no old action found";
	}


	/* Now handle newly selected tool. */
	if (qa) {
		this->toolbox->activate_tool(qa);

		QString tool_name = qa->objectName();
		qDebug() << "II: Window: setting 'release' cursor for" << tool_name;
		this->viewport->setCursor(*this->toolbox->get_cursor_release(tool_name));
		this->display_tool_name();
	}
}




void Window::activate_tool(const QString & tool_id)
{
	this->toolbox->deactivate_current_tool();
	this->toolbox->activate_tool(tool_id);

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
   \brief Retrieve window's pan_move_flag

   Should be removed as soon as possible.

   \return window's pan_move
*/
bool Window::get_pan_move(void)
{
	return this->pan_move_flag;
}




void Window::menu_edit_cut_cb(void)
{
	this->items_tree->cut_selected_cb();
	this->contents_modified = true;
}




void Window::menu_edit_copy_cb(void)
{
#ifdef K
	a_clipboard_copy_selected(this->layers_tree);
#endif
}




void Window::menu_edit_paste_cb(void)
{
	if (this->items_tree->paste_selected_cb()) {
		this->contents_modified = true;
	}
}




void Window::menu_edit_delete_cb(void)
{
	if (this->items_tree->get_selected_layer()) {
		this->items_tree->delete_selected_cb();
		this->contents_modified = true;
	} else {
		Dialog::info(tr("You must select a layer to delete."), this);
	}
}




void Window::menu_edit_delete_all_cb(void)
{
	/* Do nothing if empty. */
	if (!this->items_tree->get_top_layer()->is_empty()) {
		if (Dialog::yes_or_no(tr("Are you sure you wish to delete all layers?"), this)) {
			this->items_tree->clear();
			this->set_current_document_full_path("");
			this->draw_update();
		}
	}
}




void Window::menu_copy_centre_cb(void)
{
	QString first;
	QString second;

	const Coord * coord = this->viewport->get_center();

	bool full_format = false;
	(void) a_settings_get_boolean(VIK_SETTINGS_WIN_COPY_CENTRE_FULL_FORMAT, &full_format);

	if (full_format) {
		/* Bells & Whistles - may include degrees, minutes and second symbols. */
		coord->to_strings(first, second);
	} else {
		/* Simple x.xx y.yy format. */
		struct LatLon ll;
		struct UTM utm;
		a_coords_utm_to_latlon(&ll, &utm);
		first = QString("%1").arg(ll.lat, 0, 'f', 6); /* "%.6f" */
		second = QString("%1").arg(ll.lon, 0, 'f', 6);
	}

	const QString message = QString("%1 %2").arg(first).arg(second);
#ifdef K
	a_clipboard_copy(VIK_CLIPBOARD_DATA_TEXT, LayerType::AGGREGATE, SublayerType::NONE, 0, message, NULL);
#endif
}




void Window::map_cache_flush_cb(void)
{
	map_cache_flush();
}




void Window::set_default_location_cb(void)
{
	/* Simplistic repeat of preference setting
	   Only the name & type are important for setting the preference via this 'external' way */
	Parameter pref_lat[] = {
		{ 1, PREFERENCES_NAMESPACE_GENERAL "default_latitude",  SGVariantType::DOUBLE, PARAMETER_GROUP_GENERIC, NULL, WidgetType::SPINBOX_DOUBLE, NULL, NULL, NULL, NULL },
	};
	Parameter pref_lon[] = {
		{ 1, PREFERENCES_NAMESPACE_GENERAL "default_longitude", SGVariantType::DOUBLE, PARAMETER_GROUP_GENERIC, NULL, WidgetType::SPINBOX_DOUBLE, NULL, NULL, NULL, NULL },
	};


	/* Get current center */
	struct LatLon ll = this->viewport->get_center()->get_latlon();


	/* Apply to preferences */
	SGVariant vlp_data;

	vlp_data = SGVariant((double) ll.lat);
	a_preferences_run_setparam(vlp_data, pref_lat);

	vlp_data = SGVariant((double) ll.lon);
	a_preferences_run_setparam(vlp_data, pref_lon);


	/* Remember to save */
	a_preferences_save_to_file();
}




void Window::preferences_cb(void) /* Slot. */
{
#if 0
	bool wp_icon_size = Preferences::get_use_large_waypoint_icons();
#endif
	preferences_show_window(this);
#if 0
	/* Has the waypoint size setting changed? */
	if (wp_icon_size != Preferences::get_use_large_waypoint_icons()) {
		/* Delete icon indexing 'cache' and so automatically regenerates with the new setting when changed. */
		clear_garmin_icon_syms();

		// Update all windows
		for (auto i = window_list.begin(); i != window_list.end(); i++) {
			(*)->preferences_change_update();
		}
	}


	/* Ensure TZ Lookup initialized. */
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
	if (window->contents_modified)
#else
	if (0)
#endif
#endif
	{
		QMessageBox::StandardButton reply = QMessageBox::question(this, "SlavGPS",
									  QString("Changes in file '%1' are not saved and will be lost if you don't save them.\n\n"
										  "Do you want to save the changes?").arg("some file"), // window->get_current_document_file_name()
									  QMessageBox::Cancel | QMessageBox::No | QMessageBox::Yes,
									  QMessageBox::Yes);
		if (reply == QMessageBox::No) {
			ev->accept();
		} else if (reply == QMessageBox::Yes) {
			//this->menu_file_save_cb();
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
		a_settings_set_boolean(VIK_SETTINGS_WIN_SAVE_IMAGE_PNG, window->save_viewport_as_png);

		const QString accel_file_full_path = get_viking_dir() + QDir::separator + VIKING_ACCELERATOR_KEY_FILE;
		gtk_accel_map_save(accel_file_full_path.toUtf8().constData());
#endif
	}
}




void Window::goto_default_location_cb(void)
{
	struct LatLon ll;
	ll.lat = Preferences::get_default_lat();
	ll.lon = Preferences::get_default_lon();
	this->viewport->set_center_latlon(&ll, true);
	this->items_tree->emit_update_window_cb();
}




void Window::goto_location_cb()
{
	goto_location(this, this->viewport);
	this->items_tree->emit_update_window_cb();
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



void Window::draw_with_highlight_cb(bool new_state)
{
	assert (new_state != this->draw_with_highlight);
	if (new_state != this->draw_with_highlight) {
		this->viewport->set_draw_with_highlight(new_state);
		this->draw_update();
		this->draw_with_highlight = !this->draw_with_highlight;
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
	const LayerTool * tool = this->toolbox->get_current_tool();
	if (tool) {
		this->status_bar->set_message(StatusBarField::TOOL, tool->get_description());
	}
}








bool vik_window_clear_highlight_cb(Window * window)
{
	return window->clear_highlight();
}




bool Window::clear_highlight()
{
	bool need_redraw = false;
	if (g_tree->selected_tree_item) {
		if (g_tree->selected_tree_item->tree_item_type == TreeItemType::LAYER) { /* TODO: use UID to compare tree items. */
			/* FIXME: we assume here that only LayerTRW can be a selected layer. */
			need_redraw |= ((LayerTRW *) g_tree->selected_tree_item)->clear_highlight();
		}
		g_tree->selected_tree_item = NULL;
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

	qDebug() << "II: Window: clicked \"layer defaults\" for layer type" << Layer::get_type_ui_label(layer_type);

	if (Layer::get_interface(layer_type)->has_properties_dialog()) {
		LayerDefaults::show_window(layer_type, this);
	} else {
		Dialog::info(tr("This layer type has no configurable properties."), this);
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


	QFileDialog dialog(this, "Select a GPS data file to open");

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
		if ((window->contents_modified || !window->current_document_full_path.isEmpty()) && newwindow) {
#else
		if (!window->current_document_full_path.isEmpty() && newwindow) {
#endif
			g_signal_emit(window, window_signals[VW_OPENWINDOW_SIGNAL], 0, gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog)));
		} else {
#endif

			QStringList files = dialog.selectedFiles();
			bool set_as_current_document = newwindow && (files.size() == 1); /* Only change current document path if one file. */
			bool first_vik_file = true;
			auto iter = files.begin();
			while (iter != files.end()) {

				QString file_name = *iter;
				if (newwindow && check_file_magic_vik(file_name.toUtf8().data())) {
					/* Load first of many .vik files in current window. */
					if (first_vik_file) {
						this->open_file(file_name, true);
						first_vik_file = false;
					} else {
						/* Load each subsequent .vik file in a separate window. */
						Window * new_window = Window::new_window();
						if (new_window) {
							new_window->open_file(file_name, true);
						}
					}
				} else {
					/* Other file types. */
					this->open_file(file_name, set_as_current_document);
				}

				iter++;
			}
#ifdef K
		}
#endif
	}
}




void Window::open_file(const QString & new_document_full_path, bool set_as_current_document)
{
	this->set_busy_cursor();

	/* Enable the *new* filename to be accessible by the Layers code. */
	const QString original_filename = this->current_document_full_path;
	this->current_document_full_path = new_document_full_path;

	bool success = false;
	bool restore_original_filename = false;

	LayerAggregate * agg = this->items_tree->get_top_layer();
	this->loaded_type = a_file_load(agg, this->viewport, new_document_full_path.toUtf8().constData());
	switch (this->loaded_type) {
	case LOAD_TYPE_READ_FAILURE:
		Dialog::error(tr("The file you requested could not be opened."), this);
		break;
	case LOAD_TYPE_GPSBABEL_FAILURE:
		Dialog::error(tr("GPSBabel is required to load files of this type or GPSBabel encountered problems."), this);
		break;
	case LOAD_TYPE_GPX_FAILURE:
		Dialog::error(tr("Unable to load malformed GPX file %1").arg(new_document_full_path), this);
		break;
	case LOAD_TYPE_UNSUPPORTED_FAILURE:
		Dialog::error(tr("Unsupported file type for %1").arg(new_document_full_path), this);
		break;
	case LOAD_TYPE_VIK_FAILURE_NON_FATAL:
		{
			/* Since we can process .vik files with issues just show a warning in the status bar.
			   Not that a user can do much about it... or tells them what this issue is yet... */
			this->get_statusbar()->set_message(StatusBarField::INFO, QString("WARNING: issues encountered loading %1").arg(file_base_name(new_document_full_path.toUtf8().constData())));
		}
		/* No break, carry on to show any data. */
	case LOAD_TYPE_VIK_SUCCESS:
		{

			restore_original_filename = true; /* Will actually get inverted by the 'success' component below. */
			/* Update UI */
			if (set_as_current_document) {
				this->set_current_document_full_path(new_document_full_path);
			}
#ifdef K
			QAction * drawmode_action = this->grepget_drawmode_action(this->viewport->get_drawmode());
			this->only_updating_coord_mode_ui = true; /* if we don't set this, it will change the coord to UTM if we click Lat/Lon. I don't know why. */
			gtk_check_menu_item_set_active(drawmode_action, true);
			this->only_updating_coord_mode_ui = false;
#endif

			this->items_tree->change_coord_mode(this->viewport->get_coord_mode());

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
			bool vp_state_highlight = this->viewport->get_draw_with_highlight();
			bool ui_state_highlight = this->qa_view_show_draw_with_highlight->isChecked();
			if (vp_state_highlight != ui_state_highlight) {
				this->viewport->set_draw_with_highlight(!vp_state_highlight);
				this->draw_with_highlight_cb(!vp_state_highlight);
			}
		}
		/* No break, carry on to redraw. */
		//case LOAD_TYPE_OTHER_SUCCESS:
	default:
		success = true;
		/* When LOAD_TYPE_OTHER_SUCCESS *only*, this will maintain the existing Viking project. */
		restore_original_filename = !restore_original_filename;
		this->update_recently_used_document(new_document_full_path.toUtf8().constData());
		this->update_recent_files(new_document_full_path);
		this->draw_update();
		break;
	}

	if (!success || restore_original_filename) {
		// Load didn't work or want to keep as the existing Viking project, keep using the original name
		this->set_current_document_full_path(original_filename);
	}

	this->clear_busy_cursor();
}




bool Window::menu_file_save_cb(void)
{
	if (this->current_document_full_path.isEmpty()) {
		return this->menu_file_save_as_cb();
	} else {
		this->contents_modified = false;
		return this->window_save();
	}
}




bool Window::menu_file_save_as_cb(void)
{
	bool rv = false;

	QFileDialog file_selector(this, QObject::tr("Save as Viking File."));
	file_selector.setFileMode(QFileDialog::AnyFile); /* Specify new or select existing file. */
	file_selector.setAcceptMode(QFileDialog::AcceptSave);

	/* TODO: make sure that Viking file format is default one. */
	QStringList filter;
	filter << tr("All (*)");
	filter << tr("Viking (*.vik, *.viking)");
	file_selector.setNameFilters(filter);


	if (last_folder_files_url.isValid()) {
		file_selector.setDirectoryUrl(last_folder_files_url);
	}

#ifdef K
	gtk_window_set_transient_for(file_selector, this);
	gtk_window_set_destroy_with_parent(file_selector, true);

#endif

	/* Auto append / replace extension with '.vik' to the suggested file name as it's going to be a Viking File. */
	QString auto_save_name = this->get_current_document_file_name();
	if (!a_file_check_ext(auto_save_name, ".vik")) {
		auto_save_name = auto_save_name + ".vik";
	}

	file_selector.selectFile(auto_save_name);


	while (QDialog::Accepted == file_selector.exec()) {
		QStringList selection = file_selector.selectedFiles();
		if (!selection.size()) {
			continue;
		}
		const QString full_path = selection.at(0);

		if (0 != access(full_path.toUtf8().constData(), F_OK) || Dialog::yes_or_no(tr("The file \"%1\" exists, do you wish to overwrite it?").arg(file_base_name(full_path)), this)) {
			this->set_current_document_full_path(full_path);
			rv = this->window_save();
			if (rv) {
				this->contents_modified = false;
				last_folder_files_url = file_selector.directoryUrl();
			}
			break;
		}
	}

	return rv;
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




void Window::update_recently_used_document(const QString & file_full_path)
{
#ifdef K
	/* Update Recently Used Document framework */
	GtkRecentManager *manager = gtk_recent_manager_get_default();
	GtkRecentData * recent_data = g_slice_new(GtkRecentData);
	char *groups[] = { (char *) "viking", NULL};
	GFile * file = g_file_new_for_commandline_arg(file_full_path);
	char * uri = g_file_get_uri(file);
	char * basename = g_path_get_basename(file_full_path);
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
	this->setCursor(Qt::WaitCursor);
	/* Viewport has a separate cursor. TODO: verify this */
	this->viewport->setCursor(Qt::WaitCursor);

#ifdef K
	/* Ensure cursor updated before doing stuff. TODO: do we need this? */
	while (gtk_events_pending()) {
		gtk_main_iteration();
	}
#endif
}




void Window::clear_busy_cursor()
{
	this->setCursor(Qt::ArrowCursor);
	/* Viewport has a separate cursor. */
	this->viewport->setCursor(this->viewport_cursor);
}




void Window::set_current_document_full_path(const QString & document_full_path)
{
	this->current_document_full_path = document_full_path;

	/* Refresh title of main window, but not with full (and possibly long) path, but just file name. */
	this->setWindowTitle(tr("%1 - SlavGPS").arg(this->get_current_document_file_name()));
}




QString Window::get_current_document_file_name(void)
{
	return this->current_document_full_path.isEmpty() ? tr("Untitled") : file_basename(this->current_document_full_path.toUtf8().constData());
}




QString Window::get_current_document_full_path(void)
{
	return this->current_document_full_path;
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
		locator->window->items_tree->emit_update_window_cb();
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
#ifdef K
	/* Don't add a map if we've loaded a Viking file already. */
	if (!this->current_document_full_path.isEmpty()) {
		return;
	}

	if (Preferences::get_startup_method() == VIK_STARTUP_METHOD_SPECIFIED_FILE) {
		this->open_file(Preferences::get_startup_file(), true);
		if (!this->current_document_full_path.isEmpty()) {
			return;
		}
	}

	/* Maybe add a default map layer. */
	if (Preferences::get_add_default_map_layer()) {
		LayerMap * layer = new LayerMap();
		layer->set_name(QObject::tr("Default Map"));

		this->items_tree->get_top_layer()->add_layer(layer, true);

		this->draw_update();
	}

	/* If not loaded any file, maybe try the location lookup. */
	if (this->loaded_type == LOAD_TYPE_READ_FAILURE) {
		if (Preferences::get_startup_method() == VIK_STARTUP_METHOD_AUTO_LOCATION) {

			this->status_bar->set_message(StatusBarField::INFO, tr("Trying to determine location..."));
			LocatorJob * locator = new LocatorJob(this);
			a_background_thread(locator, ThreadPoolType::REMOTE, tr("Determining location"));
		}
	}
#endif
}




void Window::open_window(void)
{
	GSList *files = NULL;

	bool set_as_current_document = (g_slist_length(files) == 1); /* Only change fn if one file. */
	GSList *cur_file = files;
	while (cur_file) {
		/* Only open a new window if a viking file. */
		char *file_name = (char *) cur_file->data;
		if (!this->current_document_full_path.isEmpty() && check_file_magic_vik(file_name)) {
			Window * new_window = Window::new_window();
			if (new_window) {
				new_window->open_file(file_name, true);
			}
		} else {
			this->open_file(file_name, set_as_current_document);
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
	a_acquire(this, this->items_tree, this->viewport, mode, datasource, NULL, NULL);
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
	ViewportToImageDialog dialog(tr("Save Viewport to Image File"), this->get_viewport(), NULL);
	dialog.build_ui(ViewportSaveMode::FILE);
	if (QDialog::Accepted != dialog.exec()) {
		return;
	}

	QString file_full_path = this->save_viewport_get_full_path(ViewportSaveMode::FILE);
	if (file_full_path.isEmpty()) {
		return;
	}

	int active_z = dialog.zoom_combo->currentIndex();
	double zoom = pow(2, active_z - 2);
	qDebug() << "II: Viewport: Save: zoom index:" << active_z << ", zoom value:" << zoom;

	this->save_viewport_to_image(file_full_path,
				     this->draw_image_width = dialog.width_spin->value(),
				     this->draw_image_height = dialog.height_spin->value(),
				     zoom,
				     this->save_viewport_as_png, // = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(png_radio)), // kamilTODO
				     false);
}




void Window::draw_viewport_to_image_dir_cb(void)
{
	ViewportToImageDialog dialog(tr("Save Viewport to Images in Directory"), this->get_viewport(), NULL);
	dialog.build_ui(ViewportSaveMode::DIRECTORY);
	if (QDialog::Accepted != dialog.exec()) {
		return;
	}

	QString dir_full_path = this->save_viewport_get_full_path(ViewportSaveMode::DIRECTORY);
	if (dir_full_path.isEmpty()) {
		return;
	}

	int active_z = dialog.zoom_combo->currentIndex();
	double zoom = pow(2, active_z - 2);
	qDebug() << "II: Window: Viewport to image dir: zoom index:" << active_z << ", zoom value:" << zoom;

	/* UTM mode ATM. */
	this->save_viewport_to_dir(dir_full_path,
				   this->draw_image_width = dialog.width_spin->value(),
				   this->draw_image_height = dialog.height_spin->value(),
				   zoom,
				   this->save_viewport_as_png, // = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(png_radio)), // kamilTODO
				   dialog.tiles_width_spin->value(),
				   dialog.tiles_height_spin->value());
}




#ifdef HAVE_ZIP_H
void Window::draw_viewport_to_kmz_file_cb(void)
{
	if (this->viewport->get_coord_mode() == CoordMode::UTM) {
		Dialog::error(tr("This feature is not available in UTM mode"));
		return;
	}

	ViewportToImageDialog dialog(tr("Save Viewport to KMZ File"), this->get_viewport(), NULL);
	dialog.build_ui(mode);
	if (QDialog::Accepted != dialog.exec()) {
		return;
	}

	QString file_full_path = this->save_viewport_get_full_path(ViewportSaveMode::FILE_KMZ);
	if (file_full_path.isEmpty()) {
		return;
	}

	int active_z = dialog.zoom_combo->currentIndex();
	double zoom = pow(2, active_z - 2);
	qDebug() << "II: Window: Viewport to kmz file: zoom index:" << active_z << ", zoom value:" << zoom;


	/* ATM This only generates a KMZ file with the current
	   viewport image - intended mostly for map images [but will
	   include any lines/icons from track & waypoints that are
	   drawn] (it does *not* include a full KML dump of every
	   track, waypoint etc...). */

	/* Remove some viewport overlays as these aren't useful in KMZ file. */
	bool has_xhair = this->viewport->get_draw_centermark();
	if (has_xhair) {
		this->viewport->set_draw_centermark(false);
	}
	bool has_scale = this->viewport->get_draw_scale();
	if (has_scale) {
		this->viewport->set_draw_scale(false);
	}

	this->save_viewport_to_image(file_full_path,
				     dialog.width_spin->value(),
				     dialog.height_spin->value(),
				     zoom,
				     false, // JPG
				     true);

	if (has_xhair) {
		this->viewport->set_draw_centermark(true);
	}

	if (has_scale) {
		this->viewport->set_draw_scale(true);
	}

	if (has_xhair || has_scale) {
		this->draw_update();
	}
}
#endif




void Window::print_cb(void)
{
	a_print(this, this->viewport);
}




void Window::save_viewport_to_image(const QString & file_full_path, unsigned int w, unsigned int h, double zoom, bool save_as_png, bool save_kmz)
{
	/* More efficient way: stuff draws directly to pixbuf (fork viewport). TODO: verify this comment. */

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

	this->status_bar->set_message(StatusBarField::INFO, QString("Generating image file..."));

	/* backup old zoom & set new */
	double old_xmpp = this->viewport->get_xmpp();
	double old_ympp = this->viewport->get_ympp();
	this->viewport->set_zoom(zoom);

	/* Set expected width and height. */
	this->viewport->configure_manually(w, h);

	/* Redraw all layers at current position and zoom. */
	this->draw_redraw();

	/* Save buffer as file. */
	QPixmap * pixmap = this->viewport->get_pixmap();
	//QPixmap * pixmap = gdk_pixbuf_get_from_drawable(NULL, GDK_DRAWABLE(this->viewport->get_pixmap()), NULL, 0, 0, 0, 0, w, h);

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
		ans = kmz_save_file(pixmap, file_full_path, north, east, south, west);
#endif
	} else {
		qDebug() << "II: Viewport: Save to Image: Saving pixmap";
		if (!pixmap->save(file_full_path, save_as_png ? "png" : "jpeg")) {
			qDebug() << "WW: Viewport: Save to Image: Unable to write to file" << file_full_path;
			success = false;
		}
	}

#ifdef K
	g_object_unref(G_OBJECT(pixmap));
#endif

	const QString message = success ? tr("Image file generated.") : tr("Failed to generate image file.");

	/* Cleanup. */

	this->status_bar->set_message(StatusBarField::INFO, QString(""));
	Dialog::info(message, this);

	this->viewport->set_xmpp(old_xmpp);
	this->viewport->set_ympp(old_ympp);
	this->viewport->configure();
	this->draw_update();
}




bool Window::save_viewport_to_dir(const QString & dir_full_path, unsigned int w, unsigned int h, double zoom, bool save_as_png, unsigned int tiles_w, unsigned int tiles_h)
{
	if (this->viewport->get_coord_mode() != CoordMode::UTM) {
		Dialog::error(tr("You must be in UTM mode to use this feature"), this);
		return false;
	}


	/* backup old zoom & set new */
	double old_xmpp = this->viewport->get_xmpp();
	double old_ympp = this->viewport->get_ympp();
	this->viewport->set_zoom(zoom);

	/* Set expected width and height. Do this only once for all images (all images have the same size). */
	this->viewport->configure_manually(w, h);

	QDir dir(dir_full_path);
	if (!dir.exists()) {
		if (!dir.mkpath(dir_full_path)) {
			qDebug() << "EE: Window: failed to create directory" << dir_full_path << "in" << __FUNCTION__ << __LINE__;
			return false;
		}
	}

	struct UTM utm;
	struct UTM utm_orig = this->viewport->get_center()->utm;
	const QString extension = save_as_png ? "png" : "jpg";

	for (unsigned int y = 1; y <= tiles_h; y++) {
		for (unsigned int x = 1; x <= tiles_w; x++) {
			QString file_full_path = QString("%1%2y%3-x%4.%5").arg(dir_full_path).arg(QDir::separator()).arg(y).arg(x).arg(extension);
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

			/* TODO: move to correct place. */
			this->viewport->set_center_utm(&utm, false);

			/* Redraw all layers at current position and zoom. */
			this->draw_redraw();
#ifdef K
			/* Save buffer as file. */
			// QPixmap * pixmap = gdk_pixbuf_get_from_drawable(NULL, GDK_DRAWABLE (this->viewport->get_pixmap()), NULL, 0, 0, 0, 0, w, h);
			QPixmap * pixmap = this->viewport->get_pixmap();
			if (!pixmap->save(file_full_path, extension)) {
				qDebug() << "WW: Viewport: Save to Image Dir: Unable to write to file" << file_full_path;
				this->status_bar->set_message(StatusBarField::INFO, QString("Unable to write to file %1").arg(file_full_path));
			}

			g_object_unref(G_OBJECT(pixmap));
#endif
		}
	}

	this->viewport->set_center_utm(&utm_orig, false);
	this->viewport->set_xmpp(old_xmpp);
	this->viewport->set_ympp(old_ympp);
	this->viewport->configure();
	this->draw_update();

	return true;
}




/**
   Get full path to either single file or to directory, to which to save a viewport image(s).
*/
QString Window::save_viewport_get_full_path(ViewportSaveMode mode)
{
	QString result;
	QStringList mime;

	QFileDialog file_selector(this);
	file_selector.setAcceptMode(QFileDialog::AcceptSave);
	if (last_folder_images_url.isValid()) {
		file_selector.setDirectoryUrl(last_folder_images_url);
	}

	switch (mode) {
	case ViewportSaveMode::DIRECTORY:

		file_selector.setWindowTitle(tr("Select directory to save Viewport to"));
		file_selector.setFileMode(QFileDialog::Directory);
		file_selector.setOption(QFileDialog::ShowDirsOnly);

		break;

	case ViewportSaveMode::FILE_KMZ:
	case ViewportSaveMode::FILE: /* png or jpeg. */

		file_selector.setWindowTitle(tr("Select file to save Viewport to"));
		file_selector.setFileMode(QFileDialog::AnyFile); /* Specify new or select existing file. */

		mime << "application/octet-stream"; /* "All files (*)" */
		if (mode == ViewportSaveMode::FILE_KMZ) {
			mime << "vnd.google-earth.kmz"; /* "KMZ" / "*.kmz"; */
		} else {
			if (this->save_viewport_as_png) {
				mime << "image/png";
			} else {
				mime << "image/jpeg";
			}
		}
		file_selector.setMimeTypeFilters(mime);
		break;
	default:
		qDebug() << "EE: Window: Save Viewport / get full path: unsupported mode" << (int) mode;
		return result; /* Empty string. */
	}


	if (QDialog::Accepted == file_selector.exec()) {
		last_folder_images_url = file_selector.directoryUrl();
		qDebug() << "II: Viewport: Save to Image: last directory saved as:" << last_folder_images_url;

		result = file_selector.selectedFiles().at(0);
		qDebug() << "II: Viewport: Save to Image: target file:" << result;

#ifdef K
		if (0 == access(result.toUtf8().constData(), F_OK)) {
			if (!Dialog::yes_or_no(tr("The file \"%1\" exists, do you wish to overwrite it?").arg(file_base_name(result)), this, "")) {
				result.resize(0);
			}
		}
#endif
	}


	qDebug() << "DD: Viewport: Save to Image: result: '" << result << "'";
	if (result.isEmpty()) {
		qDebug() << "DD: Viewport: Save to Image: returning NULL\n";
	}

	return result;
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
		menu = parent->addMenu(label);
	} else {
		menu = new QMenu(label);
	}

	if (!zoom_actions.size()) {
		create_zoom_actions();
	}

	for (auto iter = zoom_actions.begin(); iter != zoom_actions.end(); iter++) {
		menu->addAction(*iter);
	}



	int active = 5 + round(log(mpp) / log(2));
	/* Ensure value derived from mpp is in bounds of the menu. */
	if (active >= (int) zoom_actions.size()) {
		active = zoom_actions.size() - 1;
	}
	if (active < 0) {
		active = 0;
	}
	menu->setActiveAction(zoom_actions[active]);

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
			this->items_tree->change_coord_mode(CoordMode::UTM);
		} else if (olddrawmode == ViewportDrawMode::UTM) {
			this->items_tree->change_coord_mode(CoordMode::LATLON);
		}
		this->draw_update();
	}
}




/**
   Refresh maps displayed.
*/
void Window::menu_view_refresh_cb(void)
{
	/* Only get 'new' maps. */
	this->simple_map_update(true);
}




void Window::menu_view_set_highlight_color_cb(void)
{
	QColorDialog color_dialog(this);
	color_dialog.setCurrentColor(this->viewport->get_highlight_color());
	color_dialog.setWindowTitle(tr("Choose a highlight color"));
	if (QDialog::Accepted == color_dialog.exec()) {
		const QColor selected_color = color_dialog.selectedColor();
		this->viewport->set_highlight_color(selected_color);
		this->draw_update();
	}
}




void Window::menu_view_set_bg_color_cb(void)
{
	QColorDialog color_dialog(this);
	color_dialog.setCurrentColor(this->viewport->get_background_color());
	color_dialog.setWindowTitle(tr("Choose a background color"));
	if (QDialog::Accepted == color_dialog.exec()) {
		const QColor selected_color = color_dialog.selectedColor();
		this->viewport->set_background_color(selected_color);
		this->draw_update();
	}
}




void Window::menu_view_pan_cb(void)
{
	const QAction * qa = (QAction *) QObject::sender();
	const int direction = qa->data().toInt();
	qDebug() << "SLOT: Window: Menu View Pan:" << qa->text() << direction;

	/* TODO: verify how this comment applies to Qt application. */
	/* Since the tree view cell editting intercepts standard
	   keyboard handlers, it means we can receive events here.
	   Thus if currently editting, ensure we don't move the
	   viewport when Ctrl+<arrow> is received. */
	Layer * layer = this->items_tree->get_selected_layer();
	if (layer && layer->tree_view->is_editing_in_progress()) {
		qDebug() << "SLOT: Window: Menu View Pan: editing in progress";
		return;
	}

	Viewport * v = this->viewport;

	switch (direction) {
	case PAN_NORTH:
		v->set_center_screen(v->get_width() / 2, 0);
		break;
	case PAN_EAST:
		v->set_center_screen(v->get_width(), v->get_height() / 2);
		break;
	case PAN_SOUTH:
		v->set_center_screen(v->get_width() / 2, v->get_height());
		break;
	case PAN_WEST:
		v->set_center_screen(0, v->get_height() / 2);
		break;
	default:
		qDebug() << "EE: Window: unknown direction" << direction << "in" << __FUNCTION__ << __LINE__;
		break;
	}

	this->draw_update();
}




/**
   Update the displayed map
   Only update the top most visible map layer
   ATM this assumes (as per defaults) the top most map has full alpha setting
   such that other other maps even though they may be active will not be seen
   It's more complicated to work out which maps are actually visible due to alpha settings
   and overkill for this simple refresh method.
*/
void Window::simple_map_update(bool only_new)
{
	/* Find the most relevent single map layer to operate on. */
	Layer * layer = this->items_tree->get_top_layer()->get_top_visible_layer_of_type(LayerType::MAP);
	if (layer) {
		((LayerMap *) layer)->download(this->viewport, only_new);
	}
}





void Window::configure_event_cb()
{
	static int first = 1;
	this->draw_redraw();
	if (first) {
		/* This is a hack to set the cursor corresponding to the first tool.
		   FIXME find the correct way to initialize both tool and its cursor. */
		first = 0;

		this->viewport_cursor = Qt::OpenHandCursor;
		this->viewport->setCursor(this->viewport_cursor);
	}
}




/* Mouse event handlers ************************************************************************/


void Window::draw_click_cb(QMouseEvent * ev)
{
#ifdef K
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
#endif
}




/**
 * Action the single click after a small timeout.
 * If a double click has occurred then this will do nothing.
 */
static bool window_pan_timeout(Window * window)
{
#ifdef K
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
#endif
	return false;
}




void Window::draw_release_cb(QMouseEvent * ev)
{
#ifdef K
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
#endif
}




void Window::export_to_gpx_cb(void)
{
	this->export_to_common(SGFileType::GPX, ".gpx");
}




void Window::export_to_kml_cb(void)
{
	this->export_to_common(SGFileType::KML, ".kml");
}




/**
   Export all TRW Layers in the list to individual files in the specified directory

   Returns: %true on success
*/
bool Window::export_to(std::list<const Layer *> * layers, SGFileType file_type, const QString & full_dir_path, char const *extension)
{
	bool success = true;

	int export_count = 0;

	this->set_busy_cursor();

	for (auto iter = layers->begin(); iter != layers->end(); iter++) {
		const Layer * layer = *iter;
		QString path = full_dir_path + QDir::separator() + layer->name + extension;

		/* Some protection in attempting to write too many same named files.
		   As this will get horribly slow... */
		bool safe = false;
		int ii = 2;
		while (ii < 5000) {
			if (0 == access(path.toUtf8().constData(), F_OK)) {
				/* Try rename. */
				path = QString("%1%2%3#%4%5").arg(full_dir_path).arg(QDir::separator()).arg(layer->name).arg(ii, 3, 10, QChar('0')).arg(extension);
			} else {
				safe = true;
				break;
			}
			ii++;
		}
		if (ii == 5000) {
			success = false;
		}

		/* We allow exporting empty layers. */
		if (safe) {
			bool this_success = a_file_export_layer((LayerTRW *) layer, path, file_type, true);

			/* Show some progress. */
			if (this_success) {
				export_count++;
				this->status_bar->set_message(StatusBarField::INFO, QString("Exporting to file: %1").arg(path));
#ifdef K
				while (gtk_events_pending()) {
					gtk_main_iteration();
				}
#endif
			}

			success = success && this_success;
		}
	}

	this->clear_busy_cursor();

	/* Confirm what happened. */
	this->status_bar->set_message(StatusBarField::INFO, QString("Exported files: %1").arg(export_count));

	return success;
}




void Window::export_to_common(SGFileType file_type, char const * extension)
{
	std::list<Layer const *> * layers = this->items_tree->get_all_layers_of_type(LayerType::TRW, true);

	if (!layers || layers->empty()) {
		Dialog::info(tr("Nothing to Export!"), this);
		/* kamilFIXME: delete layers? */
		return;
	}

	QFileDialog file_selector(this, QObject::tr("Export to directory"));
	file_selector.setFileMode(QFileDialog::Directory);
	file_selector.setAcceptMode(QFileDialog::AcceptSave);

#ifdef K
	gtk_window_set_transient_for(file_selector, this);
	gtk_window_set_destroy_with_parent(file_selector, true);
	gtk_window_set_modal(file_selector, true);
	gtk_widget_show_all(file_selector);
#endif

	if (QDialog::Accepted != file_selector.exec()) {
		/* kamilFIXME: delete layers? */
		return;
	}

	QStringList selection = file_selector.selectedFiles();
	if (!selection.size()) {
		/* kamilFIXME: delete layers? */
		return;
	}

	const QString full_dir_path = selection.at(0);
	if (!this->export_to(layers, file_type, full_dir_path, extension)) {
		Dialog::error(tr("Could not convert all files"), this);
	}

	delete layers;
}




void Window::menu_file_properties_cb(void)
{
	if (this->current_document_full_path.isEmpty()) {
		Dialog::info(tr("No Viking File"), this);
		return;
	}

	if (0 != access(this->current_document_full_path.toUtf8().constData(), F_OK)) {
		Dialog::info(tr("File not accessible"), this);
		return;
	}

	/* Get some timestamp information of the file. */
	struct stat stat_buf;
	if (0 != stat(this->current_document_full_path.toUtf8().constData(), &stat_buf)) {
		Dialog::info(tr("File not accessible"), this);
		return;
	}

	char time_buf[64];
	strftime(time_buf, sizeof(time_buf), "%c", gmtime((const time_t *)&stat_buf.st_mtime));
	const QString size_string = Measurements::get_file_size_string(stat_buf.st_size);
	const QString message = QObject::tr("%1\n\n%2\n\n%3").arg(this->current_document_full_path).arg(time_buf).arg(size_string);

	Dialog::info(message, this);
}




bool Window::menu_file_save_and_exit_cb(void)
{
	if (this->menu_file_save_cb()) {
		this->close();
		return(true);
	} else {
		return(false);
	}
}




bool Window::window_save()
{
	this->set_busy_cursor();
	bool success = true;

	if (a_file_save(this->items_tree->get_top_layer(), this->viewport, this->current_document_full_path.toUtf8().constData())) {
		this->update_recently_used_document(this->current_document_full_path);
	} else {
		Dialog::error(tr("The filename you requested could not be opened for writing."), this);
		success = false;
	}
	this->clear_busy_cursor();
	return success;
}




void Window::import_kmz_file_cb(void)
{
	QFileDialog file_selector(this, QObject::tr("Open File"));
	file_selector.setFileMode(QFileDialog::ExistingFile);
	/* AcceptMode is QFileDialog::AcceptOpen by default. */;

	/* TODO: make sure that "all" is the default filter. */
	QStringList mime;
	mime << "application/octet-stream"; /* "All files (*)" */
	mime << "vnd.google-earth.kmz";     /* "KMZ" / "*.kmz"; */
	file_selector.setMimeTypeFilters(mime);

	if (QDialog::Accepted != file_selector.exec())  {
		return;
	}

	QStringList selection = file_selector.selectedFiles();
	if (!selection.size()) {
		return;
	}
	const QString full_path = selection.at(0);

	/* TODO convert ans value into readable explaination of failure... */
	int ans = kmz_open_file(full_path.toUtf8().constData(), this->viewport, this->items_tree);
	if (ans) {
		Dialog::error(tr("Unable to import %1.").arg(full_path), this);
	}

	this->draw_update();
}




Window * Window::new_window_cb(void)
{
	return new_window();
}




Window * Window::new_window()
{
	if (window_count >= MAX_WINDOWS) {
		return NULL;
	}

	Window * window = new Window();

#ifdef K
	QObject::connect(window, SIGNAL("destroy"), NULL, SLOT (destroy_window_cb));
	QObject::connect(window, SIGNAL("newwindow"), NULL, SLOT (new_window_cb));
	QObject::connect(window, SIGNAL("openwindow"), NULL, SLOT (open_window));
#endif

	if (Preferences::get_restore_window_state()) {
		/* These settings are applied after the show all as these options hide widgets. */
		bool visibility;

		if (a_settings_get_boolean(VIK_SETTINGS_WIN_SIDEPANEL, &visibility)) {
			window->show_side_panel(visibility);
		}

		if (a_settings_get_boolean(VIK_SETTINGS_WIN_STATUSBAR, &visibility)) {
#ifdef K
			window->view_statusbar_cb(visibility);
#endif
		}

		if (a_settings_get_boolean(VIK_SETTINGS_WIN_TOOLBAR, &visibility)) {
#ifdef K
			gtk_widget_hide(toolbar_get_widget(window->viking_vtb));
#endif
		}

		if (a_settings_get_boolean(VIK_SETTINGS_WIN_MENUBAR, &visibility)) {
			window->view_main_menu_cb(visibility);
		}
	}

	window_count++;

	return window;
}




void Window::menu_view_cache_info_cb(void)
{
	const size_t bytes = map_cache_get_size();
	const QString size_string = Measurements::get_file_size_string(bytes);
	const QString msg = QString("Map Cache size is %1 with %2 items").arg(size_string).arg(map_cache_get_count());

	Dialog::info(msg, this);
}




void Window::preferences_change_update(void)
{
	/* Want to update all TRW layers. */
	std::list<Layer const *> * layers = this->items_tree->get_all_layers_of_type(LayerType::TRW, true);
	if (!layers || layers->empty()) {
		return;
	}

	for (auto iter = layers->begin(); iter != layers->end(); iter++) {
		/* Reset the individual waypoints themselves due to the preferences change. */
		LayerTRW * trw = (LayerTRW *) *iter;
		trw->reset_waypoints();
	}

	delete layers;

	this->draw_update();
}




void Window::destroy_window_cb(void)
{
	window_count--;
}





#define VIKING_ACCELERATOR_KEY_FILE "keys.rc"
/**
 * Used to handle keys pressed in main UI, e.g. as hotkeys.
 *
 * This is the global key press handler
 *  Global shortcuts are available at any time and hence are not restricted to when a certain tool is enabled
 */
bool Window::key_press_event_cb(QKeyEvent * event)
{
#ifdef K
	// The keys handled here are not in the menuing system for a couple of reasons:
	//  . Keeps the menu size compact (alebit at expense of discoverably)
	//  . Allows differing key bindings to perform the same actions

	// First decide if key events are related to the maps layer
	bool map_download = false;
	bool map_download_only_new = true; // Only new or reload

	GdkModifierType modifiers = (GdkModifierType) gtk_accelerator_get_default_mod_mask();

	// Standard 'Refresh' keys: F5 or Ctrl+r
	// Note 'F5' is actually handled via menu_view_refresh_cb() later on
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
		this->viewport->zoom_in();
		this->draw_update();
		return true; // handled keypress
	}
	else if (ev->keyval == GDK_KEY_KP_Subtract && (ev->state & modifiers) == GDK_CONTROL_MASK) {
		this->viewport->zoom_out();
		this->draw_update();
		return true; // handled keypress
	}

	if (map_download) {
		this->simple_map_update(map_download_only_new);
		return true; // handled keypress
	}

	Layer * layer = this->items_tree->get_selected_layer();
	if (layer && this->tb->active_tool && this->tb->active_tool->key_press) {
		LayerType ltype = this->tb->active_tool->layer_type;
		if (layer && ltype == layer->type) {
			return this->tb->active_tool->key_press(layer, ev, this->tb->active_tool);
		}
	}

	// Ensure called only on window tools (i.e. not on any of the Layer tools since the layer is NULL)
	if (this->current_tool < TOOL_LAYER) {
		// No layer - but enable window tool keypress processing - these should be able to handle a NULL layer
		if (this->tb->active_tool->key_press) {
			return this->tb->active_tool->key_press(layer, ev, this->tb->active_tool);
		}
	}

	/* Restore Main Menu via Escape key if the user has hidden it */
	/* This key is more likely to be used as they may not remember the function key */
	if (ev->keyval == GDK_Escape) {
		GtkWidget *check_box = gtk_ui_manager_get_widget(this->uim, "/ui/MainMenu/View/SetShow/ViewMainMenu");
		if (check_box) {
			bool state = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(check_box));
			if (!state) {
				gtk_widget_show(gtk_ui_manager_get_widget(this->uim, "/ui/MainMenu"));
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_box), true);
				return true; /* handled keypress */
			}
		}
	}
#endif
	return false; /* don't handle the keypress */
}





enum {
	TARGET_URIS,
};
void Window::drag_data_received_cb(GtkWidget * widget, GdkDragContext *context, int x, int y, GtkSelectionData * selection_data, unsigned int target_type, unsigned int time)
{
	bool success = false;
#ifdef K
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
#endif
}




#if 0   /* Remnants of window_gtk.cpp */




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
			/* GSList & contents are freed by main.open_window. */
		} else {
			window->open_file(path, true);
			free(path);
		}
	}

	free(filename);
}




/* TODO - add method to add tool icons defined from outside this file,
   and remove the reverse dependency on icon definition from this file. */
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




#endif
