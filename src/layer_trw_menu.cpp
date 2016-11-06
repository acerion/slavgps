/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2008, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2009, Hein Ragas <viking@ragas.nl>
 * Copyright (c) 2012-2015, Rob Norris <rw_norris@hotmail.com>
 * Copyright (c) 2012-2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
/* WARNING: If you go beyond this point, we are NOT responsible for any ill effects on your sanity */
/* viktrwlayer.c -- 8000+ lines can make a difference in the state of things */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <cassert>

#include "util.h"
#include "layer_trw.h"

#ifdef K
#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "vikgpslayer.h"
#include "viktrwlayer_export.h"
#include "waypoint_properties.h"
#include "viktrwlayer_propwin.h"
#include "viktrwlayer_analysis.h"
#include "viktrwlayer_tracklist.h"
#include "viktrwlayer_waypointlist.h"
#ifdef VIK_CONFIG_GEOTAG
#include "viktrwlayer_geotag.h"
#include "geotag_exif.h"
#endif
#include "garminsymbols.h"
#include "thumbnails.h"
#include "background.h"
#include "gpx.h"
#include "file.h"
#include "dialog.h"
#include "geojson.h"
#include "babel.h"
#include "dem.h"
#include "dems.h"
#include "geonamessearch.h"
#ifdef VIK_CONFIG_OPENSTREETMAP
#include "osm-traces.h"
#endif
#include "acquire.h"
#include "datasources.h"
#include "datasource_gps.h"
#include "vikexttools.h"
#include "vikexttool_datasources.h"
#include "ui_util.h"
#include "vikutils.h"
#include "gpspoint.h"
#include "clipboard.h"
#include "settings.h"
#include "globals.h"
#include "vikrouting.h"
#include "layer_trw_draw.h"
#include "icons/icons.h"
#endif




using namespace SlavGPS;




#define VIK_CONFIG_OPENSTREETMAP
#define VIK_CONFIG_GEONAMES
#define VIK_CONFIG_GEOCACHES
#define VIK_CONFIG_GEOTAG


#define POINTS 1
#define LINES 2

/* This is how it knows when you click if you are clicking close to a trackpoint. */
#define TRACKPOINT_SIZE_APPROX 5
#define WAYPOINT_SIZE_APPROX 5

#define MIN_STOP_LENGTH 15
#define MAX_STOP_LENGTH 86400




extern bool have_astro_program;
extern char * astro_program;
extern bool have_diary_program;
extern char *diary_program;
extern bool have_geojson_export;




