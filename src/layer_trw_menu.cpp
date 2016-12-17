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
#include "track_profile_dialog.h"
#include "viktrwlayer_analysis.h"
#include "track_list_dialog.h"
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




void LayerTRW::add_menu_items(QMenu & menu)
{
	QAction * qa = NULL;

	menu.addSeparator();

	if (this->current_trk) {
		if (this->current_trk->is_route) {
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

	qa = menu.addAction(QIcon::fromTheme("edit-find"), QString(_("Find &Waypoint...")));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (find_waypoint_dialog_cb()));

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
		qa->setEnabled(!this->current_trk);

		qa = new_submenu->addAction(QIcon::fromTheme("document-new"), QString(_("New &Route")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (new_route_cb()));
		/* Make it available only when a new route is *not* already in progress. */
		qa->setEnabled(!this->current_trk);
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
	item = a_acquire_trwlayer_menu(this->get_window(), this->menu_data->layers_panel,
				       this->menu_data->layers_panel->get_viewport(), this);
	if (item) {
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	}

	item = a_acquire_trwlayer_track_menu(this->get_window(), this->menu_data->layers_panel,
					     this->menu_data->layers_panel->get_viewport(), this);
	if (item) {
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	}
#endif

	qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("&Tracks List..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_list_dialog_cb()));
	qa->setEnabled((bool) (this->tracks.size() + this->routes.size()));

	qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("&Waypoints List..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoint_list_dialog_cb()));
	qa->setEnabled((bool) (this->waypoints.size()));

#ifdef K
	QMenu * external_submenu = menu.addMenu(QIcon::fromTheme("EXECUTE"), QString("Externa&l"));
	/* TODO: Should use selected layer's centre - rather than implicitly using the current viewport. */
	vik_ext_tools_add_menu_items_to_menu(this->get_window(), GTK_MENU (external_submenu), NULL);
#endif
}




/* Panel can be NULL if necessary - i.e. right-click from a tool. */
/* Viewpoint is now available instead. */
bool LayerTRW::sublayer_add_menu_items(QMenu & menu)
{
	QAction * qa = NULL;
	bool rv = false;

	if (this->menu_data->sublayer->type == SublayerType::WAYPOINT
	    || this->menu_data->sublayer->type == SublayerType::TRACK
	    || this->menu_data->sublayer->type == SublayerType::ROUTE) {

		rv = true;

		qa = menu.addAction(QIcon::fromTheme("document-properties"), QString(_("&Properties")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (properties_item_cb()));

		if (this->menu_data->sublayer->type == SublayerType::TRACK) {
			Track * trk = this->tracks.at(this->menu_data->sublayer->uid);
			if (trk && trk->property_dialog) {
				qa->setEnabled(false);
			}
		}
		if (this->menu_data->sublayer->type == SublayerType::ROUTE) {
			Track * trk = this->routes.at(this->menu_data->sublayer->uid);
			if (trk && trk->property_dialog) {
				qa->setEnabled(false);
			}
		}
	}

	if (this->menu_data->sublayer->type == SublayerType::TRACK
	    || this->menu_data->sublayer->type == SublayerType::ROUTE) {

		qa = menu.addAction(QIcon::fromTheme("document-properties"), QString(_("P&rofile")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (profile_item_cb()));

		if (this->menu_data->sublayer->type == SublayerType::TRACK) {
			Track * trk = this->tracks.at(this->menu_data->sublayer->uid);
			if (trk && trk->property_dialog) {
				qa->setEnabled(false);
			}
		}
		if (this->menu_data->sublayer->type == SublayerType::ROUTE) {
			Track * trk = this->routes.at(this->menu_data->sublayer->uid);
			if (trk && trk->property_dialog) {
				qa->setEnabled(false);
			}
		}
	}

	if (this->menu_data->sublayer->type == SublayerType::WAYPOINT
	    || this->menu_data->sublayer->type == SublayerType::TRACK
	    || this->menu_data->sublayer->type == SublayerType::ROUTE) {



		qa = menu.addAction(QIcon::fromTheme("edit-cut"), QString(_("Cut")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (cut_sublayer_cb()));

		qa = menu.addAction(QIcon::fromTheme("edit-copy"), QString(_("Copy")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (copy_sublayer_cb()));

		qa = menu.addAction(QIcon::fromTheme("edit-delete"), QString(_("Delete")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_sublayer_cb()));

		if (this->menu_data->sublayer->type == SublayerType::WAYPOINT) {

			/* Always create separator as now there is always at least the transform menu option. */
			menu.addSeparator();

			/* Could be a right-click using the tool. */
			if (this->menu_data->layers_panel != NULL) {
				qa = menu.addAction(QIcon::fromTheme("go-jump"), QString(_("&Go to this Waypoint")));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (go_to_selected_waypoint_cb()));
			}

			Waypoint * wp = this->waypoints.at(this->menu_data->sublayer->uid);

			if (wp && wp->name) {
				if (is_valid_geocache_name(wp->name)) {
					qa = menu.addAction(QIcon::fromTheme("go-jump"), QString(_("&Visit Geocache Webpage")));
					connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoint_geocache_webpage_cb()));
				}
#ifdef VIK_CONFIG_GEOTAG
				qa = menu.addAction(QIcon::fromTheme("go-jump"), QString(_("Geotag &Images...")));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (geotagging_waypoint_cb()));
				qa->setToolTip(QString(_("Geotag multiple images against this waypoint")));
#endif
			}


			if (wp && wp->image) {
				/* Set up image parameter. */
				this->menu_data->misc = wp->image;

				qa = menu.addAction(QIcon::fromTheme("vik-icon-Show Picture"), QString(_("&Show Picture..."))); /* TODO: icon. */
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (show_picture_cb()));

#ifdef VIK_CONFIG_GEOTAG
				{
					QMenu * geotag_submenu = menu.addMenu(QIcon::fromTheme("view-refresh"), QString("Update Geotag on &Image"));

					qa = geotag_submenu->addAction(QString(_("&Update")));
					connect(qa, SIGNAL (triggered(bool)), this, SLOT (geotagging_waypoint_mtime_update_cb()));

					qa = geotag_submenu->addAction(QString(_("Update and &Keep File Timestamp")));
					connect(qa, SIGNAL (triggered(bool)), this, SLOT (geotagging_waypoint_mtime_keep_cb()));
				}
#endif
			}

			if (wp)	{
				if (wp->url
				    || (wp->comment && !strncmp(wp->comment, "http", 4))
				    || (wp->description && !strncmp(wp->description, "http", 4))) {

					qa = menu.addAction(QIcon::fromTheme("applications-internet"), QString(_("Visit &Webpage")));
					connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoint_webpage_cb()));
				}
			}
		}
	}


	if (this->menu_data->sublayer->type == SublayerType::WAYPOINTS
	    || this->menu_data->sublayer->type == SublayerType::TRACKS
	    || this->menu_data->sublayer->type == SublayerType::ROUTES) {

		qa = menu.addAction(QIcon::fromTheme("edit-paste"), QString(_("Paste")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (paste_sublayer_cb()));
#ifdef K
		/* TODO: only enable if suitable item is in clipboard - want to determine *which* sublayer type. */
		if (a_clipboard_type() == VIK_CLIPBOARD_DATA_SUBLAYER) {
			qa->setEnabled(true);
		} else {
			qa->setEnabled(false);
		}
#endif

		menu.addSeparator();
	}


	if (this->menu_data->layers_panel && (this->menu_data->sublayer->type == SublayerType::WAYPOINTS || this->menu_data->sublayer->type == SublayerType::WAYPOINT)) {
		rv = true;
		qa = menu.addAction(QIcon::fromTheme("document-new"), QString(_("&New Waypoint...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (new_waypoint_cb()));
	}



	if (this->menu_data->sublayer->type == SublayerType::WAYPOINTS) {
		qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), QString(_("&View All Waypoints")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (full_view_waypoints_cb()));

		qa = menu.addAction(QIcon::fromTheme("edit-find"), QString(_("Find &Waypoint...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (find_waypoint_dialog_cb()));

		qa = menu.addAction(QIcon::fromTheme("list-remove"), QString(_("Delete &All Waypoints")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_all_waypoints_cb()));

		qa = menu.addAction(QString(_("&Delete Waypoints From Selection...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_selected_waypoints_cb()));

		{
			QMenu * vis_submenu = menu.addMenu(QString("&Visibility"));

			qa = vis_submenu->addAction(QIcon::fromTheme("list-add"), QString(_("&Show All Waypoints")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoints_visibility_on_cb()));

			qa = vis_submenu->addAction(QIcon::fromTheme("list-remove"), QString(_("&Hide All Waypoints")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoints_visibility_off_cb()));

			qa = vis_submenu->addAction(QString(_("&Toggle")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoints_visibility_toggle_cb()));
		}

		qa = menu.addAction(tr("&Waypoints List..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoint_list_dialog_cb()));
	}



	if (this->menu_data->sublayer->type == SublayerType::TRACKS) {
		rv = true;

		if (this->current_trk && !this->current_trk->is_route) {
			qa = menu.addAction(QString(_("&Finish Track")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (finish_track_cb()));

			menu.addSeparator();
		}

		qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), QString(_("&View All Tracks")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (full_view_tracks_cb()));

		qa = menu.addAction(QIcon::fromTheme("document-new"), QString(_("&New Track")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (new_track_cb()));
		/* Make it available only when a new track is *not* already in progress. */
		qa->setEnabled(!this->current_trk);

		qa = menu.addAction(QIcon::fromTheme("list-remove"), QString(_("Delete &All Tracks")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_all_tracks_cb()));

		qa = menu.addAction(QString(_("&Delete Tracks From Selection...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_selected_tracks_cb()));

		{
			QMenu * vis_submenu = menu.addMenu(QString("&Visibility"));

			qa = vis_submenu->addAction(QIcon::fromTheme("list-add"), QString(_("&Show All Tracks")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (tracks_visibility_on_cb()));

			qa = vis_submenu->addAction(QIcon::fromTheme("list-remove"), QString(_("&Hide All Tracks")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (tracks_visibility_off_cb()));

			qa = vis_submenu->addAction(QString(_("&Toggle")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (tracks_visibility_toggle_cb()));
		}

		qa = menu.addAction(tr("&Tracks List..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_list_dialog_single_cb()));

		qa = menu.addAction(QString(_("&Statistics")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (tracks_stats_cb()));
	}


	if (this->menu_data->sublayer->type == SublayerType::ROUTES) {
		rv = true;

		if (this->current_trk && this->current_trk->is_route) {
			qa = menu.addAction(QString(_("_Finish Route")));
			/* Reuse finish track method. */
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (finish_track_cb()));

			menu.addSeparator();
		}

		qa = menu.addAction(QIcon::fromTheme("ZOOM_FIT"), QString(_("_View All Routes")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (full_view_routes_cb()));

		qa = menu.addAction(QIcon::fromTheme("document-new"), QString(_("_New Route")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (new_route_cb()));
		/* Make it available only when a new track is *not* already in progress. */
		qa->setEnabled(!this->current_trk);

		qa = menu.addAction(QIcon::fromTheme("list-delete"), QString(_("Delete _All Routes")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_all_routes_cb()));

		qa = menu.addAction(QIcon::fromTheme("INDEX"), QString(_("_Delete Routes From Selection...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_selected_routes_cb()));

		{
			QMenu * vis_submenu = menu.addMenu(QString(_("_Visibility")));

			qa = vis_submenu->addAction(QIcon::fromTheme("list-add"), QString(_("_Show All Routes")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (routes_visibility_on_cb()));

			qa = vis_submenu->addAction(QIcon::fromTheme("list-delete"), QString(_("_Hide All Routes")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (routes_visibility_off_cb()));

			qa = vis_submenu->addAction(QIcon::fromTheme("view-refresh"), QString(_("_Toggle")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (routes_visibility_toggle_cb()));
		}

		qa = menu.addAction(QIcon::fromTheme("INDEX"), QString(_("_List Routes...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_list_dialog_single_cb()));


		qa = menu.addAction(QString(_("_Statistics")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (routes_stats_cb()));
	}


	if (this->menu_data->sublayer->type == SublayerType::WAYPOINTS
	    || this->menu_data->sublayer->type == SublayerType::TRACKS
	    || this->menu_data->sublayer->type == SublayerType::ROUTES) {

		QMenu * sort_submenu = menu.addMenu(QIcon::fromTheme("view-refresh"), QString(_("_Sort")));

		qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-ascending"), QString(_("Name &Ascending")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (sort_order_a2z_cb()));

		qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-descending"), QString(_("Name &Descending")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (sort_order_z2a_cb()));

		qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-ascending"), QString(_("Date Ascending")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (sort_order_timestamp_ascend_cb()));

		qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-descending"), QString(_("Date Descending")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (sort_order_timestamp_descend_cb()));
	}

	QMenu * upload_submenu = menu.addMenu(QIcon::fromTheme("go-up"), QString(_("&Upload")));

	if (this->menu_data->sublayer->type == SublayerType::TRACK || this->menu_data->sublayer->type == SublayerType::ROUTE) {

		if (this->current_trk && this->menu_data->sublayer->type == SublayerType::TRACK && !this->current_trk->is_route) {
			qa = menu.addAction(QString(_("_Finish Track")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (finish_track_cb()));
			menu.addSeparator();
		} else if (this->current_trk && this->menu_data->sublayer->type == SublayerType::ROUTE && this->current_trk->is_route) {
			qa = menu.addAction(QString(_("_Finish Route")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (finish_track_cb()));
			menu.addSeparator();
		}

		if (this->menu_data->sublayer->type == SublayerType::TRACK) {
			qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), QString(_("_View Track")));
		} else {
			qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), QString(_("_View Route")));
		}
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (auto_track_view_cb()));

		qa = menu.addAction(QString(_("_Statistics")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_statistics_cb()));

		{
			QMenu * goto_submenu = menu.addMenu(QIcon::fromTheme("go-jump"), QString(_("&Goto")));

			qa = goto_submenu->addAction(QIcon::fromTheme("go-first"), QString(_("&Startpoint")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (goto_track_startpoint_cb()));

			qa = goto_submenu->addAction(QIcon::fromTheme("go-jump"), QString(_("\"&Center\"")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (goto_track_center_cb()));

			qa = goto_submenu->addAction(QIcon::fromTheme("go-last"), QString(_("&Endpoint")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (goto_track_endpoint_cb()));

			qa = goto_submenu->addAction(QIcon::fromTheme("go-top"), QString(_("&Highest Altitude")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (goto_track_max_alt_cb()));

			qa = goto_submenu->addAction(QIcon::fromTheme("go-bottom"), QString(_("&Lowest Altitude")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (goto_track_min_alt_cb()));

			/* Routes don't have speeds. */
			if (this->menu_data->sublayer->type == SublayerType::TRACK) {
				qa = goto_submenu->addAction(QIcon::fromTheme("media-seek-forward"), QString(_("_Maximum Speed")));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (goto_track_max_speed_cb()));
			}
		}

		{

			QMenu * combine_submenu = menu.addMenu(QIcon::fromTheme("CONNECT"), QString(_("Co&mbine")));

			/* Routes don't have times or segments... */
			if (this->menu_data->sublayer->type == SublayerType::TRACK) {
				qa = combine_submenu->addAction(QString(_("&Merge By Time...")));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (merge_by_timestamp_cb()));

				qa = combine_submenu->addAction(QString(_("Merge &Segments")));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (merge_by_segment_cb()));
			}

			qa = combine_submenu->addAction(QString(_("Merge &With Other Tracks...")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (merge_with_other_cb()));

			if (this->menu_data->sublayer->type == SublayerType::TRACK) {
				qa = combine_submenu->addAction(QString(_("&Append Track...")));
			} else {
				qa = combine_submenu->addAction(QString(_("&Append Route...")));
			}
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (append_track_cb()));

			if (this->menu_data->sublayer->type == SublayerType::TRACK) {
				qa = combine_submenu->addAction(QString(_("Append _Route...")));
			} else {
				qa = combine_submenu->addAction(QString(_("Append _Track...")));
			}
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (append_other_cb()));
		}



		{
			QMenu * split_submenu = menu.addMenu(QIcon::fromTheme("DISCONNECT"), QString(_("&Split")));

			/* Routes don't have times or segments... */
			if (this->menu_data->sublayer->type == SublayerType::TRACK) {
				qa = split_submenu->addAction(QString(_("_Split By Time...")));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (split_by_timestamp_cb()));

				/* ATM always enable this entry - don't want to have to analyse the track before displaying the menu - to keep the menu speedy. */
				qa = split_submenu->addAction(QString(_("Split Se_gments")));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (split_segments_cb()));
			}

			qa = split_submenu->addAction(QString(_("Split By _Number of Points...")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (split_by_n_points_cb()));

			qa = split_submenu->addAction(QString(_("Split at _Trackpoint")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (split_at_trackpoint_cb()));
			/* Make it available only when a trackpoint is selected. */
			qa->setEnabled(this->selected_tp.valid);
		}



		{
			QMenu * insert_submenu = menu.addMenu(QIcon::fromTheme("list-add"), QString(_("&Insert Points")));

			qa = insert_submenu->addAction(QIcon::fromTheme(""), QString(_("Insert Point _Before Selected Point")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (insert_point_before_cb()));
			/* Make it available only when a point is selected. */
			qa->setEnabled(this->selected_tp.valid);

			qa = insert_submenu->addAction(QIcon::fromTheme(""), QString(_("Insert Point _After Selected Point")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (insert_point_after_cb()));
			/* Make it available only when a point is selected. */
			qa->setEnabled(this->selected_tp.valid);
		}


		{
			QMenu * delete_submenu = menu.addMenu(QIcon::fromTheme("list-delete"), QString(_("Delete Poi&nts")));

			qa = delete_submenu->addAction(QIcon::fromTheme("list-delete"), QString(_("Delete &Selected Point")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_point_selected_cb()));
			/* Make it available only when a point is selected. */
			qa->setEnabled(this->selected_tp.valid);

			qa = delete_submenu->addAction(QString(_("Delete Points With The Same _Position")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_points_same_position_cb()));

			qa = delete_submenu->addAction(QString(_("Delete Points With The Same _Time")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_points_same_time_cb()));
		}

		{
			QMenu * transform_submenu = menu.addMenu(QIcon::fromTheme("CONVERT"), QString(_("_Transform")));
			{
				QMenu * dem_submenu = transform_submenu->addMenu(QIcon::fromTheme("vik-icon-DEM Download"), QString(_("&Apply DEM Data")));

				qa = dem_submenu->addAction(QString(_("_Overwrite")));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (apply_dem_data_all_cb()));
				qa->setToolTip(QString(_("Overwrite any existing elevation values with DEM values")));

				qa = dem_submenu->addAction(QString(_("_Keep Existing")));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (apply_dem_data_only_missing_cb()));
				qa->setToolTip(QString(_("Keep existing elevation values, only attempt for missing values")));
			}

			{
				QMenu * smooth_submenu = transform_submenu->addMenu(QString(_("&Smooth Missing Elevation Data")));

				qa = smooth_submenu->addAction(QString(_("&Interpolated")));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (missing_elevation_data_interp_cb()));
				qa->setToolTip(QString(_("Interpolate between known elevation values to derive values for the missing elevations")));

				qa = smooth_submenu->addAction(QString(_("&Flat")));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (missing_elevation_data_flat_cb()));
				qa->setToolTip(QString(_("Set unknown elevation values to the last known value")));
			}

			if (this->menu_data->sublayer->type == SublayerType::TRACK) {
				qa = transform_submenu->addAction(QIcon::fromTheme("CONVERT"), QString(_("C_onvert to a Route")));
			} else {
				qa = transform_submenu->addAction(QIcon::fromTheme("CONVERT"), QString(_("C_onvert to a Track")));
			}
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (convert_track_route_cb()));

			/* Routes don't have timestamps - so these are only available for tracks. */
			if (this->menu_data->sublayer->type == SublayerType::TRACK) {
				qa = transform_submenu->addAction(QString(_("_Anonymize Times")));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (anonymize_times_cb()));
				qa->setToolTip(QString(_("Shift timestamps to a relative offset from 1901-01-01")));

				qa = transform_submenu->addAction(QString(_("_Interpolate Times")));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (interpolate_times_cb()));
				qa->setToolTip(QString(_("Reset trackpoint timestamps between the first and last points such that track is traveled at equal speed")));
			}
		}


		if (this->menu_data->sublayer->type == SublayerType::TRACK) {
			qa = menu.addAction(QIcon::fromTheme("go-back"), QString(_("_Reverse Track")));
		} else {
			qa = menu.addAction(QIcon::fromTheme("go-back"), QString(_("_Reverse Route")));
		}
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (reverse_cb()));

		if (this->menu_data->sublayer->type == SublayerType::ROUTE) {
			qa = menu.addAction(QIcon::fromTheme("edit-find"), QString(_("Refine Route...")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (route_refine_cb()));
		}

		/* ATM This function is only available via the layers panel, due to the method in finding out the maps in use. */
		if (this->menu_data->layers_panel) {
			if (this->menu_data->sublayer->type == SublayerType::TRACK) {
				qa = menu.addAction(QIcon::fromTheme("vik-icon-Maps Download"), QString(_("Down_load Maps Along Track...")));
			} else {
				qa = menu.addAction(QIcon::fromTheme("vik-icon-Maps Download"), QString(_("Down_load Maps Along Route...")));
			}
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (download_map_along_track_cb()));
		}

		if (this->menu_data->sublayer->type == SublayerType::TRACK) {
			qa = menu.addAction(QIcon::fromTheme("document-save-as"), QString(_("_Export Track as GPX...")));
		} else {
			qa = menu.addAction(QIcon::fromTheme("document-save-as"), QString(_("_Export Route as GPX...")));
		}
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (export_gpx_track_cb()));

		if (this->menu_data->sublayer->type == SublayerType::TRACK) {
			qa = menu.addAction(QIcon::fromTheme("list-add"), QString(_("E_xtend Track End")));
		} else {
			qa = menu.addAction(QIcon::fromTheme("list-add"), QString(_("E_xtend Route End")));
		}
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (extend_track_end_cb()));

		if (this->menu_data->sublayer->type == SublayerType::ROUTE) {
			qa = menu.addAction(QIcon::fromTheme("vik-icon-Route Finder"), QString(_("Extend _Using Route Finder")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (extend_track_end_route_finder_cb()));
		}

		/* ATM can't upload a single waypoint but can do waypoints to a GPS. */
		if (this->menu_data->sublayer->type != SublayerType::WAYPOINT) {

			qa = upload_submenu->addAction(QIcon::fromTheme("go-forward"), QString(_("_Upload to GPS...")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (gps_upload_any_cb()));
		}
	}

	QMenu * external_submenu = menu.addMenu(QIcon::fromTheme("EXECUTE"), QString("Externa&l"));

	/* These are only made available if a suitable program is installed. */
	if ((have_astro_program || have_diary_program)
	    && (this->menu_data->sublayer->type == SublayerType::TRACK || this->menu_data->sublayer->type == SublayerType::WAYPOINT)) {

		if (have_diary_program) {
			qa = external_submenu->addAction(QIcon::fromTheme("SPELL_CHECK"), QString(_("_Diary")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (diary_cb()));
			qa->setToolTip(QString(_("Open diary program at this date")));
		}

		if (have_astro_program) {
			qa = external_submenu->addAction(QString(_("_Astronomy")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (astro_cb()));
			qa->setToolTip(QString(_("Open astronomy program at this date and location")));
		}
	}

#ifdef K
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
#endif


#ifdef VIK_CONFIG_GOOGLE
	if (this->menu_data->sublayer->type == SublayerType::ROUTE && (this->is_valid_google_route(this->menu_data->sublayer->uid))) {
		qa = menu.addAction(QIcon::fromTheme("applications-internet"), QString(_("_View Google Directions")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (google_route_webpage_cb()));
	}
#endif

	/* Some things aren't usable with routes. */
	if (this->menu_data->sublayer->type == SublayerType::TRACK) {
#ifdef VIK_CONFIG_OPENSTREETMAP
		qa = upload_submenu->addAction(QIcon::fromTheme("go-up"), QString(_("Upload to _OSM...")));
		/* Convert internal pointer into track. */
		this->menu_data->misc = this->tracks.at(this->menu_data->sublayer->uid);
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (osm_traces_upload_track_cb()));
#endif

		/* Currently filter with functions all use shellcommands and thus don't work in Windows. */
#ifndef WINDOWS
		qa = menu.addAction(QIcon::fromTheme("INDEX"), QString(_("Use with &Filter")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_use_with_filter_cb()));
#endif


		/* ATM This function is only available via the layers panel, due to needing a panel. */
		if (this->menu_data->layers_panel) {
#ifdef K
			item = a_acquire_track_menu(this->get_window(), this->menu_data->layers_panel,
						    this->menu_data->viewport,
						    this->tracks.at(this->menu_data->sublayer->uid));
			if (item) {
				gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
			}
#endif
		}

#ifdef VIK_CONFIG_GEOTAG
		qa = menu.addAction(QString(_("Geotag _Images...")));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (geotagging_track_cb()));
#endif
	}

	if (this->menu_data->sublayer->type == SublayerType::TRACK || this->menu_data->sublayer->type == SublayerType::ROUTE) {
		/* Only show on viewport popmenu when a trackpoint is selected. */
		if (!this->menu_data->layers_panel && this->selected_tp.valid) {

			menu.addSeparator();

			qa = menu.addAction(QIcon::fromTheme("document-properties"), QString(_("_Edit Trackpoint")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (edit_trackpoint_cb()));
		}
	}

	if (this->menu_data->sublayer->type == SublayerType::WAYPOINTS || this->menu_data->sublayer->type == SublayerType::WAYPOINT) {

		QMenu * transform_submenu = menu.addMenu(QIcon::fromTheme("CONVERT"), QString(_("&Transform")));
		{
			QMenu * dem_submenu = transform_submenu->addMenu(QIcon::fromTheme("vik-icon-DEM Download"), QString(_("&Apply DEM Data")));

			qa = dem_submenu->addAction(QString(_("_Overwrite")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (apply_dem_data_wpt_all_cb()));
			qa->setToolTip(QString(_("Overwrite any existing elevation values with DEM values")));

			qa = dem_submenu->addAction(QString(_("_Keep Existing")));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (apply_dem_data_wpt_only_missing_cb()));
			qa->setToolTip(QString(_("Keep existing elevation values, only attempt for missing values")));
		}
	}


	return rv;

}
