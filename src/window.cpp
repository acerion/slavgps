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




#if HAVE_UNISTD_H
#include <unistd.h>
#endif

/* stat() */
#include <sys/types.h>
#include <sys/stat.h>




#include <QtWidgets>
#include <QFileDialog>




#include "generic_tools.h"
#include "window.h"
#include "viewport.h"
#include "viewport_zoom.h"
#include "viewport_internal.h"
#include "viewport_to_image.h"
#include "layer.h"
#include "layer_defaults.h"
#include "layers_panel.h"
#include "toolbox.h"
#include "layer_trw.h"
#include "layer_aggregate.h"
#include "layer_map.h"
#include "ui_builder.h"
#include "application_state.h"
#include "background.h"
#include "dialog.h"
#include "util.h"
#include "vikutils.h"
#include "file.h"
#include "file_utils.h"
#include "datasources.h"
#include "goto.h"
#include "print.h"
#include "kmz.h"
#include "external_tools.h"
#include "external_tool_datasources.h"
#include "preferences.h"
#include "clipboard.h"
#include "map_cache.h"
#include "tree_view_internal.h"
#include "measurements.h"
#include "garmin_symbols.h"
#include "statusbar.h"




using namespace SlavGPS;




#define SG_MODULE "Window"




/* This seems rather arbitary, quite large and pointless.
   I mean, if you have a thousand windows open;
   why not be allowed to open a thousand more... */
#define MAX_WINDOWS 1024
static unsigned int window_count = 0;
static std::list<Window *> window_list;

#define VIKING_WINDOW_WIDTH      400
#define VIKING_WINDOW_HEIGHT     300

#define WIN_MAIN_DOCK_MIN_WIDTH 50  /* Minimal usable width/height of dock. */


#define VIKING_ACCELERATOR_KEY_FILE "keys.rc"




enum {
	OPEN_FILE_IN_THE_SAME_WINDOW = 0,
	OPEN_FILE_IN_NEW_WINDOW = 1,
};





SelectedTreeItems g_selected;
static ThisApp g_this_app;




enum WindowPan {
	PAN_NORTH = 0,
	PAN_EAST,
	PAN_SOUTH,
	PAN_WEST
};




#define VIK_SETTINGS_WIN_SIDEPANEL  "window_sidepanel"
#define VIK_SETTINGS_WIN_TOOLS_DOCK "window_tools_dock"
#define VIK_SETTINGS_WIN_STATUSBAR  "window_statusbar"
#define VIK_SETTINGS_WIN_TOOLBAR    "window_toolbar"
/* Menubar setting to off is never auto saved in case it's accidentally turned off.
   It's not so obvious so to recover the menu visibility.
   Thus this value is for setting manually via editting the settings file directly. */
#define VIK_SETTINGS_WIN_MENUBAR "window_menubar"




class ZoomLevels {
public:
	/**
	 * @viking_scale: The initial scale level.
	 */
	static QMenu * create_zoom_submenu(const VikingScale & viking_scale, const QString & label, QMenu * parent);

	static double get_selected_scale(const QAction * qa)
	{
		const int qa_idx = qa->data().toInt(); /* tag::zoom_actions_qa_idx */
		qDebug() << SG_PREFIX_SLOT << "Selected new zoom" << qa->text() << "with QAction no." << qa_idx;
		return pow(2, qa_idx - 5);
	}

	static int qa_index_from_viking_scale(const VikingScale & viking_scale)
	{
		int qa_idx = 5 + round(log(viking_scale.get_x()) / log(2));

		/* Ensure value derived from mpp is in bounds of the menu. */
		if (qa_idx >= (int) ZoomLevels::actions.size()) {
			qa_idx = ZoomLevels::actions.size() - 1;
		}
		if (qa_idx < 0) {
			qa_idx = 0;
		}

		return qa_idx;
	}

private:
	static void init_once(void);
	static std::vector<QAction *> actions;
	static std::vector<QString> labels;

};
std::vector<QAction *> ZoomLevels::actions;
std::vector<QString> ZoomLevels::labels = {
	"0.031",   /* 2^-5 */
	"0.063",   /* 2^-4 */
	"0.125",   /* 2^-3 */
	"0.25",    /* 2^-2 */
	"0.5",     /* 2^-1 */
	"1",       /* 2^0  */
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
};




Window::Window()
{
	strcpy(this->type_string, "SlavGPS Main Window");
	g_this_app.set(this, this->items_tree, NULL);


	for (int i = 0; i < QIcon::themeSearchPaths().size(); i++) {
		qDebug() << SG_PREFIX_I << "XDG DATA FOLDER: " << QIcon::themeSearchPaths().at(i);
	}
	QIcon::setThemeName("default");
	qDebug() << SG_PREFIX_I << "Using icon theme " << QIcon::themeName();


	this->create_layout();
	this->create_actions();
	this->toolbox = new Toolbox(this);
	this->create_ui();
	g_this_app.set(this, this->items_tree, this->main_gis_vp); /* Calling it again, this time with non-NULL layers panel and viewport. */


	connect(this->main_gis_vp, SIGNAL (list_of_center_coords_changed(GisViewport *)), this, SLOT (center_changed_cb(GisViewport *)));
	connect(this->main_gis_vp, SIGNAL (center_coord_or_zoom_changed(GisViewport *)), this, SLOT (draw_tree_items_cb(GisViewport *)));
	connect(this->main_gis_vp, SIGNAL (size_changed(ViewportPixmap *)), this, SLOT (draw_tree_items_cb(ViewportPixmap *)));
	connect(this->items_tree, SIGNAL (items_tree_updated()), this, SLOT (draw_tree_items_cb()));


	this->pan_pos = ScreenPos(-1, -1);  /* -1: off */
	this->set_current_document_full_path("");



#ifdef K_FIXME_RESTORE
	QObject::connect(this, SIGNAL("delete_event"), NULL, SLOT (delete_event));


	// Signals from GTK
	QObject::connect(this->main_gis_vp, SIGNAL("expose_event"), this, SLOT (draw_sync_cb));
	QObject::connect(this->main_gis_vp, SIGNAL(size_changed((ViewportPixmap *)), this, SLOT (window_configure_event));
	gtk_widget_add_events(this->main_gis_vp, GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK);

	/* This signal appears to be already handled by GisViewport::wheelEvent(). */
	QObject::connect(this->main_gis_vp, SIGNAL("scroll_event"), this, SLOT (draw_scroll_cb));

	QObject::connect(this->main_gis_vp, SIGNAL("button_press_event"), this, SLOT (draw_click_cb(QMouseEvent *)));
	QObject::connect(this->main_gis_vp, SIGNAL("button_release_event"), this, SLOT (draw_release_cb(QMouseEvent *)));

	QObject::connect(this->layers_tree, SIGNAL("delete_child_item"), this, SLOT (vik_window_clear_highlight_cb));


#endif
	Background::add_window(this);

	window_list.push_front(this);

	int height = VIKING_WINDOW_HEIGHT;
	int width = VIKING_WINDOW_WIDTH;

	if (Preferences::get_restore_window_state()) {

		QSize available_size = qApp->desktop()->availableGeometry().size();
		const int available_width = available_size.width();
		const int available_height = available_size.height();

		if (ApplicationState::get_integer(VIK_SETTINGS_WIN_HEIGHT, &height)) {
			/* Enforce a basic minimum size. */
			if (height < VIKING_WINDOW_HEIGHT) {
				height = VIKING_WINDOW_HEIGHT;
			}
		} else {
			/* No setting, so use default. */
			height = VIKING_WINDOW_HEIGHT;
		}
		if (height > available_height) {
			height = available_height;
		}

		if (ApplicationState::get_integer(VIK_SETTINGS_WIN_WIDTH, &width)) {
			/* Enforce a basic minimum size. */
			if (width < VIKING_WINDOW_WIDTH) {
				width = VIKING_WINDOW_WIDTH;
			}
		} else {
			/* No setting, so use default. */
			width = VIKING_WINDOW_WIDTH;
		}
		if (width > available_width) {
			width = available_width;
		}

		this->setGeometry(0, 0, width, height);



		bool maxed;
		if (ApplicationState::get_boolean(VIK_SETTINGS_WIN_MAX, &maxed)) {
			if (maxed) {
				this->showMaximized();
			}
		}

		bool full;
		if (ApplicationState::get_boolean(VIK_SETTINGS_WIN_FULLSCREEN, &full)) {
			if (full) {
				this->set_full_screen_state_cb(true);
			}
		}

		int size = 0;
		if (ApplicationState::get_integer(VIK_SETTINGS_WIN_MAIN_DOCK_SIZE, &size) && size > WIN_MAIN_DOCK_MIN_WIDTH) {

			/* Adjust geometry of panel dock.

			   TODO_LATER: this doesn't work. Either
			   setGeometry() doesn't work, or something
			   overwrites panel's size. */

			QRect geometry = this->layers_panel_dock->geometry();
			const Qt::DockWidgetArea area = this->dockWidgetArea(this->layers_panel_dock);
			switch (area) {
			case Qt::LeftDockWidgetArea:
			case Qt::RightDockWidgetArea:
				geometry.setWidth(size);
				qDebug() << SG_PREFIX_I << "Restoring window panel width" << size;
				this->layers_panel_dock->setGeometry(geometry);
				break;
			case Qt::TopDockWidgetArea:
			case Qt::BottomDockWidgetArea:
				geometry.setHeight(size);
				qDebug() << SG_PREFIX_I << "Restoring window panel height" << size;
				this->layers_panel_dock->setGeometry(geometry);
				break;
			default:
				break;
			}
		}
	}

	/* Drag and Drop of files are accepted only onto the viewport, nowhere else */
	this->main_gis_vp->setAcceptDrops(true);


	/* Set the default tool + mode. */
	this->toolbox->activate_tool_by_id(LayerToolPan::tool_id());
	this->qa_drawmode_mercator->setChecked(true);

	/* Set the application icon. */
	this->setWindowIcon(QIcon(":/icons/application.png"));


	qDebug() << SG_PREFIX_I << "final panel geometry" << this->layers_panel_dock->geometry();
}




Window::~Window()
{
	Background::remove_window(this);

	window_list.remove(this);
	delete this->main_gis_vp;
	delete this->items_tree;
}




void Window::create_layout()
{
	this->toolbar = new QToolBar(tr("Main Toolbar"));
	this->addToolBar(this->toolbar);


	this->main_gis_vp = new GisViewport(90, 60, 30, 120, this);
	qDebug() << SG_PREFIX_I << "Created Viewport with center's size:" << this->main_gis_vp->central_get_height() << this->main_gis_vp->central_get_width();
	this->setCentralWidget(this->main_gis_vp);


	this->layers_panel_dock = new QDockWidget(tr("Layers"), this);
	this->layers_panel_dock->setAllowedAreas(Qt::AllDockWidgetAreas);

	this->items_tree = new LayersPanel(this->layers_panel_dock, this);

	this->layers_panel_dock->setWidget(this->items_tree);
	this->addDockWidget(Qt::LeftDockWidgetArea, this->layers_panel_dock);


	this->tools_dock = new QDockWidget(tr("Tools"), this);
	this->tools_dock->setAllowedAreas(Qt::AllDockWidgetAreas);
	this->addDockWidget(Qt::RightDockWidgetArea, this->tools_dock);


	setStyleSheet("QMainWindow::separator { image: url(src/icons/handle_indicator.png); width: 8}");


	this->status_bar = new StatusBar(this);
	this->setStatusBar(this->status_bar);


	return;
}




void Window::create_actions(void)
{
	this->menu_file = new QMenu(tr("&File"));
	this->menu_edit = new QMenu(tr("&Edit"));
	this->menu_view = new QMenu(tr("&View"));
	this->menu_layers = new QMenu(tr("&Layers"));
	this->menu_tools = new QMenu(tr("&Tools"));
	this->menu_help = new QMenu(tr("&Help"));

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
		qa_file_new->setToolTip(tr("Open a file"));
		connect(qa_file_open, SIGNAL (triggered(bool)), this, SLOT (new_window_cb()));

		qa_file_open = this->menu_file->addAction(QIcon::fromTheme("document-open"), tr("&Open..."));
		qa_file_open->setShortcut(Qt::CTRL + Qt::Key_O);
		qa_file_open->setData(QVariant((int) OPEN_FILE_IN_NEW_WINDOW));
		connect(qa_file_open, SIGNAL (triggered(bool)), this, SLOT (open_file_cb()));

		/* This submenu will be populated by Window::update_recent_files(). */
		this->submenu_recent_files = this->menu_file->addMenu(tr("Open &Recent File"));

		qa = this->menu_file->addAction(QIcon::fromTheme("list-add"), tr("Append &File..."));
		qa->setData(QVariant((int) OPEN_FILE_IN_THE_SAME_WINDOW));
		qa->setToolTip(tr("Append data from a different file"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (open_file_cb()));

		qa = this->menu_file->addAction(tr("&Save"));
		qa->setIcon(QIcon::fromTheme("document-save"));
		qa->setToolTip(tr("Save the file"));
		qa->setShortcut(Qt::CTRL + Qt::Key_S);
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_file_save_cb()));

		qa = this->menu_file->addAction(tr("Save &As..."));
		qa->setIcon(QIcon::fromTheme("document-save-as"));
		qa->setToolTip(tr("Save the file under different name"));
		qa->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_S);
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_file_save_as_cb()));

		qa = this->menu_file->addAction(tr("Properties..."));
		qa->setToolTip(tr("Properties of current file"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_file_properties_cb()));


		this->menu_file->addSeparator();


		this->submenu_file_acquire = this->menu_file->addMenu(tr("A&cquire"));

		{
			qa = this->submenu_file_acquire->addAction(tr("From &GPS..."));
			qa->setToolTip(tr("Transfer data from a GPS device"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_gps_cb(void)));

			qa = this->submenu_file_acquire->addAction(tr("From &File (With GPSBabel)..."));
			qa->setToolTip(tr("Import File With GPSBabel..."));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_file_cb(void)));

			qa = this->submenu_file_acquire->addAction(tr("&Directions..."));
			qa->setToolTip(tr("Get driving directions"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_routing_cb(void)));

			qa = this->submenu_file_acquire->addAction(tr("Import Geo&JSON File..."));
			qa->setToolTip(tr("Import GeoJSON file"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_geojson_cb(void)));

			qa = this->submenu_file_acquire->addAction(tr("&OSM Traces..."));
			qa->setToolTip(tr("Get traces from OpenStreetMap"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_osm_cb(void)));

			qa = this->submenu_file_acquire->addAction(tr("&My OSM Traces..."));
			qa->setToolTip(tr("Get Your Own Traces from OpenStreetMap"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_my_osm_cb(void)));

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
			qa = this->submenu_file_acquire->addAction(tr("Import KMZ &Map File..."));
			qa->setToolTip(tr("Import a KMZ file"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_kmz_file_cb(void)));