void LayerTRW::add_menu_items(QMenu & menu, void * panel_)
{
	QAction * qa = NULL;

	menu.addSeparator();

	if (this->current_track) {
		if (this->current_track->is_route) {
			qa = menu.addAction(QString(_("_Finish Route")));
		} else {
			qa = menu.addAction(QString(_("_Finish Track")));
		}
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (finish_track_cb()));


		menu.addSeparator();
	}

	qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), QString(_("&View Layer")));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (full_view_cb()));

	{
		QMenu * view_submenu = menu.addMenu(QIcon::fromTheme("edit-find"), QString("V&iew"));

		qa = view_submenu->addAction(QString("View All &Tracks"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (full_view_tracks_cb()));

		qa = view_submenu->addAction(QString("View All &Routes"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (full_view_routes_cb()));

		qa = view_submenu->addAction(QString("View All &Waypoints"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (full_view_waypoints_cb()));
	}

	qa = menu.addAction(QIcon::fromTheme("go-jump"), QString(_("&Goto Center of Layer")));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (centerize_cb()));

	qa = menu.addAction(QIcon::fromTheme(""), QString(_("Goto &Waypoint...")));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (goto_waypoint_cb()));

	{
		QMenu * export_submenu = menu.addMenu(QIcon::fromTheme("document-save-as"), QString("&Export Layer"));

		qa = export_submenu->addAction(QString(_("Export as GPS&Point...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (export_as_gpspoint_cb()));

		qa = export_submenu->addAction(QString(_("Export as GPS&Mapper...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (export_as_gpsmapper_cb()));

		qa = export_submenu->addAction(QString(_("Export as &GPX...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (export_as_gpx_cb()));

		qa = export_submenu->addAction(QString(_("Export as &KML...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (export_as_kml_cb()));

		if (have_geojson_export) {
			qa = export_submenu->addAction(QString(_("Export as GEO&JSON...")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (export_as_geojson_cb()));
		}

		qa = export_submenu->addAction(QString(_("Export via GPSbabel...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (export_via_babel_cb()));

		qa = export_submenu->addAction(QString(_("Open with External Program&1: %1")).arg(QString(a_vik_get_external_gpx_program_1())));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (open_with_external_gpx_1_cb()));

		qa = export_submenu->addAction(QString(_("Open with External Program&2: %1")).arg(QString(a_vik_get_external_gpx_program_2())));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (open_with_external_gpx_2_cb()));
	}



	{
		QMenu * new_submenu = menu.addMenu(QIcon::fromTheme("document-new"), QString("&New"));

		qa = new_submenu->addAction(QIcon::fromTheme("document-new"), QString(_("New &Waypoint...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (new_waypoint_cb()));

		qa = new_submenu->addAction(QIcon::fromTheme("document-new"), QString(_("New &Track")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (new_track_cb()));
		/* Make it available only when a new track is *not* already in progress. */
		qa->setEnabled(!(bool)KPOINTER_TO_INT(this->current_track));

		qa = new_submenu->addAction(QIcon::fromTheme("document-new"), QString(_("New &Route")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (new_route_cb()));
		/* Make it available only when a new route is *not* already in progress. */
		qa->setEnabled(!(bool)KPOINTER_TO_INT(this->current_track));
	}



#ifdef VIK_CONFIG_GEOTAG
	qa = menu.addAction(QString(_("Geotag &Images...")));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (geotag_images_cb()));
#endif



	{
		QMenu * acquire_submenu = menu.addMenu(QIcon::fromTheme("go-down"), QString("&Acquire"));

		qa = acquire_submenu->addAction(QString(_("From &GPS...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_gps_cb()));

		/* FIXME: only add menu when at least a routing engine has support for Directions. */
		qa = acquire_submenu->addAction(QString(_("From &Directions...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_routing_cb()));

#ifdef VIK_CONFIG_OPENSTREETMAP
		qa = acquire_submenu->addAction(QString(_("From &OSM Traces...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_osm_cb()));

		qa = acquire_submenu->addAction(QString(_("From &My OSM Traces...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_osm_my_traces_cb()));
#endif

		qa = acquire_submenu->addAction(QString(_("From &URL...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_url_cb()));

#ifdef VIK_CONFIG_GEONAMES
		{
			QMenu * wikipedia_submenu = acquire_submenu->addMenu(QIcon::fromTheme("list-add"), QString("From &Wikipedia Waypoints"));

			qa = wikipedia_submenu->addAction(QIcon::fromTheme("zoom-fit-best"), QString(_("Within &Layer Bounds")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_wikipedia_waypoints_layer_cb()));

			qa = wikipedia_submenu->addAction(QIcon::fromTheme("zoom-original"), QString(_("Within &Current View")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_wikipedia_waypoints_viewport_cb()));
		}
#endif


#ifdef VIK_CONFIG_GEOCACHES
		qa = acquire_submenu->addAction(QString(_("From Geo&caching...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_geocache_cb()));
#endif

#ifdef VIK_CONFIG_GEOTAG
		qa = acquire_submenu->addAction(QString(_("From Geotagged &Images...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_geotagged_images_cb()));
#endif

		qa = acquire_submenu->addAction(QString(_("From &File...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_file_cb()));
		qa->setToolTip(QString(_("Import File With GPS_Babel...")));
#ifdef K
		vik_ext_tool_datasources_add_menu_items_to_menu(this->get_window(), GTK_MENU (acquire_submenu));
#endif
	}



	{
		QMenu * upload_submenu = menu.addMenu(QIcon::fromTheme("go-up"), QString("&Upload"));

		qa = upload_submenu->addAction(QIcon::fromTheme("go-next"), QString(_("Upload to &GPS...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (upload_to_gps_cb()));

#ifdef VIK_CONFIG_OPENSTREETMAP
		qa = upload_submenu->addAction(QIcon::fromTheme("go-up"), QString(_("Upload to &OSM...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (upload_to_osm_traces_cb()));
#endif
	}



	{
		QMenu * delete_submenu = menu.addMenu(QIcon::fromTheme("list-remove"), QString("De&lete"));

		qa = delete_submenu->addAction(QIcon::fromTheme("list-remove"), QString(_("Delete All &Tracks")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_all_tracks_cb()));

		qa = delete_submenu->addAction(/* QIcon::fromTheme(""), */ QString(_("Delete Tracks _From Selection...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_selected_tracks_cb()));

		qa = delete_submenu->addAction(QIcon::fromTheme("list-remove"), QString(_("Delete &All Routes")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_all_routes_cb()));

		qa = delete_submenu->addAction(/* QIcon::fromTheme(""), */ QString(_("&Delete Routes From Selection...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_selected_routes_cb()));

		qa = delete_submenu->addAction(QIcon::fromTheme("list-remove"), QString(_("Delete All &Waypoints")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_all_waypoints_cb()));

		qa = delete_submenu->addAction(/* QIcon::fromTheme(""), */ QString(_("Delete Waypoints From &Selection...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_selected_waypoints_cb()));
	}



#ifdef K
	item = a_acquire_trwlayer_menu(this->get_window(), panel,
				       panel->get_viewport(), this);
	if (item) {
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
	}

	item = a_acquire_trwlayer_track_menu(this->get_window(), panel,
					     panel->get_viewport(), this);
	if (item) {
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
	}

	item = gtk_image_menu_item_new_with_mnemonic(_("Track _List..."));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_track_list_dialog), &pass_along);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);
	gtk_widget_set_sensitive(item, (bool) (this->tracks.size() + this->routes.size()));

	item = gtk_image_menu_item_new_with_mnemonic(_("_Waypoint List..."));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_waypoint_list_dialog), &pass_along);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);
	gtk_widget_set_sensitive(item, (bool) (this->waypoints.size()));

	GtkWidget *external_submenu = create_external_submenu(menu);
	/* TODO: Should use selected layer's centre - rather than implicitly using the current viewport. */
	vik_ext_tools_add_menu_items_to_menu(this->get_window(), GTK_MENU (external_submenu), NULL);
#endif
}




/* Panel can be NULL if necessary - i.e. right-click from a tool. */
/* Viewpoint is now available instead. */
bool LayerTRW::sublayer_add_menu_items(QMenu & menu, void * panel, SublayerType sublayer_type, sg_uid_t sublayer_uid, TreeIndex * index, Viewport * viewport)
{
#ifdef K
	GtkWidget *item;
	bool rv = false;

	static trw_menu_sublayer_t pass_along;

	pass_along.layer         = this;
	pass_along.panel         = (LayersPanel *) panel;
	pass_along.sublayer_type = sublayer_type;
	pass_along.sublayer_uid  = sublayer_uid;
	pass_along.confirm       = true; /* Confirm delete request. */
	pass_along.viewport      = viewport;
	pass_along.tv_iter       = iter;
	pass_along.misc          = NULL; /* For misc purposes - maybe track or waypoint. */

	if (sublayer_type == SublayerType::WAYPOINT || sublayer_type == SublayerType::TRACK || sublayer_type == SublayerType::ROUTE) {
		rv = true;

		item = gtk_image_menu_item_new_from_stock(GTK_STOCK_PROPERTIES, NULL);
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_properties_item), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		if (sublayer_type == SublayerType::TRACK) {
			Track * trk = this->tracks.at(sublayer_uid);
			if (trk && trk->property_dialog) {
				gtk_widget_set_sensitive(GTK_WIDGET (item), false);
			}
		}
		if (sublayer_type == SublayerType::ROUTE) {
			Track * trk = this->routes.at(sublayer_uid);
			if (trk && trk->property_dialog) {
				gtk_widget_set_sensitive(GTK_WIDGET (item), false);
			}
		}

		item = gtk_image_menu_item_new_from_stock(GTK_STOCK_CUT, NULL);
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_cut_item_cb), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_from_stock(GTK_STOCK_COPY, NULL);
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_copy_item_cb), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_from_stock(GTK_STOCK_DELETE, NULL);
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_delete_item), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		if (sublayer_type == SublayerType::WAYPOINT) {
			/* Always create separator as now there is always at least the transform menu option. */
			item = gtk_menu_item_new();
			gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
			gtk_widget_show(item);

			/* Could be a right-click using the tool. */
			if (panel != NULL) {
				item = gtk_image_menu_item_new_with_mnemonic(_("_Goto"));
				gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU));
				g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_goto_waypoint), &pass_along);
				gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
				gtk_widget_show(item);
			}

			Waypoint * wp = this->waypoints.at(sublayer_uid);

			if (wp && wp->name) {
				if (is_valid_geocache_name(wp->name)) {
					item = gtk_menu_item_new_with_mnemonic(_("_Visit Geocache Webpage"));
					g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_waypoint_gc_webpage), &pass_along);
					gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
					gtk_widget_show(item);
				}
#ifdef VIK_CONFIG_GEOTAG
				item = gtk_menu_item_new_with_mnemonic(_("Geotag _Images..."));
				g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_geotagging_waypoint), &pass_along);
				gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
				gtk_widget_set_tooltip_text(item, _("Geotag multiple images against this waypoint"));
				gtk_widget_show(item);
#endif
			}

			if (wp && wp->image) {
				/* Set up image paramater. */
				pass_along.misc = wp->image;

				item = gtk_image_menu_item_new_with_mnemonic(_("_Show Picture..."));
				gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock("vik-icon-Show Picture", GTK_ICON_SIZE_MENU)); // Own icon - see stock_icons in vikwindow.c
				g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_show_picture), &pass_along);
				gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
				gtk_widget_show(item);

#ifdef VIK_CONFIG_GEOTAG
				GtkWidget *geotag_submenu = gtk_menu_new();
				item = gtk_image_menu_item_new_with_mnemonic(_("Update Geotag on _Image"));
				gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU));
				gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
				gtk_widget_show(item);
				gtk_menu_item_set_submenu(GTK_MENU_ITEM (item), geotag_submenu);

				item = gtk_menu_item_new_with_mnemonic(_("_Update"));
				g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_geotagging_waypoint_mtime_update), &pass_along);
				gtk_menu_shell_append(GTK_MENU_SHELL (geotag_submenu), item);
				gtk_widget_show(item);

				item = gtk_menu_item_new_with_mnemonic(_("Update and _Keep File Timestamp"));
				g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_geotagging_waypoint_mtime_keep), &pass_along);
				gtk_menu_shell_append(GTK_MENU_SHELL (geotag_submenu), item);
				gtk_widget_show(item);
#endif
			}

			if (wp)	{
				if (wp->url
				    || (wp->comment && !strncmp(wp->comment, "http", 4))
				    || (wp->description && !strncmp(wp->description, "http", 4))) {

					item = gtk_image_menu_item_new_with_mnemonic(_("Visit _Webpage"));
					gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_NETWORK, GTK_ICON_SIZE_MENU));
					g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_waypoint_webpage), &pass_along);
					gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
					gtk_widget_show(item);
				}
			}
		}
	}

	if (sublayer_type == SublayerType::WAYPOINTS || sublayer_type == SublayerType::TRACKS || sublayer_type == SublayerType::ROUTES) {
		item = gtk_image_menu_item_new_from_stock(GTK_STOCK_PASTE, NULL);
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_paste_item_cb), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
		/* TODO: only enable if suitable item is in clipboard - want to determine *which* sublayer type. */
		if (a_clipboard_type() == VIK_CLIPBOARD_DATA_SUBLAYER) {
			gtk_widget_set_sensitive(item, true);
		} else {
			gtk_widget_set_sensitive(item, false);
		}

		/* Add separator. */
		item = gtk_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
	}

	if (panel && (sublayer_type == SublayerType::WAYPOINTS || sublayer_type == SublayerType::WAYPOINT)) {
		rv = true;
		item = gtk_image_menu_item_new_with_mnemonic(_("_New Waypoint..."));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_NEW, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(new_waypoint_cb), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
	}

	if (sublayer_type == SublayerType::WAYPOINTS) {
		item = gtk_image_menu_item_new_with_mnemonic(_("_View All Waypoints"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(full_view_waypoints_cb), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("Goto _Waypoint..."));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(goto_waypoint_cb()), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("Delete _All Waypoints"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(delete_all_waypoints_cb), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Delete Waypoints From Selection..."));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(delete_selected_waypoints_cb), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		GtkWidget *vis_submenu = gtk_menu_new();
		item = gtk_menu_item_new_with_mnemonic(_("_Visibility"));
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM (item), vis_submenu);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Show All Waypoints"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_APPLY, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_waypoints_visibility_on), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (vis_submenu), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Hide All Waypoints"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_waypoints_visibility_off), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (vis_submenu), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Toggle"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_waypoints_visibility_toggle), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (vis_submenu), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("_List Waypoints..."));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_waypoint_list_dialog), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	}

	if (sublayer_type == SublayerType::TRACKS) {
		rv = true;

		if (this->current_track && !this->current_track->is_route) {
			item = gtk_menu_item_new_with_mnemonic(_("_Finish Track"));
			g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(finish_track_cb), &pass_along);
			gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
			gtk_widget_show(item);
			/* Add separator. */
			item = gtk_menu_item_new();
			gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
			gtk_widget_show(item);
		}

		item = gtk_image_menu_item_new_with_mnemonic(_("_View All Tracks"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(full_view_tracks_cb()), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("_New Track"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_NEW, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(new_track_cb), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
		/* Make it available only when a new track is *not* already in progress. */
		gtk_widget_set_sensitive(item, !(bool)KPOINTER_TO_INT(this->current_track));

		item = gtk_image_menu_item_new_with_mnemonic(_("Delete _All Tracks"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(delete_all_tracks_cb), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Delete Tracks From Selection..."));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(delete_selected_tracks_cb), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		GtkWidget *vis_submenu = gtk_menu_new();
		item = gtk_menu_item_new_with_mnemonic(_("_Visibility"));
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), vis_submenu);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Show All Tracks"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_APPLY, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_tracks_visibility_on), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (vis_submenu), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Hide All Tracks"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_tracks_visibility_off), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (vis_submenu), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Toggle"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_tracks_visibility_toggle), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (vis_submenu), item);

		item = gtk_image_menu_item_new_with_mnemonic(_("_List Tracks..."));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_track_list_dialog_single), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		item = gtk_menu_item_new_with_mnemonic(_("_Statistics"));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_tracks_stats), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
	}

	if (sublayer_type == SublayerType::ROUTES) {
		rv = true;

		if (this->current_track && this->current_track->is_route) {
			item = gtk_menu_item_new_with_mnemonic(_("_Finish Route"));
			/* Reuse finish track method. */
			g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(finish_track_cb), &pass_along);
			gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
			gtk_widget_show(item);
			/* Add separator. */
			item = gtk_menu_item_new();
			gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
			gtk_widget_show(item);
		}

		item = gtk_image_menu_item_new_with_mnemonic(_("_View All Routes"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(full_view_routes_cb), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("_New Route"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_NEW, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(new_route_cb), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
		/* Make it available only when a new track is *not* already in progress. */
		gtk_widget_set_sensitive(item, !(bool)KPOINTER_TO_INT(this->current_track));

		item = gtk_image_menu_item_new_with_mnemonic(_("Delete _All Routes"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(delete_all_routes_cb), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Delete Routes From Selection..."));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(delete_selected_routes_cb), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		GtkWidget *vis_submenu = gtk_menu_new();
		item = gtk_menu_item_new_with_mnemonic(_("_Visibility"));
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), vis_submenu);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Show All Routes"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_APPLY, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_routes_visibility_on), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (vis_submenu), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Hide All Routes"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_routes_visibility_off), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (vis_submenu), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Toggle"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_routes_visibility_toggle), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (vis_submenu), item);

		item = gtk_image_menu_item_new_with_mnemonic(_("_List Routes..."));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_track_list_dialog_single), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);

		gtk_widget_show(item);

		item = gtk_menu_item_new_with_mnemonic(_("_Statistics"));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_routes_stats), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
	}


	if (sublayer_type == SublayerType::WAYPOINTS || sublayer_type == SublayerType::TRACKS || sublayer_type == SublayerType::ROUTES) {
		GtkWidget *submenu_sort = gtk_menu_new();
		item = gtk_image_menu_item_new_with_mnemonic(_("_Sort"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU));
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu_sort);

		item = gtk_image_menu_item_new_with_mnemonic(_("Name _Ascending"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_SORT_ASCENDING, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_sort_order_a2z), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (submenu_sort), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("Name _Descending"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_SORT_DESCENDING, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_sort_order_z2a), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (submenu_sort), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("Date Ascending"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_SORT_ASCENDING, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_sort_order_timestamp_ascend), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (submenu_sort), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("Date Descending"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_SORT_DESCENDING, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_sort_order_timestamp_descend), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (submenu_sort), item);
		gtk_widget_show(item);
	}

	GtkWidget *upload_submenu = gtk_menu_new();

	if (sublayer_type == SublayerType::TRACK || sublayer_type == SublayerType::ROUTE) {
		item = gtk_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		if (this->current_track && sublayer_type == SublayerType::TRACK && !this->current_track->is_route) {
			item = gtk_menu_item_new_with_mnemonic(_("_Finish Track"));
		}

		if (this->current_track && sublayer_type == SublayerType::ROUTE && this->current_track->is_route) {
			item = gtk_menu_item_new_with_mnemonic(_("_Finish Route"));
		}

		if (this->current_track) {
			g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(finish_track_cb), &pass_along);
			gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
			gtk_widget_show(item);

			/* Add separator. */
			item = gtk_menu_item_new();
			gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
			gtk_widget_show(item);
		}

		if (sublayer_type == SublayerType::TRACK) {
			item = gtk_image_menu_item_new_with_mnemonic(_("_View Track"));
		} else {
			item = gtk_image_menu_item_new_with_mnemonic(_("_View Route"));
		}

		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_auto_track_view), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		item = gtk_menu_item_new_with_mnemonic(_("_Statistics"));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_track_statistics), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		GtkWidget *goto_submenu;
		goto_submenu = gtk_menu_new();
		item = gtk_image_menu_item_new_with_mnemonic(_("_Goto"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU));
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), goto_submenu);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Startpoint"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_GOTO_FIRST, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_goto_track_startpoint), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (goto_submenu), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("\"_Center\""));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_goto_track_center), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (goto_submenu), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Endpoint"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_GOTO_LAST, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_goto_track_endpoint), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (goto_submenu), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Highest Altitude"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_GOTO_TOP, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_goto_track_max_alt), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (goto_submenu), item);
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Lowest Altitude"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_GOTO_BOTTOM, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_goto_track_min_alt), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (goto_submenu), item);
		gtk_widget_show(item);

		/* Routes don't have speeds. */
		if (sublayer_type == SublayerType::TRACK) {
			item = gtk_image_menu_item_new_with_mnemonic(_("_Maximum Speed"));
			gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_MEDIA_FORWARD, GTK_ICON_SIZE_MENU));
			g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_goto_track_max_speed), &pass_along);
			gtk_menu_shell_append(GTK_MENU_SHELL (goto_submenu), item);
			gtk_widget_show(item);
		}

		GtkWidget *combine_submenu;
		combine_submenu = gtk_menu_new();
		item = gtk_image_menu_item_new_with_mnemonic(_("Co_mbine"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_CONNECT, GTK_ICON_SIZE_MENU));
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), combine_submenu);

		/* Routes don't have times or segments... */
		if (sublayer_type == SublayerType::TRACK) {
			item = gtk_menu_item_new_with_mnemonic(_("_Merge By Time..."));
			g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_merge_by_timestamp), &pass_along);
			gtk_menu_shell_append(GTK_MENU_SHELL (combine_submenu), item);
			gtk_widget_show(item);

			item = gtk_menu_item_new_with_mnemonic(_("Merge _Segments"));
			g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_merge_by_segment), &pass_along);
			gtk_menu_shell_append(GTK_MENU_SHELL (combine_submenu), item);
			gtk_widget_show(item);
		}

		item = gtk_menu_item_new_with_mnemonic(_("Merge _With Other Tracks..."));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_merge_with_other), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (combine_submenu), item);
		gtk_widget_show(item);

		if (sublayer_type == SublayerType::TRACK) {
			item = gtk_menu_item_new_with_mnemonic(_("_Append Track..."));
		} else {
			item = gtk_menu_item_new_with_mnemonic(_("_Append Route..."));
		}
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_append_track), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (combine_submenu), item);
		gtk_widget_show(item);

		if (sublayer_type == SublayerType::TRACK) {
			item = gtk_menu_item_new_with_mnemonic(_("Append _Route..."));
		} else {
			item = gtk_menu_item_new_with_mnemonic(_("Append _Track..."));
		}
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_append_other), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (combine_submenu), item);
		gtk_widget_show(item);

		GtkWidget *split_submenu;
		split_submenu = gtk_menu_new();
		item = gtk_image_menu_item_new_with_mnemonic(_("_Split"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_DISCONNECT, GTK_ICON_SIZE_MENU));
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM (item), split_submenu);

		/* Routes don't have times or segments... */
		if(sublayer_type == SublayerType::TRACK) {
			item = gtk_menu_item_new_with_mnemonic(_("_Split By Time..."));
			g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_split_by_timestamp), &pass_along);
			gtk_menu_shell_append(GTK_MENU_SHELL (split_submenu), item);
			gtk_widget_show(item);

			/* ATM always enable this entry - don't want to have to analyse the track before displaying the menu - to keep the menu speedy. */
			item = gtk_menu_item_new_with_mnemonic(_("Split Se_gments"));
			g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_split_segments), &pass_along);
			gtk_menu_shell_append(GTK_MENU_SHELL (split_submenu), item);
			gtk_widget_show(item);
		}

		item = gtk_menu_item_new_with_mnemonic(_("Split By _Number of Points..."));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_split_by_n_points), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (split_submenu), item);
		gtk_widget_show(item);

		item = gtk_menu_item_new_with_mnemonic(_("Split at _Trackpoint"));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_split_at_trackpoint), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (split_submenu), item);
		gtk_widget_show(item);
		/* Make it available only when a trackpoint is selected. */
		gtk_widget_set_sensitive(item, this->selected_tp.valid);

		GtkWidget *insert_submenu = gtk_menu_new();
		item = gtk_image_menu_item_new_with_mnemonic(_("_Insert Points"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU));
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), insert_submenu);

		item = gtk_menu_item_new_with_mnemonic(_("Insert Point _Before Selected Point"));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_insert_point_before), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (insert_submenu), item);
		gtk_widget_show(item);
		/* Make it available only when a point is selected. */
		gtk_widget_set_sensitive(item, this->selected_tp.valid);

		item = gtk_menu_item_new_with_mnemonic(_("Insert Point _After Selected Point"));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_insert_point_after), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (insert_submenu), item);
		gtk_widget_show(item);
		/* Make it available only when a point is selected. */
		gtk_widget_set_sensitive(item, this->selected_tp.valid);

		GtkWidget *delete_submenu;
		delete_submenu = gtk_menu_new();
		item = gtk_image_menu_item_new_with_mnemonic(_("Delete Poi_nts"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_DELETE, GTK_ICON_SIZE_MENU));
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), delete_submenu);

		item = gtk_image_menu_item_new_with_mnemonic(_("Delete _Selected Point"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_DELETE, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_delete_point_selected), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (delete_submenu), item);
		gtk_widget_show(item);
		/* Make it available only when a point is selected. */
		gtk_widget_set_sensitive(item, this->selected_tp.valid);

		item = gtk_menu_item_new_with_mnemonic(_("Delete Points With The Same _Position"));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_delete_points_same_position), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (delete_submenu), item);
		gtk_widget_show(item);

		item = gtk_menu_item_new_with_mnemonic(_("Delete Points With The Same _Time"));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_delete_points_same_time), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (delete_submenu), item);
		gtk_widget_show(item);

		GtkWidget *transform_submenu;
		transform_submenu = gtk_menu_new();
		item = gtk_image_menu_item_new_with_mnemonic(_("_Transform"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_CONVERT, GTK_ICON_SIZE_MENU));
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), transform_submenu);

		GtkWidget *dem_submenu;
		dem_submenu = gtk_menu_new();
		item = gtk_image_menu_item_new_with_mnemonic(_("_Apply DEM Data"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock("vik-icon-DEM Download", GTK_ICON_SIZE_MENU)); // Own icon - see stock_icons in vikwindow.c
		gtk_menu_shell_append(GTK_MENU_SHELL (transform_submenu), item);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), dem_submenu);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Overwrite"));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_apply_dem_data_all), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (dem_submenu), item);
		gtk_widget_set_tooltip_text(item, _("Overwrite any existing elevation values with DEM values"));
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Keep Existing"));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_apply_dem_data_only_missing), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (dem_submenu), item);
		gtk_widget_set_tooltip_text(item, _("Keep existing elevation values, only attempt for missing values"));
		gtk_widget_show(item);

		GtkWidget *smooth_submenu;
		smooth_submenu = gtk_menu_new();
		item = gtk_menu_item_new_with_mnemonic(_("_Smooth Missing Elevation Data"));
		gtk_menu_shell_append(GTK_MENU_SHELL (transform_submenu), item);
		gtk_widget_show(item);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), smooth_submenu);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Interpolated"));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_missing_elevation_data_interp), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (smooth_submenu), item);
		gtk_widget_set_tooltip_text(item, _("Interpolate between known elevation values to derive values for the missing elevations"));
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Flat"));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_missing_elevation_data_flat), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (smooth_submenu), item);
		gtk_widget_set_tooltip_text(item, _("Set unknown elevation values to the last known value"));
		gtk_widget_show(item);

		if (sublayer_type == SublayerType::TRACK) {
			item = gtk_image_menu_item_new_with_mnemonic(_("C_onvert to a Route"));
		} else {
			item = gtk_image_menu_item_new_with_mnemonic(_("C_onvert to a Track"));
		}
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_CONVERT, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_convert_track_route), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (transform_submenu), item);
		gtk_widget_show(item);

		/* Routes don't have timestamps - so these are only available for tracks. */
		if (sublayer_type == SublayerType::TRACK) {
			item = gtk_image_menu_item_new_with_mnemonic(_("_Anonymize Times"));
			g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_anonymize_times), &pass_along);
			gtk_menu_shell_append(GTK_MENU_SHELL (transform_submenu), item);
			gtk_widget_set_tooltip_text(item, _("Shift timestamps to a relative offset from 1901-01-01"));
			gtk_widget_show(item);

			item = gtk_image_menu_item_new_with_mnemonic(_("_Interpolate Times"));
			g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_interpolate_times), &pass_along);
			gtk_menu_shell_append(GTK_MENU_SHELL (transform_submenu), item);
			gtk_widget_set_tooltip_text(item, _("Reset trackpoint timestamps between the first and last points such that track is traveled at equal speed"));
			gtk_widget_show(item);
		}

		if (sublayer_type == SublayerType::TRACK) {
			item = gtk_image_menu_item_new_with_mnemonic(_("_Reverse Track"));
		} else {
			item = gtk_image_menu_item_new_with_mnemonic(_("_Reverse Route"));
		}
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_GO_BACK, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_reverse), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		if (sublayer_type == SublayerType::ROUTE) {
			item = gtk_image_menu_item_new_with_mnemonic(_("Refine Route..."));
			gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_FIND, GTK_ICON_SIZE_MENU));
			g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_route_refine), &pass_along);
			gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
			gtk_widget_show(item);
		}

		/* ATM This function is only available via the layers panel, due to the method in finding out the maps in use. */
		if (panel) {
			if (sublayer_type == SublayerType::TRACK) {
				item = gtk_image_menu_item_new_with_mnemonic(_("Down_load Maps Along Track..."));
			} else {
				item = gtk_image_menu_item_new_with_mnemonic(_("Down_load Maps Along Route..."));
			}
			gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock("vik-icon-Maps Download", GTK_ICON_SIZE_MENU)); /* Own icon - see stock_icons in vikwindow.c. */
			g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_download_map_along_track_cb), &pass_along);
			gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
			gtk_widget_show(item);
		}

		if (sublayer_type == SublayerType::TRACK) {
			item = gtk_image_menu_item_new_with_mnemonic(_("_Export Track as GPX..."));
		} else {
			item = gtk_image_menu_item_new_with_mnemonic(_("_Export Route as GPX..."));
		}
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_HARDDISK, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_export_gpx_track), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		if (sublayer_type == SublayerType::TRACK) {
			item = gtk_image_menu_item_new_with_mnemonic(_("E_xtend Track End"));
		} else {
			item = gtk_image_menu_item_new_with_mnemonic(_("E_xtend Route End"));
		}
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_extend_track_end), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);

		if (sublayer_type == SublayerType::ROUTE) {
			item = gtk_image_menu_item_new_with_mnemonic(_("Extend _Using Route Finder"));
			gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock("vik-icon-Route Finder", GTK_ICON_SIZE_MENU)); // Own icon - see stock_icons in vikwindow.c
			g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_extend_track_end_route_finder), &pass_along);
			gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
			gtk_widget_show(item);
		}

		/* ATM can't upload a single waypoint but can do waypoints to a GPS. */
		if (sublayer_type != SublayerType::WAYPOINT) {
			item = gtk_image_menu_item_new_with_mnemonic(_("_Upload"));
			gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_GO_UP, GTK_ICON_SIZE_MENU));
			gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
			gtk_widget_show(item);
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), upload_submenu);

			item = gtk_image_menu_item_new_with_mnemonic(_("_Upload to GPS..."));
			gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_MENU));
			g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_gps_upload_any), &pass_along);
			gtk_menu_shell_append(GTK_MENU_SHELL (upload_submenu), item);
			gtk_widget_show(item);
		}
	}

	GtkWidget *external_submenu = create_external_submenu(menu);

	/* These are only made available if a suitable program is installed. */
	if ((have_astro_program || have_diary_program)
	    && (sublayer_type == SublayerType::TRACK || sublayer_type == SublayerType::WAYPOINT)) {

		if (have_diary_program) {
			item = gtk_image_menu_item_new_with_mnemonic(_("_Diary"));
			gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_SPELL_CHECK, GTK_ICON_SIZE_MENU));
			g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_diary), &pass_along);
			gtk_menu_shell_append(GTK_MENU_SHELL (external_submenu), item);
			gtk_widget_set_tooltip_text(item, _("Open diary program at this date"));
			gtk_widget_show(item);
		}

		if (have_astro_program) {
			item = gtk_image_menu_item_new_with_mnemonic(_("_Astronomy"));
			g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_astro), &pass_along);
			gtk_menu_shell_append(GTK_MENU_SHELL (external_submenu), item);
			gtk_widget_set_tooltip_text(item, _("Open astronomy program at this date and location"));
			gtk_widget_show(item);
		}
	}

	if (this->selected_tp.valid || this->current_wp) {
		/* For the selected point. */
		VikCoord *vc;
		if (this->selected_tp.valid) {
			vc = &(*this->selected_tp.iter)->coord;
		} else {
			vc = &(this->current_wp->coord);
		}
		vik_ext_tools_add_menu_items_to_menu(this->get_window(), GTK_MENU (external_submenu), vc);
	} else {
		/* Otherwise for the selected sublayer.
		   TODO: Should use selected items centre - rather than implicitly using the current viewport. */
		vik_ext_tools_add_menu_items_to_menu(this->get_window(), GTK_MENU (external_submenu), NULL);
	}


