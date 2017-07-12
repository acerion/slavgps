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

#include "layers_panel.h"
#include "util.h"
#include "layer_trw.h"
#include "preferences.h"
#include "acquire.h"
#include "vikexttools.h"
#include "vikexttool_datasources.h"




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
		if (this->current_trk->sublayer_type == SublayerType::ROUTE) {
			qa = menu.addAction(tr("&Finish Route"));
		} else {
			qa = menu.addAction(tr("&Finish Track"));
		}
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (finish_track_cb()));


		menu.addSeparator();
	}

	qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), tr("&View Layer"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (full_view_cb()));

	{
		QMenu * view_submenu = menu.addMenu(QIcon::fromTheme("edit-find"), tr("V&iew"));

		qa = view_submenu->addAction(tr("View All &Tracks"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (full_view_tracks_cb()));

		qa = view_submenu->addAction(tr("View All &Routes"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (full_view_routes_cb()));

		qa = view_submenu->addAction(tr("View All &Waypoints"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (full_view_waypoints_cb()));
	}

	qa = menu.addAction(QIcon::fromTheme("go-jump"), tr("&Goto Center of Layer"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (centerize_cb()));

	qa = menu.addAction(QIcon::fromTheme("edit-find"), tr("Find &Waypoint..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (find_waypoint_dialog_cb()));

	{
		QMenu * export_submenu = menu.addMenu(QIcon::fromTheme("document-save-as"), tr("&Export Layer"));

		qa = export_submenu->addAction(tr("Export as GPS&Point..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (export_as_gpspoint_cb()));

		qa = export_submenu->addAction(tr("Export as GPS&Mapper..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (export_as_gpsmapper_cb()));

		qa = export_submenu->addAction(tr("Export as &GPX..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (export_as_gpx_cb()));

		qa = export_submenu->addAction(tr("Export as &KML..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (export_as_kml_cb()));

		if (have_geojson_export) {
			qa = export_submenu->addAction(tr("Export as GEO&JSON..."));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (export_as_geojson_cb()));
		}

		qa = export_submenu->addAction(tr("Export via GPSbabel..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (export_via_babel_cb()));

		qa = export_submenu->addAction(tr("Open with External Program&1: %1").arg(Preferences::get_external_gpx_program_1()));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (open_with_external_gpx_1_cb()));

		qa = export_submenu->addAction(tr("Open with External Program&2: %1").arg(Preferences::get_external_gpx_program_2()));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (open_with_external_gpx_2_cb()));
	}



	{
		QMenu * new_submenu = menu.addMenu(QIcon::fromTheme("document-new"), tr("&New"));

		qa = new_submenu->addAction(QIcon::fromTheme("document-new"), tr("New &Waypoint..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (new_waypoint_cb()));

		qa = new_submenu->addAction(QIcon::fromTheme("document-new"), tr("New &Track"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (new_track_cb()));
		/* Make it available only when a new track is *not* already in progress. */
		qa->setEnabled(!this->current_trk);

		qa = new_submenu->addAction(QIcon::fromTheme("document-new"), tr("New &Route"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (new_route_cb()));
		/* Make it available only when a new route is *not* already in progress. */
		qa->setEnabled(!this->current_trk);
	}



#ifdef VIK_CONFIG_GEOTAG
	qa = menu.addAction(tr("Geotag &Images..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (geotag_images_cb()));
#endif



	{
		QMenu * acquire_submenu = menu.addMenu(QIcon::fromTheme("go-down"), tr("&Acquire"));

		qa = acquire_submenu->addAction(tr("From &GPS..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_gps_cb()));

		/* FIXME: only add menu when at least a routing engine has support for Directions. */
		qa = acquire_submenu->addAction(tr("From &Directions..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_routing_cb()));

#ifdef VIK_CONFIG_OPENSTREETMAP
		qa = acquire_submenu->addAction(tr("From &OSM Traces..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_osm_cb()));

		qa = acquire_submenu->addAction(tr("From &My OSM Traces..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_osm_my_traces_cb()));
#endif

		qa = acquire_submenu->addAction(tr("From &URL..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_url_cb()));

#ifdef VIK_CONFIG_GEONAMES
		{
			QMenu * wikipedia_submenu = acquire_submenu->addMenu(QIcon::fromTheme("list-add"), tr("From &Wikipedia Waypoints"));

			qa = wikipedia_submenu->addAction(QIcon::fromTheme("zoom-fit-best"), tr("Within &Layer Bounds"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_wikipedia_waypoints_layer_cb()));

			qa = wikipedia_submenu->addAction(QIcon::fromTheme("zoom-original"), tr("Within &Current View"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_wikipedia_waypoints_viewport_cb()));
		}
#endif


#ifdef VIK_CONFIG_GEOCACHES
		qa = acquire_submenu->addAction(tr("From Geo&caching..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_geocache_cb()));
#endif

#ifdef VIK_CONFIG_GEOTAG
		qa = acquire_submenu->addAction(tr("From Geotagged &Images..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_geotagged_images_cb()));
#endif

		qa = acquire_submenu->addAction(tr("From &File..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_file_cb()));
		qa->setToolTip(tr("Import File With GPS_Babel..."));

		vik_ext_tool_datasources_add_menu_items(acquire_submenu, this->get_window());
	}



	{
		QMenu * upload_submenu = menu.addMenu(QIcon::fromTheme("go-up"), tr("&Upload"));

		qa = upload_submenu->addAction(QIcon::fromTheme("go-next"), tr("Upload to &GPS..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (upload_to_gps_cb()));

#ifdef VIK_CONFIG_OPENSTREETMAP
		qa = upload_submenu->addAction(QIcon::fromTheme("go-up"), tr("Upload to &OSM..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (upload_to_osm_traces_cb()));
#endif
	}



	{
		QMenu * delete_submenu = menu.addMenu(QIcon::fromTheme("list-remove"), tr("De&lete"));

		qa = delete_submenu->addAction(QIcon::fromTheme("list-remove"), tr("Delete All &Tracks"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_all_tracks_cb()));

		qa = delete_submenu->addAction(/* QIcon::fromTheme(""), */ tr("Delete Tracks _From Selection..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_selected_tracks_cb()));

		qa = delete_submenu->addAction(QIcon::fromTheme("list-remove"), tr("Delete &All Routes"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_all_routes_cb()));

		qa = delete_submenu->addAction(/* QIcon::fromTheme(""), */ tr("&Delete Routes From Selection..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_selected_routes_cb()));

		qa = delete_submenu->addAction(QIcon::fromTheme("list-remove"), tr("Delete All &Waypoints"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_all_waypoints_cb()));

		qa = delete_submenu->addAction(/* QIcon::fromTheme(""), */ tr("Delete Waypoints From &Selection..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_selected_waypoints_cb()));
	}


	/* kamilFIXME: .addMenu() does not make menu take ownership of the submenu. */


	QMenu * submenu = a_acquire_trwlayer_menu(this->get_window(), this->menu_data->layers_panel,
						  this->menu_data->layers_panel->get_viewport(), this);
	if (submenu) {
		menu.addMenu(submenu);
	}

	submenu = a_acquire_trwlayer_track_menu(this->get_window(), this->menu_data->layers_panel,
						this->menu_data->layers_panel->get_viewport(), this);
	if (submenu) {
		menu.addMenu(submenu);
	}


	qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("&Tracks List..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_list_dialog_cb()));
	qa->setEnabled((bool) (this->tracks.size() + this->routes.size()));

	qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("&Waypoints List..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoint_list_dialog_cb()));
	qa->setEnabled((bool) (this->waypoints.size()));

	QMenu * external_submenu = menu.addMenu(QIcon::fromTheme("EXECUTE"), tr("Externa&l"));
	/* TODO: Should use selected layer's centre - rather than implicitly using the current viewport. */
	vik_ext_tools_add_menu_items_to_menu(this->get_window(), external_submenu, NULL);
}




/* Panel can be NULL if necessary - i.e. right-click from a tool. */
/* Viewpoint is now available instead. */
bool LayerTRW::sublayer_add_menu_items(QMenu & menu)
{
	QAction * qa = NULL;
	bool rv = false;

	if (this->menu_data->sublayer->sublayer_type == SublayerType::WAYPOINT
	    || this->menu_data->sublayer->sublayer_type == SublayerType::TRACK
	    || this->menu_data->sublayer->sublayer_type == SublayerType::ROUTE) {

		rv = true;

		qa = menu.addAction(QIcon::fromTheme("document-properties"), tr("&Properties"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (properties_item_cb()));

		if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK) {
			Track * trk = this->tracks.at(this->menu_data->sublayer->uid);
			if (trk && trk->property_dialog) {
				qa->setEnabled(false);
			}
		}
		if (this->menu_data->sublayer->sublayer_type == SublayerType::ROUTE) {
			Track * trk = this->routes.at(this->menu_data->sublayer->uid);
			if (trk && trk->property_dialog) {
				qa->setEnabled(false);
			}
		}
	}

	if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK
	    || this->menu_data->sublayer->sublayer_type == SublayerType::ROUTE) {

		qa = menu.addAction(QIcon::fromTheme("document-properties"), tr("P&rofile"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (profile_item_cb()));

		if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK) {
			Track * trk = this->tracks.at(this->menu_data->sublayer->uid);
			if (trk && trk->property_dialog) {
				qa->setEnabled(false);
			}
		}
		if (this->menu_data->sublayer->sublayer_type == SublayerType::ROUTE) {
			Track * trk = this->routes.at(this->menu_data->sublayer->uid);
			if (trk && trk->property_dialog) {
				qa->setEnabled(false);
			}
		}
	}

	if (this->menu_data->sublayer->sublayer_type == SublayerType::WAYPOINT
	    || this->menu_data->sublayer->sublayer_type == SublayerType::TRACK
	    || this->menu_data->sublayer->sublayer_type == SublayerType::ROUTE) {



		qa = menu.addAction(QIcon::fromTheme("edit-cut"), tr("Cut"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (cut_sublayer_cb()));

		qa = menu.addAction(QIcon::fromTheme("edit-copy"), tr("Copy"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (copy_sublayer_cb()));

		qa = menu.addAction(QIcon::fromTheme("edit-delete"), tr("Delete"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_sublayer_cb()));

		if (this->menu_data->sublayer->sublayer_type == SublayerType::WAYPOINT) {

			/* Always create separator as now there is always at least the transform menu option. */
			menu.addSeparator();

			/* Could be a right-click using the tool. */
			if (this->menu_data->layers_panel != NULL) {
				qa = menu.addAction(QIcon::fromTheme("go-jump"), tr("&Go to this Waypoint"));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (go_to_selected_waypoint_cb()));
			}

			Waypoint * wp = this->waypoints.at(this->menu_data->sublayer->uid);

			if (wp && wp->name) {
				if (is_valid_geocache_name(wp->name)) {
					qa = menu.addAction(QIcon::fromTheme("go-jump"), tr("&Visit Geocache Webpage"));
					connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoint_geocache_webpage_cb()));
				}
#ifdef VIK_CONFIG_GEOTAG
				qa = menu.addAction(QIcon::fromTheme("go-jump"), tr("Geotag &Images..."));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (geotagging_waypoint_cb()));
				qa->setToolTip(tr("Geotag multiple images against this waypoint"));
#endif
			}


			if (wp && wp->image) {
				/* Set up image parameter. */
				this->menu_data->misc = wp->image;

				qa = menu.addAction(QIcon::fromTheme("vik-icon-Show Picture"), tr("&Show Picture...")); /* TODO: icon. */
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (show_picture_cb()));

#ifdef VIK_CONFIG_GEOTAG
				{
					QMenu * geotag_submenu = menu.addMenu(QIcon::fromTheme("view-refresh"), tr("Update Geotag on &Image"));

					qa = geotag_submenu->addAction(tr("&Update"));
					connect(qa, SIGNAL (triggered(bool)), this, SLOT (geotagging_waypoint_mtime_update_cb()));

					qa = geotag_submenu->addAction(tr("Update and &Keep File Timestamp"));
					connect(qa, SIGNAL (triggered(bool)), this, SLOT (geotagging_waypoint_mtime_keep_cb()));
				}
#endif
			}

			if (wp)	{
				if (wp->url
				    || (wp->comment && !strncmp(wp->comment, "http", 4))
				    || (wp->description && !strncmp(wp->description, "http", 4))) {

					qa = menu.addAction(QIcon::fromTheme("applications-internet"), tr("Visit &Webpage"));
					connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoint_webpage_cb()));
				}
			}
		}
	}


	if (this->menu_data->sublayer->sublayer_type == SublayerType::WAYPOINTS
	    || this->menu_data->sublayer->sublayer_type == SublayerType::TRACKS
	    || this->menu_data->sublayer->sublayer_type == SublayerType::ROUTES) {

		qa = menu.addAction(QIcon::fromTheme("edit-paste"), tr("Paste"));
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


	if (this->menu_data->layers_panel && (this->menu_data->sublayer->sublayer_type == SublayerType::WAYPOINTS || this->menu_data->sublayer->sublayer_type == SublayerType::WAYPOINT)) {
		rv = true;
		qa = menu.addAction(QIcon::fromTheme("document-new"), tr("&New Waypoint..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (new_waypoint_cb()));
	}



	if (this->menu_data->sublayer->sublayer_type == SublayerType::WAYPOINTS) {
		qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), tr("&View All Waypoints"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (full_view_waypoints_cb()));

		qa = menu.addAction(QIcon::fromTheme("edit-find"), tr("Find &Waypoint..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (find_waypoint_dialog_cb()));

		qa = menu.addAction(QIcon::fromTheme("list-remove"), tr("Delete &All Waypoints"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_all_waypoints_cb()));

		qa = menu.addAction(tr("&Delete Waypoints From Selection..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_selected_waypoints_cb()));

		{
			QMenu * vis_submenu = menu.addMenu(tr("&Visibility"));

			qa = vis_submenu->addAction(QIcon::fromTheme("list-add"), tr("&Show All Waypoints"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoints_visibility_on_cb()));

			qa = vis_submenu->addAction(QIcon::fromTheme("list-remove"), tr("&Hide All Waypoints"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoints_visibility_off_cb()));

			qa = vis_submenu->addAction(tr("&Toggle"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoints_visibility_toggle_cb()));
		}

		qa = menu.addAction(tr("&Waypoints List..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoint_list_dialog_cb()));
	}



	if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACKS) {
		rv = true;

		if (this->current_trk && this->current_trk->sublayer_type == SublayerType::TRACK) {
			qa = menu.addAction(tr("&Finish Track"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (finish_track_cb()));

			menu.addSeparator();
		}

		qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), tr("&View All Tracks"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (full_view_tracks_cb()));

		qa = menu.addAction(QIcon::fromTheme("document-new"), tr("&New Track"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (new_track_cb()));
		/* Make it available only when a new track is *not* already in progress. */
		qa->setEnabled(!this->current_trk);

		qa = menu.addAction(QIcon::fromTheme("list-remove"), tr("Delete &All Tracks"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_all_tracks_cb()));

		qa = menu.addAction(tr("&Delete Tracks From Selection..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_selected_tracks_cb()));

		{
			QMenu * vis_submenu = menu.addMenu(tr("&Visibility"));

			qa = vis_submenu->addAction(QIcon::fromTheme("list-add"), tr("&Show All Tracks"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (tracks_visibility_on_cb()));

			qa = vis_submenu->addAction(QIcon::fromTheme("list-remove"), tr("&Hide All Tracks"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (tracks_visibility_off_cb()));

			qa = vis_submenu->addAction(tr("&Toggle"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (tracks_visibility_toggle_cb()));
		}

		qa = menu.addAction(tr("&Tracks List..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_list_dialog_single_cb()));

		qa = menu.addAction(tr("&Statistics"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (tracks_stats_cb()));
	}


	if (this->menu_data->sublayer->sublayer_type == SublayerType::ROUTES) {
		rv = true;

		if (this->current_trk && this->current_trk->sublayer_type == SublayerType::ROUTE) {
			qa = menu.addAction(tr("&Finish Route"));
			/* Reuse finish track method. */
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (finish_track_cb()));

			menu.addSeparator();
		}

		qa = menu.addAction(QIcon::fromTheme("ZOOM_FIT"), tr("&View All Routes"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (full_view_routes_cb()));

		qa = menu.addAction(QIcon::fromTheme("document-new"), tr("&New Route"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (new_route_cb()));
		/* Make it available only when a new track is *not* already in progress. */
		qa->setEnabled(!this->current_trk);

		qa = menu.addAction(QIcon::fromTheme("list-delete"), tr("Delete &All Routes"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_all_routes_cb()));

		qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("&Delete Routes From Selection..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_selected_routes_cb()));

		{
			QMenu * vis_submenu = menu.addMenu(tr("&Visibility"));

			qa = vis_submenu->addAction(QIcon::fromTheme("list-add"), tr("&Show All Routes"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (routes_visibility_on_cb()));

			qa = vis_submenu->addAction(QIcon::fromTheme("list-delete"), tr("&Hide All Routes"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (routes_visibility_off_cb()));

			qa = vis_submenu->addAction(QIcon::fromTheme("view-refresh"), tr("&Toggle"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (routes_visibility_toggle_cb()));
		}

		qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("&List Routes..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_list_dialog_single_cb()));


		qa = menu.addAction(tr("&Statistics"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (routes_stats_cb()));
	}


	if (this->menu_data->sublayer->sublayer_type == SublayerType::WAYPOINTS
	    || this->menu_data->sublayer->sublayer_type == SublayerType::TRACKS
	    || this->menu_data->sublayer->sublayer_type == SublayerType::ROUTES) {

		QMenu * sort_submenu = menu.addMenu(QIcon::fromTheme("view-refresh"), tr("&Sort"));

		qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-ascending"), tr("Name &Ascending"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (sort_order_a2z_cb()));

		qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-descending"), tr("Name &Descending"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (sort_order_z2a_cb()));

		qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-ascending"), tr("Date Ascending"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (sort_order_timestamp_ascend_cb()));

		qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-descending"), tr("Date Descending"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (sort_order_timestamp_descend_cb()));
	}

	QMenu * upload_submenu = menu.addMenu(QIcon::fromTheme("go-up"), tr("&Upload"));

	if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK || this->menu_data->sublayer->sublayer_type == SublayerType::ROUTE) {

		if (this->current_trk && this->menu_data->sublayer->sublayer_type == SublayerType::TRACK && this->current_trk->sublayer_type == SublayerType::TRACK) {
			qa = menu.addAction(tr("&Finish Track"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (finish_track_cb()));
			menu.addSeparator();
		} else if (this->current_trk && this->menu_data->sublayer->sublayer_type == SublayerType::ROUTE && this->current_trk->sublayer_type == SublayerType::ROUTE) {
			qa = menu.addAction(tr("&Finish Route"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (finish_track_cb()));
			menu.addSeparator();
		}

		if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK) {
			qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), tr("&View Track"));
		} else {
			qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), tr("&View Route"));
		}
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (auto_track_view_cb()));

		qa = menu.addAction(tr("&Statistics"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_statistics_cb()));

		{
			QMenu * goto_submenu = menu.addMenu(QIcon::fromTheme("go-jump"), tr("&Goto"));

			qa = goto_submenu->addAction(QIcon::fromTheme("go-first"), tr("&Startpoint"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (goto_track_startpoint_cb()));

			qa = goto_submenu->addAction(QIcon::fromTheme("go-jump"), tr("\"&Center\""));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (goto_track_center_cb()));

			qa = goto_submenu->addAction(QIcon::fromTheme("go-last"), tr("&Endpoint"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (goto_track_endpoint_cb()));

			qa = goto_submenu->addAction(QIcon::fromTheme("go-top"), tr("&Highest Altitude"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (goto_track_max_alt_cb()));

			qa = goto_submenu->addAction(QIcon::fromTheme("go-bottom"), tr("&Lowest Altitude"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (goto_track_min_alt_cb()));

			/* Routes don't have speeds. */
			if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK) {
				qa = goto_submenu->addAction(QIcon::fromTheme("media-seek-forward"), tr("&Maximum Speed"));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (goto_track_max_speed_cb()));
			}
		}

		{

			QMenu * combine_submenu = menu.addMenu(QIcon::fromTheme("CONNECT"), tr("Co&mbine"));

			/* Routes don't have times or segments... */
			if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK) {
				qa = combine_submenu->addAction(tr("&Merge By Time..."));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (merge_by_timestamp_cb()));

				qa = combine_submenu->addAction(tr("Merge &Segments"));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (merge_by_segment_cb()));
			}

			qa = combine_submenu->addAction(tr("Merge &With Other Tracks..."));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (merge_with_other_cb()));

			if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK) {
				qa = combine_submenu->addAction(tr("&Append Track..."));
			} else {
				qa = combine_submenu->addAction(tr("&Append Route..."));
			}
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (append_track_cb()));

			if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK) {
				qa = combine_submenu->addAction(tr("Append &Route..."));
			} else {
				qa = combine_submenu->addAction(tr("Append &Track..."));
			}
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (append_other_cb()));
		}



		{
			QMenu * split_submenu = menu.addMenu(QIcon::fromTheme("DISCONNECT"), tr("&Split"));

			/* Routes don't have times or segments... */
			if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK) {
				qa = split_submenu->addAction(tr("&Split By Time..."));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (split_by_timestamp_cb()));

				/* ATM always enable this entry - don't want to have to analyse the track before displaying the menu - to keep the menu speedy. */
				qa = split_submenu->addAction(tr("Split Se&gments"));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (split_segments_cb()));
			}

			qa = split_submenu->addAction(tr("Split By &Number of Points..."));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (split_by_n_points_cb()));

			qa = split_submenu->addAction(tr("Split at &Trackpoint"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (split_at_trackpoint_cb()));
			/* Make it available only when a trackpoint is selected. */
			qa->setEnabled(this->selected_tp.valid);
		}



		{
			QMenu * insert_submenu = menu.addMenu(QIcon::fromTheme("list-add"), tr("&Insert Points"));

			qa = insert_submenu->addAction(QIcon::fromTheme(""), tr("Insert Point &Before Selected Point"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (insert_point_before_cb()));
			/* Make it available only when a point is selected. */
			qa->setEnabled(this->selected_tp.valid);

			qa = insert_submenu->addAction(QIcon::fromTheme(""), tr("Insert Point &After Selected Point"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (insert_point_after_cb()));
			/* Make it available only when a point is selected. */
			qa->setEnabled(this->selected_tp.valid);
		}


		{
			QMenu * delete_submenu = menu.addMenu(QIcon::fromTheme("list-delete"), tr("Delete Poi&nts"));

			qa = delete_submenu->addAction(QIcon::fromTheme("list-delete"), tr("Delete &Selected Point"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_point_selected_cb()));
			/* Make it available only when a point is selected. */
			qa->setEnabled(this->selected_tp.valid);

			qa = delete_submenu->addAction(tr("Delete Points With The Same &Position"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_points_same_position_cb()));

			qa = delete_submenu->addAction(tr("Delete Points With The Same &Time"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (delete_points_same_time_cb()));
		}

		{
			QMenu * transform_submenu = menu.addMenu(QIcon::fromTheme("CONVERT"), tr("&Transform"));
			{
				QMenu * dem_submenu = transform_submenu->addMenu(QIcon::fromTheme("vik-icon-DEM Download"), tr("&Apply DEM Data"));

				qa = dem_submenu->addAction(tr("&Overwrite"));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (apply_dem_data_all_cb()));
				qa->setToolTip(tr("Overwrite any existing elevation values with DEM values"));

				qa = dem_submenu->addAction(tr("&Keep Existing"));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (apply_dem_data_only_missing_cb()));
				qa->setToolTip(tr("Keep existing elevation values, only attempt for missing values"));
			}

			{
				QMenu * smooth_submenu = transform_submenu->addMenu(tr("&Smooth Missing Elevation Data"));

				qa = smooth_submenu->addAction(tr("&Interpolated"));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (missing_elevation_data_interp_cb()));
				qa->setToolTip(tr("Interpolate between known elevation values to derive values for the missing elevations"));

				qa = smooth_submenu->addAction(tr("&Flat"));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (missing_elevation_data_flat_cb()));
				qa->setToolTip(tr("Set unknown elevation values to the last known value"));
			}

			if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK) {
				qa = transform_submenu->addAction(QIcon::fromTheme("CONVERT"), tr("C&onvert to a Route"));
			} else {
				qa = transform_submenu->addAction(QIcon::fromTheme("CONVERT"), tr("C&onvert to a Track"));
			}
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (convert_track_route_cb()));

			/* Routes don't have timestamps - so these are only available for tracks. */
			if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK) {
				qa = transform_submenu->addAction(tr("&Anonymize Times"));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (anonymize_times_cb()));
				qa->setToolTip(tr("Shift timestamps to a relative offset from 1901-01-01"));

				qa = transform_submenu->addAction(tr("&Interpolate Times"));
				connect(qa, SIGNAL (triggered(bool)), this, SLOT (interpolate_times_cb()));
				qa->setToolTip(tr("Reset trackpoint timestamps between the first and last points such that track is traveled at equal speed"));
			}
		}


		if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK) {
			qa = menu.addAction(QIcon::fromTheme("go-back"), tr("&Reverse Track"));
		} else {
			qa = menu.addAction(QIcon::fromTheme("go-back"), tr("&Reverse Route"));
		}
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (reverse_cb()));

		if (this->menu_data->sublayer->sublayer_type == SublayerType::ROUTE) {
			qa = menu.addAction(QIcon::fromTheme("edit-find"), tr("Refine Route..."));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (route_refine_cb()));
		}

		/* ATM This function is only available via the layers panel, due to the method in finding out the maps in use. */
		if (this->menu_data->layers_panel) {
			if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK) {
				qa = menu.addAction(QIcon::fromTheme("vik-icon-Maps Download"), tr("Down&load Maps Along Track..."));
			} else {
				qa = menu.addAction(QIcon::fromTheme("vik-icon-Maps Download"), tr("Down&load Maps Along Route..."));
			}
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (download_map_along_track_cb()));
		}

		if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK) {
			qa = menu.addAction(QIcon::fromTheme("document-save-as"), tr("&Export Track as GPX..."));
		} else {
			qa = menu.addAction(QIcon::fromTheme("document-save-as"), tr("&Export Route as GPX..."));
		}
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (export_gpx_track_cb()));

		if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK) {
			qa = menu.addAction(QIcon::fromTheme("list-add"), tr("E&xtend Track End"));
		} else {
			qa = menu.addAction(QIcon::fromTheme("list-add"), tr("E&xtend Route End"));
		}
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (extend_track_end_cb()));

		if (this->menu_data->sublayer->sublayer_type == SublayerType::ROUTE) {
			qa = menu.addAction(QIcon::fromTheme("vik-icon-Route Finder"), tr("Extend &Using Route Finder"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (extend_track_end_route_finder_cb()));
		}

		/* ATM can't upload a single waypoint but can do waypoints to a GPS. */
		if (this->menu_data->sublayer->sublayer_type != SublayerType::WAYPOINT) {

			qa = upload_submenu->addAction(QIcon::fromTheme("go-forward"), tr("&Upload to GPS..."));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (gps_upload_any_cb()));
		}
	}

	QMenu * external_submenu = menu.addMenu(QIcon::fromTheme("EXECUTE"), tr("Externa&l"));

	/* These are only made available if a suitable program is installed. */
	if ((have_astro_program || have_diary_program)
	    && (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK || this->menu_data->sublayer->sublayer_type == SublayerType::WAYPOINT)) {

		if (have_diary_program) {
			qa = external_submenu->addAction(QIcon::fromTheme("SPELL_CHECK"), tr("&Diary"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (diary_cb()));
			qa->setToolTip(tr("Open diary program at this date"));
		}

		if (have_astro_program) {
			qa = external_submenu->addAction(tr("&Astronomy"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (astro_cb()));
			qa->setToolTip(tr("Open astronomy program at this date and location"));
		}
	}

#ifdef K
	if (this->selected_tp.valid || this->current_wp) {
		/* For the selected point. */
		Coord * coord = NULL;
		if (this->selected_tp.valid) {
			coord = &(*this->selected_tp.iter)->coord;
		} else {
			coord = &(this->current_wp->coord);
		}
		vik_ext_tools_add_menu_items_to_menu(this->get_window(), GTK_MENU (external_submenu), coord);
	} else {
		/* Otherwise for the selected sublayer.
		   TODO: Should use selected items centre - rather than implicitly using the current viewport. */
		vik_ext_tools_add_menu_items_to_menu(this->get_window(), GTK_MENU (external_submenu), NULL);
	}
#endif


#ifdef VIK_CONFIG_GOOGLE
	if (this->menu_data->sublayer->sublayer_type == SublayerType::ROUTE && (this->is_valid_google_route(this->menu_data->sublayer->uid))) {
		qa = menu.addAction(QIcon::fromTheme("applications-internet"), tr("&View Google Directions"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (google_route_webpage_cb()));
	}
#endif

	/* Some things aren't usable with routes. */
	if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK) {
#ifdef VIK_CONFIG_OPENSTREETMAP
		qa = upload_submenu->addAction(QIcon::fromTheme("go-up"), tr("Upload to &OSM..."));
		/* Convert internal pointer into track. */
		this->menu_data->misc = this->tracks.at(this->menu_data->sublayer->uid);
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (osm_traces_upload_track_cb()));
#endif

		/* Currently filter with functions all use shellcommands and thus don't work in Windows. */
#ifndef WINDOWS
		qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("Use with &Filter"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_use_with_filter_cb()));
#endif


		/* ATM This function is only available via the layers panel, due to needing a panel. */
		if (this->menu_data->layers_panel) {
			QMenu * submenu = a_acquire_track_menu(this->get_window(), this->menu_data->layers_panel,
							       this->menu_data->viewport,
							       this->tracks.at(this->menu_data->sublayer->uid));
			if (submenu) {
				/* kamilFIXME: .addMenu() does not make menu take ownership of submenu. */
				menu.addMenu(submenu);
			}
		}

#ifdef VIK_CONFIG_GEOTAG
		qa = menu.addAction(tr("Geotag _Images..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (geotagging_track_cb()));
#endif
	}

	if (this->menu_data->sublayer->sublayer_type == SublayerType::TRACK || this->menu_data->sublayer->sublayer_type == SublayerType::ROUTE) {
		/* Only show on viewport popmenu when a trackpoint is selected. */
		if (!this->menu_data->layers_panel && this->selected_tp.valid) {

			menu.addSeparator();

			qa = menu.addAction(QIcon::fromTheme("document-properties"), tr("&Edit Trackpoint"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (edit_trackpoint_cb()));
		}
	}

	if (this->menu_data->sublayer->sublayer_type == SublayerType::WAYPOINTS || this->menu_data->sublayer->sublayer_type == SublayerType::WAYPOINT) {

		QMenu * transform_submenu = menu.addMenu(QIcon::fromTheme("CONVERT"), tr("&Transform"));
		{
			QMenu * dem_submenu = transform_submenu->addMenu(QIcon::fromTheme("vik-icon-DEM Download"), tr("&Apply DEM Data"));

			qa = dem_submenu->addAction(tr("&Overwrite"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (apply_dem_data_wpt_all_cb()));
			qa->setToolTip(tr("Overwrite any existing elevation values with DEM values"));

			qa = dem_submenu->addAction(tr("&Keep Existing"));
			connect(qa, SIGNAL (triggered(bool)), this, SLOT (apply_dem_data_wpt_only_missing_cb()));
			qa->setToolTip(tr("Keep existing elevation values, only attempt for missing values"));
		}
	}


	return rv;

}