#endif

			ExternalToolDataSource::add_menu_items(this->submenu_file_acquire, this->main_gis_vp);
		}


		QMenu * submenu_file_export = this->menu_file->addMenu(tr("&Export All"));
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
		qa->setToolTip(tr("Print"));


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


		qa = new QAction(tr("Copy Centre &Location"), this);
		qa->setIcon(QIcon::fromTheme(""));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_copy_centre_cb(void)));
		qa->setShortcut(Qt::CTRL + Qt::Key_H);
		this->menu_edit->addAction(qa);

		qa = new QAction(tr("&Flush Map Cache"), this);
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (map_cache_flush_cb(void)));
		this->menu_edit->addAction(qa);

		qa = new QAction(tr("&Set Current as the Default Location"), this);
		qa->setIcon(QIcon::fromTheme("go-next"));
		qa->setToolTip(tr("Save current position as the Default Location"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (set_default_location_cb(void)));
		this->menu_edit->addAction(qa);

		qa = new QAction(tr("&Preferences"), this);
		qa->setIcon(QIcon::fromTheme("preferences-other"));
		qa->setToolTip(tr("Program Preferences"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (preferences_cb(void)));
		this->menu_edit->addAction(qa);

		{
			QMenu * defaults_submenu = this->menu_edit->addMenu(QIcon::fromTheme("document-properties"), tr("&Layer Defaults"));

			for (LayerKind layer_kind = LayerKind::Aggregate; layer_kind < LayerKind::Max; ++layer_kind) {
				qa = defaults_submenu->addAction("&" + Layer::get_translated_layer_kind_string(layer_kind) + "...");
				qa->setData(QVariant((int) layer_kind));
				qa->setIcon(Layer::get_interface(layer_kind)->action_icon);
				/* Layer Defaults should be already initialized. If they aren't, it's because of runtime errors. */
				qa->setEnabled(LayerDefaults::is_initialized());
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

		this->qa_drawmode_utm = new QAction(GisViewportDrawModes::get_label_with_accelerator(GisViewportDrawMode::UTM), this);
		this->qa_drawmode_utm->setData(QVariant((int) GisViewportDrawMode::UTM));
		this->qa_drawmode_utm->setCheckable(true);
		group->addAction(this->qa_drawmode_utm);
		this->menu_view->addAction(this->qa_drawmode_utm);

#ifdef VIK_CONFIG_EXPEDIA
		this->qa_drawmode_expedia = new QAction(GisViewportDrawModes::get_label_with_accelerator(GisViewportDrawMode::Expedia), this);
		this->qa_drawmode_expedia->setData(QVariant((int) GisViewportDrawMode::Expedia));
		this->qa_drawmode_expedia->setCheckable(true);
		group->addAction(this->qa_drawmode_expedia);
		this->menu_view->addAction(this->qa_drawmode_expedia);
#endif

		this->qa_drawmode_mercator = new QAction(GisViewportDrawModes::get_label_with_accelerator(GisViewportDrawMode::Mercator), this);
		this->qa_drawmode_mercator->setData(QVariant((int) GisViewportDrawMode::Mercator));
		this->qa_drawmode_mercator->setCheckable(true);
		group->addAction(this->qa_drawmode_mercator);
		this->menu_view->addAction(this->qa_drawmode_mercator);

		this->qa_drawmode_latlon = new QAction(GisViewportDrawModes::get_label_with_accelerator(GisViewportDrawMode::LatLon), this);
		this->qa_drawmode_latlon->setData(QVariant((int) GisViewportDrawMode::LatLon));
		this->qa_drawmode_latlon->setCheckable(true);
		group->addAction(this->qa_drawmode_latlon);
		this->menu_view->addAction(this->qa_drawmode_latlon);

		connect(group, SIGNAL (triggered(QAction *)), this, SLOT (change_coord_mode_cb(QAction *)));


		this->menu_view->addSeparator();


		qa = new QAction(tr("Go to the &Default Location"), this);
		qa->setToolTip(tr("Go to the default location"));
		qa->setIcon(QIcon::fromTheme("go-home"));
		this->menu_view->addAction(qa);
		connect(qa, SIGNAL(triggered(bool)), this, SLOT(goto_default_location_cb(void)));


		qa = new QAction(tr("Go to &Location..."), this);
		qa->setToolTip(tr("Go to address/place using text search"));
		qa->setIcon(QIcon::fromTheme("go-jump"));
		this->menu_view->addAction(qa);
		connect(qa, SIGNAL(triggered(bool)), this, SLOT(goto_location_cb(void)));


		qa = new QAction(tr("&Go to Lat/Lon..."), this);
		qa->setToolTip(tr("Go to arbitrary lat/lon coordinate"));
		qa->setIcon(QIcon::fromTheme("go-jump"));
		this->menu_view->addAction(qa);
		connect(qa, SIGNAL(triggered(bool)), this, SLOT(goto_latlon_cb(void)));


		qa = new QAction(tr("Go to UTM..."), this);
		qa->setToolTip(tr("Go to arbitrary UTM coordinate"));
		qa->setIcon(QIcon::fromTheme("go-jump"));
		this->menu_view->addAction(qa);
		connect(qa, SIGNAL(triggered(bool)), this, SLOT(goto_utm_cb(void)));


		this->qa_previous_location = new QAction(tr("Go to the Pre&vious Location"), this);
		this->qa_previous_location->setToolTip(tr("Go to the previous location"));
		this->qa_previous_location->setIcon(QIcon::fromTheme("go-previous"));
		this->qa_previous_location->setEnabled(false); /* At the beginning there is no "previous location" to go to. */
		this->menu_view->addAction(this->qa_previous_location);
		connect(this->qa_previous_location, SIGNAL(triggered(bool)), this, SLOT(goto_previous_location_cb(void)));


		this->qa_next_location = new QAction(tr("Go to the &Next Location"), this);
		this->qa_next_location->setToolTip(tr("Go to the next location"));
		this->qa_next_location->setIcon(QIcon::fromTheme("go-next"));
		this->qa_next_location->setEnabled(false); /* At the beginning there is no "next location" to go to. */
		this->menu_view->addAction(this->qa_next_location);
		connect(this->qa_next_location, SIGNAL(triggered(bool)), this, SLOT(goto_next_location_cb(void)));


		this->menu_view->addSeparator();


		qa = new QAction(tr("&Refresh"), this);
		qa->setShortcut(Qt::Key_F5);
		qa->setToolTip(tr("Refresh any maps displayed"));
		qa->setIcon(QIcon::fromTheme("view-refresh"));
		this->menu_view->addAction(qa);
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_view_refresh_cb(void)));


		this->menu_view->addSeparator();


		qa = new QAction(tr("Set &Highlight Color..."), this);
		qa->setToolTip(tr("Set Highlight Color"));
		this->menu_view->addAction(qa);
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_view_set_highlight_color_cb(void)));


		qa = new QAction(tr("Set Bac&kground Color..."), this);
		qa->setToolTip(tr("Set Background Color"));
		this->menu_view->addAction(qa);
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_view_set_bg_color_cb(void)));


		this->qa_view_full_screen_state = new QAction(tr("&Full Screen"), this);
		this->qa_view_full_screen_state->setShortcut(Qt::Key_F11);
		this->qa_view_full_screen_state->setCheckable(true);
		this->qa_view_full_screen_state->setChecked(false); /* Final value of this checkbox will be set by code restoring window state. */
		this->qa_view_full_screen_state->setToolTip(tr("Activate full screen mode"));
		this->qa_view_full_screen_state->setIcon(QIcon::fromTheme("view-fullscreen"));
		this->menu_view->addAction(this->qa_view_full_screen_state);
		connect(this->qa_view_full_screen_state, SIGNAL (triggered(bool)), this, SLOT (set_full_screen_state_cb(bool)));


		QMenu * show_submenu = new QMenu(tr("&Show"), this);
		this->menu_view->addMenu(show_submenu);

		this->qa_view_scale_visibility = new QAction(tr("Show &Scale"), this);
		this->qa_view_scale_visibility->setShortcut(Qt::SHIFT + Qt::Key_F5);
		this->qa_view_scale_visibility->setCheckable(true);
		this->qa_view_scale_visibility->setChecked(true);
		this->qa_view_scale_visibility->setToolTip(tr("Show Scale"));
		show_submenu->addAction(this->qa_view_scale_visibility);
		connect(this->qa_view_scale_visibility, SIGNAL (triggered(bool)), this, SLOT (set_scale_visibility_cb(bool)));


		this->qa_view_center_mark_visibility = new QAction(tr("Show &Center Mark"), this);
		this->qa_view_center_mark_visibility->setShortcut(Qt::Key_F6);
		this->qa_view_center_mark_visibility->setCheckable(true);
		this->qa_view_center_mark_visibility->setChecked(true);
		this->qa_view_center_mark_visibility->setToolTip(tr("Show Center Mark"));
		show_submenu->addAction(this->qa_view_center_mark_visibility);
		connect(this->qa_view_center_mark_visibility, SIGNAL (triggered(bool)), this, SLOT (set_center_mark_visibility_cb(bool)));


		this->qa_view_highlight_usage = new QAction(tr("Use &Highlighting"), this);
		this->qa_view_highlight_usage->setShortcut(Qt::Key_F7);
		this->qa_view_highlight_usage->setCheckable(true);
		this->qa_view_highlight_usage->setChecked(true);
		this->qa_view_highlight_usage->setToolTip(tr("Use highlighting when drawing selected items"));
		show_submenu->addAction(this->qa_view_highlight_usage);
		connect(this->qa_view_highlight_usage, SIGNAL (triggered(bool)), this, SLOT (set_highlight_usage_cb(bool)));


		qa = this->layers_panel_dock->toggleViewAction(); /* Existing action! */
		qa->setText(tr("Show Side &Panel"));
		qa->setShortcut(Qt::Key_F9);
		qa->setCheckable(true);
		qa->setChecked(this->layers_panel_dock->isVisible());
		qa->setToolTip(tr("Show Side Panel"));
		show_submenu->addAction(qa);
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (set_side_panel_visibility_cb(bool)));


		qa = this->tools_dock->toggleViewAction(); /* Existing action! */
		qa->setText(tr("Show Tools Panel"));
		//qa->setShortcut(Qt::Key_F9);
		qa->setCheckable(true);
		qa->setChecked(this->tools_dock->isVisible());
		qa->setToolTip(tr("Show Tools Panel"));
		show_submenu->addAction(qa);
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (set_tools_dock_visibility_cb(bool)));


		qa = this->status_bar->toggleViewAction();
		qa->setText(tr("Show Status&bar"));
		qa->setShortcut(Qt::Key_F12);
		qa->setCheckable(true);
		qa->setChecked(this->status_bar->isVisible());
		qa->setToolTip(tr("Show Statusbar"));
		show_submenu->addAction(qa);
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (set_status_bar_visibility_cb(bool)));


		qa = this->toolbar->toggleViewAction(); /* This is already existing action! */
		qa->setText(tr("Show &Toolbar"));
		qa->setShortcut(Qt::Key_F3);
		qa->setCheckable(true);
		// qa->setChecked(this->toolbar->toggleViewAction()->isVisible());
		qa->setToolTip(tr("Show Toolbar"));
		/* No signal connection needed, we have toggleViewAction(). */
		show_submenu->addAction(qa);


		qa = new QAction(tr("Show &Menu"), this);
		qa->setShortcut(Qt::Key_F4);
		qa->setCheckable(true);
		qa->setChecked(true);
		qa->setToolTip(tr("Show Menu"));
		show_submenu->addAction(qa);
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (set_main_menu_visibility_cb(bool)));


		this->menu_view->addSeparator();


		qa_view_zoom_in = new QAction(tr("Zoom &In"), this);
		qa_view_zoom_in->setShortcut(Qt::CTRL + Qt::Key_Plus);
		qa_view_zoom_in->setIcon(QIcon::fromTheme("zoom-in"));
		connect(qa_view_zoom_in, SIGNAL (triggered(bool)), this, SLOT (zoom_cb(void)));
		this->menu_view->addAction(qa_view_zoom_in);

		qa_view_zoom_out = new QAction(tr("Zoom &Out"), this);
		qa_view_zoom_out->setShortcut(Qt::CTRL + Qt::Key_Minus);
		qa_view_zoom_out->setIcon(QIcon::fromTheme("zoom-out"));
		connect(qa_view_zoom_out, SIGNAL (triggered(bool)), this, SLOT (zoom_cb(void)));
		this->menu_view->addAction(qa_view_zoom_out);

		qa_view_zoom_to = new QAction(tr("Zoom &To..."), this);
		qa_view_zoom_to->setShortcut(Qt::CTRL + Qt::Key_Z);
		qa_view_zoom_to->setIcon(QIcon::fromTheme("zoom-fit-best"));
		connect(qa_view_zoom_to, SIGNAL (triggered(bool)), this, SLOT (zoom_to_cb(void)));
		this->menu_view->addAction(qa_view_zoom_to);


		QMenu * zoom_submenu = ZoomLevels::create_zoom_submenu(this->main_gis_vp->get_viking_scale(), tr("&Zoom"), this->menu_view);
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


		qa = new QAction(tr("Background &Jobs"), this);
		qa->setIcon(QIcon::fromTheme("emblem-system"));
		connect(qa, SIGNAL(triggered(bool)), this, SLOT(show_background_jobs_window_cb(void)));
		this->menu_view->addAction(qa);