#ifdef VIK_CONFIG_GOOGLE
	if (sublayer_type == SublayerType::ROUTE && (this->is_valid_google_route(sublayer_uid))) {
		item = gtk_image_menu_item_new_with_mnemonic(_("_View Google Directions"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_NETWORK, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_google_route_webpage), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
	}
#endif

	/* Some things aren't usable with routes. */
	if (sublayer_type == SublayerType::TRACK) {
#ifdef VIK_CONFIG_OPENSTREETMAP
		item = gtk_image_menu_item_new_with_mnemonic(_("Upload to _OSM..."));
		/* Convert internal pointer into track. */
		pass_along.misc = this->tracks.at(sublayer_uid);
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_GO_UP, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_osm_traces_upload_track_cb), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (upload_submenu), item);
		gtk_widget_show(item);
#endif

		/* Currently filter with functions all use shellcommands and thus don't work in Windows. */
#ifndef WINDOWS
		item = gtk_image_menu_item_new_with_mnemonic(_("Use with _Filter"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_INDEX, GTK_ICON_SIZE_MENU));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_track_use_with_filter), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
#endif

		/* ATM This function is only available via the layers panel, due to needing a panel. */
		if (panel) {
			item = a_acquire_track_menu(this->get_window(), (LayersPanel *) panel,
						    ((LayersPanel *) panel)->get_viewport(),
						    this->tracks.at(sublayer_uid));
			if (item) {
				gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
				gtk_widget_show(item);
			}
		}