#if 1           /* This is only for debugging purposes (or is it not?). */
		qa = new QAction(tr("Show Centers"), this);
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (show_centers_cb(void)));
		this->menu_view->addAction(qa);

		qa = new QAction(tr("&Map Cache Info"), this);
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (menu_view_cache_info_cb(void)));
		this->menu_view->addAction(qa);
#endif
	}



	/* "Layers" menu. */
	{
		this->qa_tree_item_properties = new QAction(tr("Properties..."), this);
		this->menu_layers->addAction(this->qa_tree_item_properties);
		connect(this->qa_tree_item_properties, SIGNAL (triggered(bool)), this->items_tree->get_tree_view(), SLOT (tree_item_properties_cb(void)));

		this->new_layers_submenu_add_actions(this->menu_layers);
	}


	/* "Help" menu. */
	{
		QAction * qa_help_help = new QAction(tr("&Help"), this);
		qa_help_help->setIcon(QIcon::fromTheme("help-contents"));
		qa_help_help->setShortcut(Qt::Key_F1);
		connect(qa_help_help, SIGNAL (triggered(bool)), this, SLOT (help_help_cb(void)));

		QAction * qa_help_about = new QAction(tr("&About"), this);
		qa_help_about->setIcon(QIcon::fromTheme("help-about"));
		connect(qa_help_about, SIGNAL (triggered(bool)), this, SLOT (help_about_cb(void)));

		this->menu_help->addAction(qa_help_help);
		this->menu_help->addAction(qa_help_about);
	}


	this->toolbar->addAction(qa_file_new);


	return;
}




void Window::draw_tree_items_cb(void)
{
	qDebug() << SG_PREFIX_SLOT;
	this->draw_tree_items(this->main_gis_vp);
}




void Window::draw_tree_items_cb(GisViewport * gisview)
{
	qDebug() << SG_PREFIX_SLOT;
	this->draw_tree_items(gisview);
}




void Window::draw_tree_items_cb(ViewportPixmap * vpixmap)
{
	qDebug() << SG_PREFIX_SLOT;
	GisViewport * gisview = (GisViewport *) vpixmap;
	this->draw_tree_items(gisview);
}




static void draw_sync_cb(Window * window)
{
	window->draw_sync();
}




void Window::draw_sync()
{
	qDebug() << SG_PREFIX_I << "Sync begin";
	//this->main_gis_vp->sync();
	qDebug() << SG_PREFIX_I << "Sync end";
	this->update_status_bar_on_redraw();
}




void Window::update_status_bar_on_redraw(void)
{
	const QString scale = this->main_gis_vp->get_viking_scale().pretty_print(this->main_gis_vp->get_coord_mode());

	qDebug() << SG_PREFIX_I << "Viking scale is" << scale;
	this->status_bar->set_message(StatusBarField::Zoom, scale);
	this->display_current_tool_name();
}




void Window::menu_layer_new_cb(void) /* Slot. */
{
	QAction * qa = (QAction *) QObject::sender();
	LayerKind layer_kind = (LayerKind) qa->data().toInt();

	qDebug() << SG_PREFIX_I << "Clicked \"layer new\" for layer kind" << Layer::get_translated_layer_kind_string(layer_kind);


	Layer * layer = Layer::construct_layer(layer_kind, this->main_gis_vp, true);
	if (layer) {
		this->items_tree->add_layer(layer, this->main_gis_vp->get_coord_mode());

		qDebug() << SG_PREFIX_I << "Calling layer->draw_tree_item() for new layer" << Layer::get_translated_layer_kind_string(layer_kind);
		layer->draw_tree_item(this->main_gis_vp, false, false);

		qDebug() << SG_PREFIX_I << "Will call draw_tree_items()";
		this->draw_tree_items(this->main_gis_vp);
		this->set_dirty_flag(true);
	}
}




void Window::draw_tree_items(GisViewport * gisview)
{
	qDebug() << "\n";
	qDebug() << SG_PREFIX_I;

#ifdef K_FIXME_RESTORE
	const Coord old_center = this->trigger_center;
	this->trigger_center = gisview->get_center_coord();
	Layer * new_trigger = this->trigger;
	this->trigger = NULL;
	Layer * old_trigger = gisview->get_trigger();

	if (!new_trigger) {
		; /* Do nothing -- have to redraw everything. */
	} else if ((old_trigger != new_trigger)
		   || (old_center != this->trigger_center)
		   || (new_trigger->type == LayerKind::Aggregate)) {
		gisview->set_trigger(new_trigger); /* todo: set to half_drawn mode if new trigger is above old */
	} else {
		gisview->set_half_drawn(true);
	}
#endif


	gisview->clear();

	/* Main layer drawing.  This is a standard drawing of items in
	   main viewport, so allow highlight. */
	this->items_tree->draw_tree_items(gisview, true, false);

	/* Other viewport decoration items on top if they are enabled/in use. */
	gisview->draw_decorations();

	if (1) { /* Debug. Draw on top of decorations. */
		gisview->debug_draw_debugs();
	}

	/* This will call GisViewport::paintEvent(), triggering final render to screen. */
	gisview->update();

	gisview->set_half_drawn(false); /* Just in case. */

	this->draw_sync();
}




void Window::draw_layer_cb(sg_uid_t uid) /* Slot. */
{
	qDebug() << SG_PREFIX_SLOT << "Will redraw all tree items after change made to layer with uid" << (qulonglong) uid;
	/* Only one layer has changed (e.g. its visibility was
	   toggled), which means that all tree items need to be
	   redrawn. */
	this->draw_tree_items(this->main_gis_vp);
}




void Window::handle_selection_of_tree_item(TreeItem & tree_item)
{
	/* Get either the selected layer itself, or an owner/parent of selected sublayer item. */
	const Layer * layer = tree_item.get_immediate_layer();
	qDebug() << SG_PREFIX_I << "Selected layer kind" << layer->get_translated_layer_kind_string();
	this->toolbox->activate_tools_group(layer->get_fixed_layer_kind_string());
}




GisViewport * Window::get_main_gis_view(void)
{
	return this->main_gis_vp;
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




QDockWidget * Window::get_tools_dock(void) const
{
	return this->tools_dock;
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




void Window::center_changed_cb(GisViewport * gisview) /* Slot. */
{
	qDebug() << SG_PREFIX_SLOT << "Called; back available?" << gisview->back_available() << "; forward available?" << gisview->forward_available();

	this->qa_previous_location->setEnabled(gisview->back_available());
	this->qa_next_location->setEnabled(gisview->forward_available());
}




QMenu * Window::get_layer_menu(QMenu * menu)
{
	menu->addAction(this->qa_tree_item_properties);
	return menu;
}




QMenu * Window::new_layers_submenu_add_actions(QMenu * menu)
{
	for (LayerKind layer_kind = LayerKind::Aggregate; layer_kind < LayerKind::Max; ++layer_kind) {

		const LayerInterface * iface = Layer::get_interface(layer_kind);

		QVariant variant((int) layer_kind);
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
	/* Menu Tools -> Online Services. */
	{
		QActionGroup * group = new QActionGroup(this);
		group->setObjectName("online_services");
		ExternalTools::add_action_items(group, this->main_gis_vp);

		QMenu * submenu_online_services = this->menu_tools->addMenu(tr("Online Services"));
		submenu_online_services->addActions(group->actions());
	}


	/* Menu Tools -> Generic tools;
	   Toolbar -> Generic Tools. */
	{
		LayerToolContainer tools = GenericTools::create_tools(this, this->main_gis_vp);

		QActionGroup * action_group = this->toolbox->add_tools(tools);
		if (action_group) {
			const QList <QAction *> actions = action_group->actions();

			if (!actions.isEmpty()) {
				action_group->setObjectName("generic");

				this->toolbar->addSeparator();
				this->toolbar->addActions(actions);

				this->menu_tools->addSeparator();
				this->menu_tools->addActions(actions);

				/* The same callback for all generic tools. */
				connect(action_group, SIGNAL(triggered(QAction *)), this, SLOT(layer_tool_cb(QAction *)));

				{
					/* We want some action in "generic tools"
					   group to be active by default. Let it be
					   the first tool in the group. */
					QAction * default_qa = actions.first();
					default_qa->setChecked(true);
					default_qa->trigger();

					QVariant property = default_qa->property("property_tool_id");
					const SGObjectTypeID default_tool_id = property.value<SGObjectTypeID>();
					this->toolbox->activate_tool_by_id(default_tool_id);
				}
			}
		} else {
			qDebug() << SG_PREFIX_E << "NULL generic tools group";
		}
	}


	/* Menu Tools -> layer-specific tools;
	   Toolbar -> layer-specific tools. */
	{
		for (LayerKind layer_kind = LayerKind::Aggregate; layer_kind < LayerKind::Max; ++layer_kind) {

			/* We can't build the layer tools when a layer
			   interface is constructed, because the layer
			   tools require Window and Viewport
			   variables, which may not be available at that time. */

			LayerToolContainer tools = Layer::get_interface(layer_kind)->create_tools(this, this->main_gis_vp);
			if (tools.empty()) {
				/* Either error, or given layer kind has no layer-specific tools. */
				continue;
			}

			QActionGroup * action_group = this->toolbox->add_tools(tools);
			if (!action_group) {
				qDebug() << SG_PREFIX_E << "NULL layer tools group";
				continue;
			}

			const QList<QAction *> actions = action_group->actions();

			if (!actions.isEmpty()) {
				action_group->setObjectName(Layer::get_fixed_layer_kind_string(layer_kind));

				this->toolbar->addSeparator();
				this->toolbar->addActions(actions);

				this->menu_tools->addSeparator();
				this->menu_tools->addActions(actions);

				action_group->setEnabled(false); /* A layer-specific tool group is disabled by default, until a specific layer is selected in tree view. */

				/* The same callback for all layer tools. */
				connect(action_group, SIGNAL (triggered(QAction *)), this, SLOT (layer_tool_cb(QAction *)));
			}
		}
	}

	Background::post_init_window(this);
}




/* Callback common for all layer tool actions. */
void Window::layer_tool_cb(QAction * qa)
{
	/* Handle old tool first. */
	this->toolbox->deactivate_current_tool();


	/* Now handle newly selected tool. */
	if (qa) {
		QVariant property = qa->property("property_tool_id");
		const SGObjectTypeID new_tool_id = property.value<SGObjectTypeID>();
		qDebug() << SG_PREFIX_I << "Setting 'release' cursor for tool" << new_tool_id;

		this->toolbox->activate_tool_by_id(new_tool_id);
		LayerTool * tool = this->toolbox->get_tool(new_tool_id);
		if (NULL == tool) {
			qDebug() << SG_PREFIX_E << "Failed to get tool with tool id =" << new_tool_id;
			/* Fallback cursor. */
			this->main_gis_vp->setCursor(QCursor(Qt::ArrowCursor));
		} else {
			this->main_gis_vp->setCursor(tool->cursor_release);
		}
		this->display_current_tool_name();
	}
}




void Window::activate_tool_by_id(const SGObjectTypeID & tool_id)
{
	this->toolbox->deactivate_current_tool();
	this->toolbox->activate_tool_by_id(tool_id);

	return;
}




void Window::pan_click(QMouseEvent * ev)
{
	qDebug() << SG_PREFIX_I;

	this->pan_move_in_progress = false;

	/* Set panning origin. */
	this->pan_pos = ScreenPos(ev->x(), ev->y());
}




void Window::pan_move(QMouseEvent * ev)
{
	//qDebug() << SG_PREFIX_I;
	if (this->pan_pos.x() != -1 && this->pan_pos.y() != -1) {
		this->pan_move_update_viewport(ev);

		this->pan_move_in_progress = true;
		this->pan_pos = ScreenPos(ev->x(), ev->y());
		this->main_gis_vp->request_redraw("pan move");
	}
}




/* Tell viewport that it needs to move by certain offset in reaction
   to pan action */
sg_ret Window::pan_move_update_viewport(const QMouseEvent * ev)
{
	const fpixel center_x = this->main_gis_vp->central_get_x_center_pixel();
	const fpixel center_y = this->main_gis_vp->central_get_y_center_pixel();

	/* By how much a center of viewport was moved by panning? */
	const int pan_delta_x = ev->x() - this->pan_pos.x();
	const int pan_delta_y = ev->y() - this->pan_pos.y();

	if (pan_delta_x != 0 || pan_delta_y != 0) {
		/* "Move a screen pixel that is delta x/y from center
		   of viewport, into a center of viewport. */
		return this->main_gis_vp->set_center_coord(center_x - pan_delta_x, center_y - pan_delta_y);
	} else {
		/* There was no movement. */
		return sg_ret::ok;
	}
}




void Window::pan_release(QMouseEvent * ev)
{
	qDebug() << SG_PREFIX_I;
	bool do_draw = true;

	if (this->pan_move_in_progress == false) {
		this->single_click_pending = !this->single_click_pending;
		if (this->single_click_pending) {
			/* Store offset to use. */
			this->delayed_pan_pos = this->pan_pos;

			/* Get double click time. */
			int interval = qApp->doubleClickInterval();

			/* Give chance for a double click to occur. Viking used +50 instead of *1.1. */
			interval *= 1.1;

#ifdef K_FIXME_RESTORE
			g_timeout_add(interval, (GSourceFunc) window_pan_timeout, this);
#endif
			do_draw = false;
		} else {
			this->main_gis_vp->set_center_coord(this->pan_pos);
		}
	} else {
		this->pan_move_update_viewport(ev);
	}

	this->pan_off();
	if (do_draw) {
		this->main_gis_vp->request_redraw("pan release");
	}
}




void Window::pan_off(void)
{
	this->pan_move_in_progress = false;
	this->pan_pos = ScreenPos(-1, -1);
	this->pan_pos.ry() = -1;
}




/**
   \brief Retrieve window's pan_move_in_progress

   Should be removed as soon as possible.

   \return window's 'pan move in progress' flag
*/
bool Window::get_pan_move_in_progress(void) const
{
	return this->pan_move_in_progress;
}




void Window::menu_edit_cut_cb(void)
{
	this->items_tree->cut_selected_cb();
	this->set_dirty_flag(true);
}




void Window::menu_edit_copy_cb(void)
{
	Clipboard::copy_selected(this->items_tree);
}




void Window::menu_edit_paste_cb(void)
{
	if (this->items_tree->paste_selected_cb()) {
		this->set_dirty_flag(true);
	}
}




void Window::menu_edit_delete_cb(void)
{
	if (this->items_tree->get_selected_layer()) {
		this->items_tree->delete_selected_cb();
		this->set_dirty_flag(true);
	} else {
		Dialog::info(tr("You must select a layer to delete."), this);
	}
}




void Window::menu_edit_delete_all_cb(void)
{
	/* Do nothing if empty. */
	if (0 == this->items_tree->get_top_layer()->get_child_layers_count()) {
		return;
	}

	if (Dialog::yes_or_no(tr("Are you sure you want to delete all layers?"), this)) {
		this->items_tree->clear();
		this->set_current_document_full_path("");

		/* TODO_LATER: verify if this call is needed, maybe removing
		   all tree items has triggered re-painting of
		   viewport? */
		this->draw_tree_items(this->main_gis_vp);
	}
}




void Window::menu_copy_centre_cb(void)
{
	QString first;
	QString second;

	const Coord coord = this->main_gis_vp->get_center_coord();

	bool full_format = false;
	(void) ApplicationState::get_boolean(VIK_SETTINGS_WIN_COPY_CENTRE_FULL_FORMAT, &full_format);

	QString message;
	if (full_format) {
		/* Bells & Whistles - may include degrees, minutes and second symbols. */
		message = coord.to_string();
	} else {
		/* Simple x.xx y.yy format. */
		message = coord.get_lat_lon().to_string();
	}

#if 0
	Pickle dummy;
	Clipboard::copy(ClipboardDataType::Text, LayerKind::Aggregate, SGObjectTypeID::any(), dummy, message);
#endif
}




void Window::map_cache_flush_cb(void)
{
	MapCache::flush();
}




void Window::set_default_location_cb(void)
{
	const LatLon current_center_lat_lon = this->main_gis_vp->get_center_coord().get_lat_lon();

	/* Push center coordinate values to Preferences */
	Preferences::set_param_value(QString(PREFERENCES_NAMESPACE_GENERAL "default_latitude"), SGVariant((double) current_center_lat_lon.lat));
	Preferences::set_param_value(QString(PREFERENCES_NAMESPACE_GENERAL "default_longitude"), SGVariant((double) current_center_lat_lon.lon));
	Preferences::save_to_file();
}




void Window::preferences_cb(void) /* Slot. */
{
	const bool orig_wp_icon_size = Preferences::get_use_large_waypoint_icons();

	Preferences::show_window(this);

	/* Has the waypoint size setting changed? */
	if (orig_wp_icon_size != Preferences::get_use_large_waypoint_icons()) {
		/* Delete icon indexing 'cache' and so automatically regenerates with the new setting when changed. */
		GarminSymbols::clear_symbols();

		/* Update all windows. */
		for (auto iter = window_list.begin(); iter != window_list.end(); iter++) {
			(*iter)->apply_new_preferences();
		}
	}

	/* Ensure TZ Lookup initialized. */
	if (Preferences::get_time_ref_frame() == SGTimeReference::World) {
		TZLookup::init();
	}
}




/*
  Return true if:
  - unsaved changes have been saved because end user has decided to save them,
  - unsaved changes are to be discarded because end user has decided to discard them,
  - there were no unsaved changes;

  Return false if:
  - user decided to cancel action that led to calling of this function
*/
bool Window::save_on_dirty_flag(void)
{
	if (!this->dirty_flag) {
		return true;
	}

	QMessageBox::StandardButton reply;
	reply = QMessageBox::question(this, QCoreApplication::applicationName(),
				      tr("Changes in file '%1' are not saved and will be lost if you don't save them.\n\n"
					 "Do you want to save the changes?").arg(this->get_current_document_file_name()),
				      QMessageBox::Cancel | QMessageBox::No | QMessageBox::Yes,
				      QMessageBox::Yes);
	if (reply == QMessageBox::No) {
		return true;
	} else if (reply == QMessageBox::Yes) {
		return this->menu_file_save_cb();
	} else {
		return false;
	}
}




void Window::closeEvent(QCloseEvent * ev)
{
	const bool proceed = this->save_on_dirty_flag();
	if (proceed) {
		ev->accept();
	} else {
		ev->ignore();
		return;
	}


	/* On the final window close - save latest state - if it's wanted... */
	if (window_count == 1) {
		if (Preferences::get_restore_window_state()) {

			const Qt::WindowStates states = this->windowState();

			bool state_max = states.testFlag(Qt::WindowMaximized);
			ApplicationState::set_boolean(VIK_SETTINGS_WIN_MAX, state_max);

			bool state_fullscreen = states.testFlag(Qt::WindowFullScreen);
			ApplicationState::set_boolean(VIK_SETTINGS_WIN_FULLSCREEN, state_fullscreen);

			ApplicationState::set_boolean(VIK_SETTINGS_WIN_SIDEPANEL,  this->layers_panel_dock->isVisible());
			ApplicationState::set_boolean(VIK_SETTINGS_WIN_TOOLS_DOCK, this->tools_dock->isVisible());
			ApplicationState::set_boolean(VIK_SETTINGS_WIN_STATUSBAR,  this->status_bar->isVisible());
			ApplicationState::set_boolean(VIK_SETTINGS_WIN_TOOLBAR,    this->toolbar->isVisible());

			/* If supersized - no need to save the enlarged width+height values. */
			if (!(state_fullscreen || state_max)) {
				qDebug() << SG_PREFIX_I << "Not saving window size";
				ApplicationState::set_integer(VIK_SETTINGS_WIN_WIDTH, this->width());
				ApplicationState::set_integer(VIK_SETTINGS_WIN_HEIGHT, this->height());
			}

			const QRect geometry = this->layers_panel_dock->geometry();
			int size = 0;
			const Qt::DockWidgetArea area = this->dockWidgetArea(this->layers_panel_dock);
			switch (area) {
			case Qt::LeftDockWidgetArea:
			case Qt::RightDockWidgetArea:
				size = geometry.width();
				break;
			case Qt::TopDockWidgetArea:
			case Qt::BottomDockWidgetArea:
				size = geometry.height();
				break;
			default:
				break;
			}

			if (size > 0) {
				qDebug() << SG_PREFIX_I << "Saving window's panel size" << size;
				ApplicationState::set_integer(VIK_SETTINGS_WIN_MAIN_DOCK_SIZE, size);
			}
		}
	}
}




void Window::goto_default_location_cb(void)
{
	const LatLon lat_lon = LatLon(Preferences::get_default_lat(), Preferences::get_default_lon());
	if (sg_ret::ok != this->main_gis_vp->set_center_coord(lat_lon)) {
		qDebug() << SG_PREFIX_E << "Failed to set center location from" << lat_lon;
		return;
	}
	this->main_gis_vp->request_redraw("go to default location");
}




void Window::goto_location_cb()
{
	if (GoTo::goto_location(this, this->main_gis_vp)) {
		this->main_gis_vp->request_redraw("go to location");
	}
}




void Window::goto_latlon_cb(void)
{
	if (sg_ret::ok != GoTo::goto_latlon(this, this->main_gis_vp)) {
		qDebug() << SG_PREFIX_E << "Failed to go to lat/lon";
		return;
	}
	this->main_gis_vp->request_redraw("go to latlon");
}




void Window::goto_utm_cb(void)
{
	if (sg_ret::ok != GoTo::goto_utm(this, this->main_gis_vp)) {
		qDebug() << SG_PREFIX_E << "Failed to go to lat/lon";
		return;
	}
	this->main_gis_vp->request_redraw("go to utm");
}




void Window::goto_previous_location_cb(void)
{
	const bool changed = this->main_gis_vp->go_back();
	qDebug() << SG_PREFIX_SLOT << "Called, going back has" << (changed ? "suceeded" : "failed");

	if (changed) {
		this->main_gis_vp->request_redraw("go to previous location");
	}
}




void Window::goto_next_location_cb(void)
{
	const bool changed = this->main_gis_vp->go_forward();
	qDebug() << SG_PREFIX_SLOT << "Called, going forward has" << (changed ? "suceeded" : "failed");
	if (changed) {
		this->main_gis_vp->request_redraw("go to next location");
	}
}




void Window::set_full_screen_state_cb(bool new_state)
{
	if (this->qa_view_full_screen_state->isChecked() != new_state) {

		this->qa_view_full_screen_state->setChecked(new_state);

		const Qt::WindowStates state = this->windowState();
		if (this->qa_view_full_screen_state->isChecked()) {
			this->setWindowState(state | Qt::WindowFullScreen);
		} else {
			this->setWindowState(state & (~Qt::WindowFullScreen));
		}
	}
}




void Window::set_scale_visibility_cb(bool new_state)
{
	if (this->qa_view_scale_visibility->isChecked() != new_state) {
		this->qa_view_scale_visibility->setChecked(new_state);
		this->main_gis_vp->set_scale_visibility(new_state);
		this->draw_tree_items(this->main_gis_vp);
	}
}




void Window::set_center_mark_visibility_cb(bool new_state)
{
	if (this->qa_view_center_mark_visibility->isChecked() != new_state) {
		this->qa_view_center_mark_visibility->setChecked(new_state);
		this->main_gis_vp->set_center_mark_visibility(new_state);
		this->draw_tree_items(this->main_gis_vp);
	}
}



void Window::set_highlight_usage_cb(bool new_state)
{
	if (this->qa_view_highlight_usage->isChecked() != new_state) {
		this->qa_view_highlight_usage->setChecked(new_state);
		this->main_gis_vp->set_highlight_usage(new_state);
		this->draw_tree_items(this->main_gis_vp);
	}
}




/**
   @reviewed on 2019-11-11
*/
void Window::set_side_panel_visibility_cb(bool new_state)
{
	if (this->layers_panel_dock->isVisible() != new_state) {
		qDebug() << SG_PREFIX_I << "Setting layers panel visibility to" << new_state;

		this->layers_panel_dock->setVisible(new_state);

		/* We need to set the qaction because this slot function
		   may be called like a regular function too. */
		this->layers_panel_dock->toggleViewAction()->setChecked(new_state);
	}
}




/**
   @reviewed on 2019-11-11
*/
void Window::set_tools_dock_visibility_cb(bool new_state)
{
	if (this->tools_dock->isVisible() != new_state) {
		qDebug() << SG_PREFIX_I << "Setting tools dock visibility to" << new_state;

		this->tools_dock->setVisible(new_state);

		/* We need to set the qaction because this slot function
		   may be called like a regular function too. */
		this->tools_dock->toggleViewAction()->setChecked(new_state);
	}
}




bool Window::get_side_panel_visibility(void) const
{
	return this->layers_panel_dock->isVisible();
}




bool Window::get_tools_dock_visibility(void) const
{
	return this->tools_dock->isVisible();
}




void Window::set_status_bar_visibility_cb(bool new_state)
{
	this->status_bar->setVisible(new_state);
}




void Window::set_main_menu_visibility_cb(bool new_state)
{
	this->menu_bar->setVisible(new_state);
}




void Window::zoom_cb(void)
{
	QAction * qa = (QAction *) QObject::sender();
	QKeySequence seq = qa->shortcut();
	QString debug_msg;

	if (seq == (Qt::CTRL + Qt::Key_Plus)) {
		qDebug() << SG_PREFIX_D << "Zoom In";
		debug_msg = "zoom in";
		this->main_gis_vp->zoom_in_on_center_pixel();
	} else if (seq == (Qt::CTRL + Qt::Key_Minus)) {
		qDebug() << SG_PREFIX_D << "Zoom Out";
		debug_msg = "zoom out";
		this->main_gis_vp->zoom_out_on_center_pixel();
	} else {
		qDebug() << SG_PREFIX_E << "Invalid zoom key sequence" << seq;
		return;
	}

	this->main_gis_vp->request_redraw(debug_msg);
}




void Window::zoom_to_cb(void)
{
	VikingScale viking_scale = this->main_gis_vp->get_viking_scale();

	if (GisViewportZoomDialog::custom_zoom_dialog(/* in/out */ viking_scale, this)) {
		this->main_gis_vp->set_viking_scale(viking_scale);
		this->main_gis_vp->request_redraw("zoom to...");
	}
}





/**
 * Display the background jobs window.
 */
void Window::show_background_jobs_window_cb(void)
{
	Background::show_window();
}




/**
   @brief Show in toolbal a name of currently selected layer tool

   @reviewed-on 2019-12-01
*/
void Window::display_current_tool_name(void)
{
	const LayerTool * tool = this->toolbox->get_current_tool();
	if (tool) {
		this->status_bar->set_message(StatusBarField::Tool, tool->get_description());
	} else {
		this->status_bar->set_message(StatusBarField::Tool, "");
	}
}




bool vik_window_clear_highlight_cb(Window * window)
{
	return window->clear_highlight();
}



/**
   Remove all tree items from "selected tree items" collection.
   If there were any, it means that we need to redraw layers.
*/
bool Window::clear_highlight(void)
{
	const bool need_redraw = (0 != g_selected.size());

	/* Clearing highlight means that there are no selected items. */
	g_selected.clear();

	return need_redraw;
}




void Window::set_redraw_trigger(TreeItem * tree_item)
{
	this->redraw_trigger = tree_item;
}




void Window::show_layer_defaults_cb(void)
{
	QAction * qa = (QAction *) QObject::sender();
	LayerKind layer_kind = (LayerKind) qa->data().toInt();

	qDebug() << SG_PREFIX_I << "Clicked \"layer defaults\" for layer kind" << Layer::get_translated_layer_kind_string(layer_kind);

	if (Layer::get_interface(layer_kind)->has_properties_dialog()) {
		LayerDefaults::show_window(layer_kind, this);
	} else {
		Dialog::info(tr("This layer kind has no configurable properties."), this);
	}

	/* No update needed. */
}




void Window::open_file_cb(void)
{
	QAction * qa = (QAction *) QObject::sender();
	const bool new_window = qa->data().toInt() == OPEN_FILE_IN_NEW_WINDOW;

	QFileDialog file_selector(this, tr("Select a GPS data file to open"));

	if (g_this_app.last_folder_files_url.isValid()) {
		file_selector.setDirectoryUrl(g_this_app.last_folder_files_url);
	}


	/* A trick to convert kml-as-MIME to list of non-MIME-file-types. */
	QStringList mime;
	mime << "application/vnd.google-earth.kml+xml";
	file_selector.setMimeTypeFilters(mime);
	const QStringList kml_name_filters = file_selector.nameFilters();


	/* Order of adding of the file types will match order of
	   appearance on filters list.

	   Could have filters for gpspoint (*.gps,*.gpsoint?) +
	   gpsmapper (*.gsm,*.gpsmapper?).  However assume this are
	   barely used and thus not worthy of inclusion as they'll
	   just make the options too many and have no clear file
	   pattern.  One can always use the all option. */
	QStringList filters;
	filters << QObject::tr("All (*)");
	filters << QObject::tr("Viking (*.vik *.viking)");
	filters << QObject::tr("JPEG (*.jpg, *.jpeg *.JPG *.JPEG)");
	filters << QObject::tr("GPX (*.gpx)");
#ifdef VIK_CONFIG_GEOCACHES
	filters << QObject::tr("Geocaching (*.loc)");
#endif
	filters << kml_name_filters;
	file_selector.setNameFilters(filters); /* This will overwrite file_selector.setMimeTypeFilters(mime) done above. */


	file_selector.setFileMode(QFileDialog::ExistingFiles); /* Zero or more existing files. */


	int dialog_code = file_selector.exec();
	if (dialog_code != QDialog::Accepted) {
		return;
	}

	g_this_app.last_folder_files_url = file_selector.directoryUrl();

	if ((this->dirty_flag || !this->current_document_full_path.isEmpty()) && new_window) {
		const QStringList selection = file_selector.selectedFiles();
		if (!selection.size()) {
			return;
		}
		this->open_window(selection);
	} else {
		QStringList selection = file_selector.selectedFiles();
		bool set_as_current_document = new_window && (selection.size() == 1); /* Only change current document path if one file. */
		bool first_vik_file = true;
		auto iter = selection.begin();
		while (iter != selection.end()) {

			const QString file_full_path = *iter;
			if (new_window && VikFile::has_vik_file_magic(file_full_path)) {
				/* Load first of many .vik files in current window. */
				if (first_vik_file) {
					this->open_file(file_full_path, true);
					first_vik_file = false;
				} else {
					/* Load each subsequent .vik file in a separate window. */
					Window * new_window2 = Window::new_window();
					if (new_window2) {
						/* No need to save on dirty flag - this is a brand new window. */
						new_window2->open_file(file_full_path, true);
					}
				}
			} else {
				/* Other file types. */
				this->open_file(file_full_path, set_as_current_document);
			}

			iter++;
		}
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
	this->file_load_status = VikFile::load(agg, this->main_gis_vp, new_document_full_path);
	switch (this->file_load_status.code) {
	case LoadStatus::Code::ReadFailure:
		Dialog::error(tr("The file you requested could not be opened."), this);
		break;
	case LoadStatus::Code::GPSBabelFailure:
		Dialog::error(tr("GPSBabel is required to load files of this type or GPSBabel encountered problems."), this);
		break;
	case LoadStatus::Code::GPXFailure:
		Dialog::error(tr("Unable to load malformed GPX file %1").arg(new_document_full_path), this);
		break;
	case LoadStatus::Code::UnsupportedFailure:
		Dialog::error(tr("Unsupported file type for %1").arg(new_document_full_path), this);
		break;
	case LoadStatus::Code::FailureNonFatal:
		{
			/* Since we can process .vik files with issues just show a warning in the status bar.
			   Not that a user can do much about it... or tells them what this issue is yet... */
			this->get_statusbar()->set_message(StatusBarField::Info, tr("WARNING: issues encountered loading %1").arg(file_base_name(new_document_full_path)));
		}
		/* No break, carry on to show any data. */
	case LoadStatus::Code::Success:
		{

			restore_original_filename = true; /* Will actually get inverted by the 'success' component below. */
			/* Update UI */
			if (set_as_current_document) {
				this->set_current_document_full_path(new_document_full_path);
			}

			QAction * drawmode_action = this->get_drawmode_action(this->main_gis_vp->get_draw_mode());
			this->only_updating_coord_mode_ui = true; /* if we don't set this, it will change the coord to UTM if we click Lat/Lon. I don't know why. */
			drawmode_action->setChecked(true);
			this->only_updating_coord_mode_ui = false;


			this->items_tree->change_coord_mode(this->main_gis_vp->get_coord_mode());

			/* Slightly long winded methods to align loaded viewport settings with the UI. */
			bool vp_state_scale = this->main_gis_vp->get_scale_visibility();
			bool ui_state_scale = this->qa_view_scale_visibility->isChecked();
			if (vp_state_scale != ui_state_scale) {
				this->main_gis_vp->set_scale_visibility(!vp_state_scale);
				this->set_scale_visibility_cb(!vp_state_scale);
			}
			bool vp_state_centermark = this->main_gis_vp->get_center_mark_visibility();
			bool ui_state_centermark = this->qa_view_center_mark_visibility->isChecked();
			if (vp_state_centermark != ui_state_centermark) {
				this->main_gis_vp->set_center_mark_visibility(!vp_state_centermark);
				this->set_center_mark_visibility_cb(!vp_state_centermark);
			}
			bool vp_state_highlight = this->main_gis_vp->get_highlight_usage();
			bool ui_state_highlight = this->qa_view_highlight_usage->isChecked();
			if (vp_state_highlight != ui_state_highlight) {
				this->main_gis_vp->set_highlight_usage(!vp_state_highlight);
				this->set_highlight_usage_cb(!vp_state_highlight);
			}
		}
		/* No break, carry on to redraw. */
		//case LoadStatus::OTHER_SUCCESS:
	default:
		success = true;
		/* When LoadStatus::OTHER_SUCCESS *only*, this will maintain the existing Viking project. */
		restore_original_filename = !restore_original_filename;

		this->update_recent_files(new_document_full_path, "text/x-gps-data");
		this->draw_tree_items(this->main_gis_vp);
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
		/* Current workspace hasn't been saved to a file yet.
		   This function will reset dirty flag if necessary. */
		return this->menu_file_save_as_cb();
	} else {
		return sg_ret::ok == this->save_current_workspace_to_file(this->current_document_full_path);
	}
}




bool Window::menu_file_save_as_cb(void)
{
	QFileDialog file_selector(this, QObject::tr("Save as Viking File"));
	file_selector.setFileMode(QFileDialog::AnyFile); /* Specify new or select existing file. */
	file_selector.setAcceptMode(QFileDialog::AcceptSave);

	/* "*.vik" should be the default format, so it goes as first one on the list. */
	const QStringList filter = QStringList() << tr("Viking (*.vik, *.viking)") << tr("All (*)");
	file_selector.setNameFilters(filter);


	if (g_this_app.last_folder_files_url.isValid()) {
		file_selector.setDirectoryUrl(g_this_app.last_folder_files_url);
	}

#ifdef K_FIXME_RESTORE
	gtk_window_set_transient_for(file_selector, this);
	gtk_window_set_destroy_with_parent(file_selector, true);
#endif

	/* Auto append / replace extension with '.vik' to the suggested file name as it's going to be a Viking File. */
	QString auto_save_name = this->get_current_document_file_name();
	if (!FileUtils::has_extension(auto_save_name, ".vik")) {
		auto_save_name = auto_save_name + ".vik";
	}

	file_selector.selectFile(auto_save_name);


	bool save_status = false;
	while (QDialog::Accepted == file_selector.exec()) {
		QStringList selection = file_selector.selectedFiles();
		if (!selection.size()) {
			continue;
		}
		const QString full_path = selection.at(0);

		/* QT file selector handles "overwrite existing file" question for us. */

		this->set_current_document_full_path(full_path);

		/* This function will display dialog message on save errors. */
		if (sg_ret::ok == this->save_current_workspace_to_file(full_path)) {
			g_this_app.last_folder_files_url = file_selector.directoryUrl();
		        save_status = true;
			break;
		}
	}

	return save_status;
}



void Window::update_recent_files(QString const & file_full_path, const QString & mime_type)
{
	SlavGPS::update_desktop_recent_documents(this, file_full_path, mime_type);

	/*
	  TODO_MAYBE
	  - add file type filter? gtk_recent_filter_add_group(filter, "viking");
	  - consider different sorting orders? gtk_recent_chooser_set_sort_type(GTK_RECENT_CHOOSER (menu), GTK_RECENT_SORT_MRU);
	*/

	/* Remove existing duplicate. */
	for (auto iter = this->recent_files.begin(); iter != this->recent_files.end(); iter++) {
		if (*iter == file_full_path) { /* This test will become more complicated as elements stored in ::recent_files become more complex. */
			this->recent_files.erase(iter);
			break;
		}
	}

	this->recent_files.push_front(file_full_path);

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
		qa->setData(*iter);
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (open_recent_file_cb()));
	}
}




/**
 * Call this before doing things that may take a long time and otherwise not show any other feedback
 * such as loading and saving files.
 */
void Window::set_busy_cursor()
{
	this->setCursor(Qt::WaitCursor);
	this->main_gis_vp->setCursor(Qt::WaitCursor);
}




void Window::clear_busy_cursor()
{
	this->setCursor(Qt::ArrowCursor);
	/* Viewport has a separate cursor. */
	/* FIXME: a cursor that has been used before exporting a track
	   to GPX file is not restored correctly.
	   Scenario:
	   1. load NMEA file with track,
	   2. in layers panel go to the track,
	   3. in context menu for the track select "export as GPX",
	   4. go through export procedure,
	   5. notice that viewport still has "busy" cursor.
	   Scenario 2:
	   1. create TRW layer, create some track,
	   2. save current workspace to file,
	   3. move cursor to main viewport, see that it's still showing 'busy' status.
	*/

	//this->main_gis_vp->setCursor(this->main_gis_vp->viewport_cursor);
}




void Window::set_current_document_full_path(const QString & document_full_path)
{
	this->current_document_full_path = document_full_path;

	/* Refresh title of main window, but not with full (and possibly long) path, but just file name. */
	this->setWindowTitle(tr("%1 - SlavGPS").arg(this->get_current_document_file_name()));
}




QString Window::get_current_document_file_name(void)
{
	return this->current_document_full_path.isEmpty() ? tr("Untitled") : FileUtils::get_base_name(this->current_document_full_path);
}




QString Window::get_current_document_full_path(void)
{
	return this->current_document_full_path;
}




QAction * Window::get_drawmode_action(GisViewportDrawMode mode)
{
	QAction * qa = NULL;
	switch (mode) {
#ifdef VIK_CONFIG_EXPEDIA
	case GisViewportDrawMode::Expedia:
		qa = this->qa_drawmode_expedia;
		break;
#endif
	case GisViewportDrawMode::Mercator:
		qa = this->qa_drawmode_mercator;
		break;
	case GisViewportDrawMode::LatLon:
		qa = this->qa_drawmode_latlon;
		break;
	case GisViewportDrawMode::UTM:
		qa = this->qa_drawmode_utm;
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected draw mode" << (int) mode;
		break;
	}

	assert(qa);
	return qa;
}




class LocatorJob : public BackgroundJob {
public:
	LocatorJob(Window * window);
	void run(void);

private:
	Window * window = NULL;
};




LocatorJob::LocatorJob(Window * new_window)
{
	this->n_items = 1; /* There is only one location to determine. */

	this->window = new_window;
}




/**
   Use the features in goto module to determine where we are
   Then set up the viewport:
   1. To goto the location
   2. Set an appropriate level zoom for the location type
   3. Some statusbar message feedback
*/
void LocatorJob::run(void)
{
	LatLon lat_lon;
	QString name;
	int ans = GoTo::where_am_i(this->window->get_main_gis_view(), lat_lon, name);

	const bool end_job = this->set_progress_state(1.0);
	if (end_job) {
		this->window->statusbar_update(StatusBarField::Info, QObject::tr("Location lookup aborted"));
		return; /* Abort thread */
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

		this->window->get_main_gis_view()->set_viking_scale(zoom);
		this->window->get_main_gis_view()->set_center_coord(lat_lon, false);
		this->window->get_main_gis_view()->request_redraw("determine location");

		this->window->statusbar_update(StatusBarField::Info, QObject::tr("Location found: %1").arg(name));
	} else {
		this->window->statusbar_update(StatusBarField::Info, QObject::tr("Unable to determine location"));
	}

	return;
}




/**
   Steps to be taken once initial loading has completed
*/
void Window::finish_new(void)
{
	/* Don't add a map if we've loaded a Viking file already. */
	if (!this->current_document_full_path.isEmpty()) {
		return;
	}

	if (Preferences::get_startup_method() == StartupMethod::SpecifiedFile) {
		/* No need to save on dirty flag - this is brand new window. */
		this->open_file(Preferences::get_startup_file(), true);
		if (!this->current_document_full_path.isEmpty()) {
			return;
		}
	}


	/* Maybe add a default map layer. */
	if (Preferences::get_add_default_map_layer()) {
		LayerMap * layer = new LayerMap();
		layer->set_name(QObject::tr("Default Map"));

		this->items_tree->get_top_layer()->add_child_item(layer, true);

		/* TODO_LATER: verify if this call is necessary. Maybe
		   adding new layer has triggered re-painting
		   already? */
		this->draw_tree_items(this->main_gis_vp);
	}


	/* If not loaded any file, maybe try the location lookup. */
	if (true || LoadStatus::Code::ReadFailure == this->file_load_status) {
		if (Preferences::get_startup_method() == StartupMethod::AutoLocation) {

			this->status_bar->set_message(StatusBarField::Info, tr("Trying to determine location..."));
			LocatorJob * locator = new LocatorJob(this);
			locator->set_description(tr("Determining location"));
			locator->run_in_background(ThreadPoolType::Remote);
		}
	}

}




void Window::open_window(const QStringList & file_full_paths)
{
	const bool set_as_current_document = (file_full_paths.size() == 1); /* Only change current document if we are opening one file. */

	for (int i = 0; i < file_full_paths.size(); i++) {
		/* Only open a new window if a viking file. */
		const QString file_full_path = file_full_paths.at(i);
		if (!this->current_document_full_path.isEmpty() && VikFile::has_vik_file_magic(file_full_path)) {
			Window * new_window = Window::new_window();
			if (new_window) {
				/* No need to save on dirty flag, this is brand new window. */
				new_window->open_file(file_full_path, true);
			}
		} else {
			this->open_file(file_full_path, set_as_current_document);
		}
	}
}




void Window::show_centers_cb() /* Slot. */
{
	this->main_gis_vp->show_center_coords(this);
}




void Window::help_help_cb(void)
{
#ifdef K_FIXME_RESTORE
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




void Window::acquire_handler(DataSource * data_source)
{
	/* Override mode. */
	DataSourceMode mode = data_source->mode;
	if (mode == DataSourceMode::AutoLayerManagement) {
		mode = DataSourceMode::CreateNewLayer;
	}

	AcquireContext acquire_context(this, this->main_gis_vp, this->items_tree->get_top_layer(), (LayerTRW *) this->items_tree->get_selected_layer());
	Acquire::acquire_from_source(data_source, mode, acquire_context);
}




void Window::acquire_from_gps_cb(void)
{
	this->acquire_handler(new DataSourceGPS());
}




void Window::acquire_from_file_cb(void)
{
	this->acquire_handler(new DataSourceFile());
}




void Window::acquire_from_geojson_cb(void)
{
	this->acquire_handler(new DataSourceGeoJSON());
}




void Window::acquire_from_routing_cb(void)
{
	this->acquire_handler(new DataSourceRouting());
}




void Window::acquire_from_osm_cb(void)
{
	this->acquire_handler(new DataSourceOSMTraces());
}




void Window::acquire_from_my_osm_cb(void)
{
	this->acquire_handler(new DataSourceOSMMyTraces());
}




#ifdef VIK_CONFIG_GEOCACHES
void Window::acquire_from_gc_cb(void)
{
	if (!DataSourceGeoCache::have_programs()) {
		return;
	}

	this->acquire_handler(new DataSourceGeoCache(ThisApp::get_main_gis_view()));
}
#endif




#ifdef VIK_CONFIG_GEOTAG
void Window::acquire_from_geotag_cb(void)
{
	this->acquire_handler(new DataSourceGeoTag());
}
#endif




#ifdef VIK_CONFIG_GEONAMES
void Window::acquire_from_wikipedia_cb(void)
{
	this->acquire_handler(new DataSourceWikipedia());
}
#endif




void Window::acquire_from_url_cb(void)
{
	this->acquire_handler(new DataSourceURL());
}




void Window::draw_viewport_to_image_file_cb(void)
{
	ViewportToImage vti(this->main_gis_vp, ViewportToImage::SaveMode::File, this);
	if (!vti.run_config_dialog(tr("Save Viewport to Image File"))) {
		return;
	}

	const QString destination_full_path = vti.get_destination_full_path();
	if (destination_full_path.isEmpty()) {
		return;
	}

	vti.save_to_destination(destination_full_path);
}




void Window::draw_viewport_to_image_dir_cb(void)
{
	if (this->main_gis_vp->get_coord_mode() != CoordMode::UTM) {
		Dialog::info(tr("This feature is available only in UTM mode"));
		return;
	}

	ViewportToImage vti(this->main_gis_vp, ViewportToImage::SaveMode::Directory, this);
	if (vti.run_config_dialog(tr("Save Viewport to Images in Directory"))) {
		vti.run_save_dialog_and_save();
	}
	return;
}




#ifdef HAVE_ZIP_H
/*
  ATM This only generates a KMZ file with the current
  viewport image - intended mostly for map images [but will
  include any lines/icons from track & waypoints that are
  drawn] (it does *not* include a full KML dump of every
  track, waypoint etc...).
*/
void Window::draw_viewport_to_kmz_file_cb(void)
{
	if (this->main_gis_vp->get_coord_mode() == CoordMode::UTM) {
		Dialog::info(tr("This feature is not available in UTM mode"));
		return;
	}

	ViewportToImage vti(this->get_viewport, ViewportToImage::SaveMode::FileKMZ, this);
	if (vti.run_config_dialog(tr("Save Viewport to KMZ File"))) {
		vti.run_save_dialog_and_save();
	}

	return;
}









}
#endif




void Window::print_cb(void)
{
	a_print(this, this->main_gis_vp);
}




/* Menu View -> Zoom -> Value. */
void Window::zoom_level_selected_cb(QAction * qa) /* Slot. */
{
	const double requested_scale = ZoomLevels::get_selected_scale(qa);

	/* But has it really changed? */
	const double current_scale = this->main_gis_vp->get_viking_scale().get_x();
	if (current_scale != 0.0 && requested_scale != current_scale) {
		this->main_gis_vp->set_viking_scale(requested_scale);

		/* Ask to draw updated viewport. */
		this->main_gis_vp->request_redraw("zoom level selected");
	}
}




void ZoomLevels::init_once(void)
{
	if (ZoomLevels::actions.size()) {
		qDebug() << SG_PREFIX_I << "Sub-module already initialized";
		return;
	} else {
		qDebug() << SG_PREFIX_I << "Initializing sub-module";
	}

	int qa_idx = 0;
	for (const auto & label : ZoomLevels::labels) {
		QAction * qa = new QAction(label, NULL);
		qa->setData(qa_idx); /* tag::zoom_actions_qa_idx */
		ZoomLevels::actions.push_back(qa);
		qa_idx++;
	}
}




QMenu * ZoomLevels::create_zoom_submenu(const VikingScale & viking_scale, QString const & label, QMenu * parent)
{
	QMenu * menu = NULL;
	if (parent) {
		menu = parent->addMenu(label);
	} else {
		menu = new QMenu(label);
	}

	/* Initialize ZoomLevels' internal data structure, but only
	   once during program's entire live. */
	ZoomLevels::init_once();

	for (auto iter = ZoomLevels::actions.begin(); iter != ZoomLevels::actions.end(); iter++) {
		menu->addAction(*iter);
	}

	const int active_qa_idx = ZoomLevels::qa_index_from_viking_scale(viking_scale);
	menu->setActiveAction(ZoomLevels::actions[active_qa_idx]);

	return menu;
}




/* Really a misnomer: changes coord mode (actual coordinates) AND/OR draw mode (viewport only). */
void Window::change_coord_mode_cb(QAction * qa)
{
	/* TODO_HARD: verify that this function changes mode in all the places that need to be updated. */

	const GisViewportDrawMode old_drawmode = this->main_gis_vp->get_draw_mode();
	const GisViewportDrawMode new_drawmode = (GisViewportDrawMode) qa->data().toInt();

	qDebug() << SG_PREFIX_D << "Coordinate mode changed from" << (int) old_drawmode << "to" << qa->text() << (int) new_drawmode;

#if 0
	if (this->only_updating_coord_mode_ui) {
		return;
	}
#endif
	if (new_drawmode == old_drawmode) {
		return;
	}


	{
		CoordMode new_coord_mode = CoordMode::Invalid;
		if (new_drawmode == GisViewportDrawMode::UTM) {
			/* Coord mode has been changed to UTM. */
			new_coord_mode = CoordMode::UTM;
		} else if (old_drawmode == GisViewportDrawMode::UTM) {
			/* CoordMode has been changed from UTM. */
			new_coord_mode = CoordMode::LatLon;
		} else {
			/* CoordMode stays the same (is LatLon). */
			qDebug() << SG_PREFIX_I << "Switching from" << old_drawmode << "to" << new_drawmode << "does not change coord mode";
		}
		if (CoordMode::Invalid != new_coord_mode) {
			/* Tell various widgets throughout application
			   that they have to adjust to new coord
			   mode. E.g. a CoordWidget int Waypoint
			   properties dialog will have to switch from
			   displaying LatLon to displaying UTM. */
			qDebug() << SG_PREFIX_SIGNAL << "Will emit 'Window::coord_mode_changed()' signal, coord mode changed to" << new_coord_mode;
			emit this->coord_mode_changed(new_coord_mode);
		}
	}



	/* Set draw mode of viewport. */
	this->main_gis_vp->set_draw_mode(new_drawmode);


	/* Set coord mode of tree items. */
	switch (new_drawmode) {
	case GisViewportDrawMode::UTM:
		this->items_tree->change_coord_mode(CoordMode::UTM);
		break;
	case GisViewportDrawMode::Expedia:
	case GisViewportDrawMode::Mercator:
	case GisViewportDrawMode::LatLon:
		this->items_tree->change_coord_mode(CoordMode::LatLon);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected drawmode" << (int) new_drawmode;
		return;
	}


	/* Draw tree items with new coord mode into viewport with new
	   draw mode. */
	this->draw_tree_items(this->main_gis_vp);
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
	color_dialog.setCurrentColor(this->main_gis_vp->get_highlight_color());
	color_dialog.setWindowTitle(tr("Choose a highlight color"));
	if (QDialog::Accepted == color_dialog.exec()) {
		const QColor selected_color = color_dialog.selectedColor();
		this->main_gis_vp->set_highlight_color(selected_color);
		this->draw_tree_items(this->main_gis_vp);
	}
}




void Window::menu_view_set_bg_color_cb(void)
{
	QColorDialog color_dialog(this);
	color_dialog.setCurrentColor(this->main_gis_vp->get_background_color());
	color_dialog.setWindowTitle(tr("Choose a background color"));
	if (QDialog::Accepted == color_dialog.exec()) {
		const QColor selected_color = color_dialog.selectedColor();
		this->main_gis_vp->set_background_color(selected_color);
		this->draw_tree_items(this->main_gis_vp);
	}
}




void Window::menu_view_pan_cb(void)
{
	const QAction * qa = (QAction *) QObject::sender();
	const int direction = qa->data().toInt();
	qDebug() << SG_PREFIX_SLOT << "Menu View Pan:" << qa->text() << direction;

	/* TODO_LATER: verify how this comment applies to Qt application. */

	/* Since the tree view cell editting intercepts standard
	   keyboard handlers, it means we can receive events here.
	   Thus if currently editting, ensure we don't move the
	   viewport when Ctrl+<arrow> is received. */
	Layer * layer = this->items_tree->get_selected_layer();
	if (layer && layer->tree_view->is_editing_in_progress()) {
		qDebug() << SG_PREFIX_SLOT << "Menu View Pan: editing in progress";
		return;
	}

	GisViewport * v = this->main_gis_vp;
	const fpixel x_center_pixel = v->central_get_x_center_pixel();
	const fpixel y_center_pixel = v->central_get_y_center_pixel();

	switch (direction) {
	case PAN_NORTH:
		v->set_center_coord(x_center_pixel, v->central_get_topmost_pixel()); /* Move topmost pixel to center. */
		break;
	case PAN_EAST:
		v->set_center_coord(v->central_get_rightmost_pixel(), y_center_pixel); /* Move rightmost pixel to center. */
		break;
	case PAN_SOUTH:
		v->set_center_coord(x_center_pixel, v->central_get_bottommost_pixel()); /* Move bottommost pixel to center. */
		break;
	case PAN_WEST:
		v->set_center_coord(v->central_get_leftmost_pixel(), y_center_pixel); /* Move leftmost pixel to center. */
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unknown pan direction" << direction;;
		break;
	}

	this->main_gis_vp->request_redraw("pan from menu");
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
	Layer * layer = this->items_tree->get_top_layer()->get_top_visible_layer_of_type(LayerKind::Map);
	if (layer) {
		((LayerMap *) layer)->download(this->main_gis_vp, only_new);
	}
}




/* Mouse event handlers ************************************************************************/


void Window::draw_click_cb(QMouseEvent * ev)
{
#ifdef K_FIXME_RESTORE
	this->main_gis_vp->setFocus();

	/* middle button pressed.  we reserve all middle button and scroll events
	 * for panning and zooming; tools only get left/right/movement
	 */
	if (ev->button() == Qt::MiddleButton) {
		if (this->tb->active_tool->pan_handler) {
			// Tool still may need to do something (such as disable something)
			this->tb->click(ev);
		}
		this->pan_click(ev);
	} else {
		this->tb->click(ev);
	}
#endif
}




/**
 * Action the single click after a small timeout.
 * If a double click has occurred then this will do nothing.
 */
static bool window_pan_timeout(Window * window)
{
#ifdef K_FIXME_RESTORE
	if (!window->single_click_pending) {
		/* Double click happened, so don't do anything. */
		return false;
	}

	/* Set panning origin. */
	window->pan_move_in_progress = false;
	window->single_click_pending = false;
	window->viewport->set_center_coord(window->delayed_pan_pos);

	/* TODO_LATER: verify if this call is necessary. Maybe call to
	   set_center_coord() has emitted a signal that triggered
	   re-painting already? */
	window->draw_tree_items(this->main_gis_vp);

	/* Really turn off the pan moving!! */
	window->pan_off();
#endif
	return false;
}




void Window::draw_release_cb(QMouseEvent * ev)
{
#ifdef K_FIXME_RESTORE
	this->main_gis_vp->setFocus();

	if (ev->button() == Qt::MiddleButton) {  /* move / pan */
		if (this->tb->active_tool->pan_handler) {
			// Tool still may need to do something (such as reenable something)
			this->tb->release(ev);
		}
		this->pan_release(ev);
	} else {
		this->tb->release(ev);
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
bool Window::export_to(const std::list<const Layer *> & layers, SGFileType file_type, const QString & full_dir_path, char const *extension)
{
	bool success = true;

	int export_count = 0;

	this->set_busy_cursor();

	for (auto iter = layers.begin(); iter != layers.end(); iter++) {
		const Layer * layer = *iter;
		QString file_full_path = full_dir_path + QDir::separator() + layer->get_name() + extension;

		/* Some protection in attempting to write too many same named files.
		   As this will get horribly slow... */
		bool safe = false;
		int ii = 2;
		while (ii < 5000) {
			if (0 == access(file_full_path.toUtf8().constData(), F_OK)) {
				/* Try rename. */
				file_full_path = QString("%1%2%3#%4%5").arg(full_dir_path).arg(QDir::separator()).arg(layer->get_name()).arg(ii, 3, 10, QChar('0')).arg(extension);
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
			bool this_success = SaveStatus::Code::Success == VikFile::export_trw_layer((LayerTRW *) layer, file_full_path, file_type, true);

			/* Show some progress. */
			if (this_success) {
				export_count++;
				this->status_bar->set_message(StatusBarField::Info, tr("Exporting to file: %1").arg(file_full_path));
			}

			success = success && this_success;
		}
	}

	this->clear_busy_cursor();

	/* Confirm what happened. */
	this->status_bar->set_message(StatusBarField::Info, tr("Exported files: %1").arg(export_count));

	return success;
}




void Window::export_to_common(SGFileType file_type, char const * extension)
{
	const std::list<const Layer *> layers = this->items_tree->get_all_layers_of_kind(LayerKind::TRW, true);
	if (layers.empty()) {
		Dialog::info(tr("Nothing to Export!"), this);
		return;
	}

	QFileDialog file_selector(this, QObject::tr("Export to directory"));
	file_selector.setFileMode(QFileDialog::Directory);
	file_selector.setAcceptMode(QFileDialog::AcceptSave);

	if (QDialog::Accepted != file_selector.exec()) {
		return;
	}

	QStringList selection = file_selector.selectedFiles();
	if (!selection.size()) {
		return;
	}

	const QString full_dir_path = selection.at(0);
	if (!this->export_to(layers, file_type, full_dir_path, extension)) {
		Dialog::error(tr("Could not convert all files"), this);
	}
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

	const Time timestamp(stat_buf.st_mtime, Time::get_internal_unit());
	const QString size_string = Measurements::get_file_size_string(stat_buf.st_size);
	const QString message = QObject::tr("%1\n\n%2\n\n%3").arg(this->current_document_full_path).arg(timestamp.strftime_utc("%c")).arg(size_string);

	Dialog::info(message, this);
}




bool Window::menu_file_save_and_exit_cb(void)
{
	if (this->menu_file_save_cb()) {
		this->close();
		return true;
	} else {
		return false;
	}
}




/**
   @reviewed-on 2019-07-27
*/
sg_ret Window::save_current_workspace_to_file(const QString & full_path)
{
	qDebug() << SG_PREFIX_I << "Before calling function that saves a file" << full_path;
	this->set_busy_cursor();
	const SaveStatus save_status = VikFile::save(this->items_tree->get_top_layer(), this->main_gis_vp, full_path);
	this->clear_busy_cursor();
	qDebug() << SG_PREFIX_I << "After calling function that saves a file" << full_path;

	if (save_status.code == SaveStatus::Code::Success) {
		this->set_dirty_flag(false);
		this->update_recent_files(full_path, "text/x-gps-data");
		return sg_ret::ok;
	} else {
		save_status.show_error_dialog(this);
		return sg_ret::err;
	}
}




void Window::import_kmz_file_cb(void)
{
	QFileDialog file_selector(this, QObject::tr("Open File"));
	file_selector.setFileMode(QFileDialog::ExistingFile);
	/* AcceptMode is QFileDialog::AcceptOpen by default. */;

	QStringList mime;
	mime << "application/vnd.google-earth.kmz";  /* "KMZ" / "*.kmz". First in QStringList -> default filter in file selector. */
	mime << "application/octet-stream";          /* "All files (*)". */
	file_selector.setMimeTypeFilters(mime);

	if (QDialog::Accepted != file_selector.exec())  {
		return;
	}

	QStringList selection = file_selector.selectedFiles();
	if (!selection.size()) {
		return;
	}
	const QString full_path = selection.at(0);

	const KMZOpenResult ret = kmz_open_file(full_path, this->main_gis_vp, this->items_tree);
	if (ret.kmz_status != KMZOpenStatus::Success) {
		Dialog::error(tr("Unable to import %1: %2").arg(full_path).arg(ret.to_string()), this);
	}

	this->draw_tree_items(this->main_gis_vp);
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

#ifdef K_FIXME_RESTORE
	QObject::connect(window, SIGNAL("destroy"), NULL, SLOT (destroy_window_cb));
	QObject::connect(window, SIGNAL("newwindow"), NULL, SLOT (new_window_cb));
#endif

	if (Preferences::get_restore_window_state()) {
		/* These settings are applied after the show all as these options hide widgets. */
		bool visibility;

		if (ApplicationState::get_boolean(VIK_SETTINGS_WIN_SIDEPANEL, &visibility)) {
			window->set_side_panel_visibility_cb(visibility);
		}

		if (ApplicationState::get_boolean(VIK_SETTINGS_WIN_TOOLS_DOCK, &visibility)) {
			window->set_tools_dock_visibility_cb(visibility);
		}

		if (ApplicationState::get_boolean(VIK_SETTINGS_WIN_STATUSBAR, &visibility)) {
			window->set_status_bar_visibility_cb(visibility);
		}

		if (ApplicationState::get_boolean(VIK_SETTINGS_WIN_TOOLBAR, &visibility)) {
			window->toolbar->setVisible(visibility);
		}

		if (ApplicationState::get_boolean(VIK_SETTINGS_WIN_MENUBAR, &visibility)) {
			window->set_main_menu_visibility_cb(visibility);
		}
	}

	window_count++;

	return window;
}




void Window::menu_view_cache_info_cb(void)
{
	const size_t bytes = MapCache::get_size_bytes();
	const QString size_string = Measurements::get_file_size_string(bytes);
	const QString msg = tr("Map Cache size is %1 with %2 items").arg(size_string).arg(MapCache::get_items_count());

	Dialog::info(msg, this);
}




void Window::apply_new_preferences(void)
{
	/* Want to update all TRW layers. */
	const std::list<Layer const *> layers = this->items_tree->get_all_layers_of_kind(LayerKind::TRW, true);
	if (layers.empty()) {
		return;
	}

	for (auto iter = layers.begin(); iter != layers.end(); iter++) {
		/* Reset the individual waypoints themselves due to the preferences change. */
		LayerTRW * trw = (LayerTRW *) *iter;
		trw->reset_waypoints();
	}

	this->draw_tree_items(this->main_gis_vp);
}




void Window::destroy_window_cb(void)
{
	window_count--;
}




void Window::keyPressEvent(QKeyEvent * ev)
{
	const int key = ev->key();
	const Qt::KeyboardModifiers modifiers = ev->modifiers();

	bool map_download = false;
	bool map_download_only_new = true; /* Only new or reload. */

	/* Standard 'Refresh' keys are: F5 or Ctrl+r.  Regular 'F5 is
	   handled via menu_view_refresh_cb(), so here handle other
	   combinations. */
	if (key == Qt::Key_R && modifiers == Qt::ControlModifier) {
		map_download = true;
		map_download_only_new = true;

	} else if ((key == Qt::Key_F5 && modifiers == Qt::ControlModifier)
		 || (key == Qt::Key_R && modifiers == (Qt::ControlModifier + Qt::ShiftModifier))) {

		/* Full cache reload. */
		map_download = true;
		map_download_only_new = false;

	} else if (key == Qt::Key_Escape) {
		/* Restore Main Menu via Escape key if the user has hidden it.
		   This key is more likely to be used as they may not remember the function key */
		if (!this->menu_bar->isVisible()) {
			this->menu_bar->setVisible(true);
		}
		/* End of handling of this key. */
		return;
	} else {
		/* Standard Ctrl++ / Ctrl+- to zoom in/out are already
		   handled by global qa_view_zoom_in/out actions in
		   window. No more keys to handle. */

		QMainWindow::keyPressEvent(ev);
		/* End of handling of this key. */
		return;
	}


#ifdef K_FIXME_RESTORE
	if (map_download) {
		this->simple_map_update(map_download_only_new);
		return;
	}

	Layer * layer = this->items_tree->get_selected_layer();
	if (layer && this->tb->active_tool && this->tb->active_tool->key_press) {
		LayerKind ltype = this->tb->active_tool->m_layer_kind;
		if (layer && ltype == layer->type) {
			this->tb->active_tool->key_press(layer, ev, this->tb->active_tool);
			return;
		}
	}

	/* Ensure called only on window tools (i.e. not on any of the
	   Layer tools since the layer is NULL). */
	if (this->current_tool < TOOL_LAYER) {
		/* No layer - but enable window tool keypress
		   processing - these should be able to handle a NULL
		   layer. */
		if (this->tb->active_tool->key_press) {
			this->tb->active_tool->key_press(layer, ev, this->tb->active_tool);
			return;
		}
	}
#endif
	return;
}




void Window::open_recent_file_cb(void)
{
	QAction * qa = (QAction *) QObject::sender();
	const QString file_full_path = qa->data().toString();

	if (file_full_path.isEmpty()) {
		qDebug() << SG_PREFIX_E << "File path from 'recent file' action is empty";
		return;
	}

	if (this->current_document_full_path.isEmpty()) {
		/* We don't have any file opened yet. Open this file
		   in this window. */

		if (this->save_on_dirty_flag()) {
			this->open_file(file_full_path, true);
		}
	} else {
		/* Current window has already some file open. Open the
		   'recent file' in new window. */
		this->open_window(QStringList(file_full_path));
	}
}




void Window::set_dirty_flag(bool dirty)
{
	this->dirty_flag = dirty;
}




Window * ThisApp::get_main_window(void)
{
	assert (g_this_app.window);

	return g_this_app.window;
}




LayersPanel * ThisApp::get_layers_panel(void)
{
	assert (g_this_app.layers_panel);

	return g_this_app.layers_panel;
}




GisViewport * ThisApp::get_main_gis_view(void)
{
	assert (g_this_app.gisview);

	return g_this_app.gisview;
}