#ifdef VIK_CONFIG_GEOTAG
		item = gtk_menu_item_new_with_mnemonic(_("Geotag _Images..."));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_geotagging_track), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
#endif
	}

	if (sublayer_type == SublayerType::TRACK || sublayer_type == SublayerType::ROUTE) {
		/* Only show on viewport popmenu when a trackpoint is selected. */
		if (!panel && this->selected_tp.valid) {
			/* Add separator. */
			item = gtk_menu_item_new();
			gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
			gtk_widget_show(item);

			item = gtk_image_menu_item_new_with_mnemonic(_("_Edit Trackpoint"));
			gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_MENU));
			g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_edit_trackpoint), &pass_along);
			gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
			gtk_widget_show(item);
		}
	}

	if (sublayer_type == SublayerType::WAYPOINTS || sublayer_type == SublayerType::WAYPOINT) {
		GtkWidget *transform_submenu;
		transform_submenu = gtk_menu_new();
		item = gtk_image_menu_item_new_with_mnemonic(_("_Transform"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_CONVERT, GTK_ICON_SIZE_MENU));
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		gtk_widget_show(item);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), transform_submenu);

		GtkWidget *dem_submenu;
		dem_submenu = gtk_menu_new();
		item = gtk_image_menu_item_new_with_mnemonic(_("_Apply DEM Data"));
		gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock("vik-icon-DEM Download", GTK_ICON_SIZE_MENU)); // Own icon - see stock_icons in vikwindow.c
		gtk_menu_shell_append(GTK_MENU_SHELL (transform_submenu), item);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), dem_submenu);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Overwrite"));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_apply_dem_data_wpt_all), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (dem_submenu), item);
		gtk_widget_set_tooltip_text(item, _("Overwrite any existing elevation values with DEM values"));
		gtk_widget_show(item);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Keep Existing"));
		g_signal_connect_swapped(G_OBJECT (item), "activate", G_CALLBACK(trw_layer_apply_dem_data_wpt_only_missing), &pass_along);
		gtk_menu_shell_append(GTK_MENU_SHELL (dem_submenu), item);
		gtk_widget_set_tooltip_text(item, _("Keep existing elevation values, only attempt for missing values"));
		gtk_widget_show(item);
	}

	gtk_widget_show_all(GTK_WIDGET (menu));

	return rv;
#endif
}