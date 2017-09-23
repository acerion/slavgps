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

#include "window.h"
#include "layers_panel.h"
#include "util.h"
#include "layer_trw.h"
#include "preferences.h"
#include "acquire.h"
#include "external_tools.h"
#include "vikexttool_datasources.h"
#include "track_internal.h"
#include "layer_trw_menu.h"




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
		if (this->current_trk->type_id == "sg.trw.route") {
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


	QMenu * submenu = a_acquire_trwlayer_menu(this->get_window(), this->get_window()->get_layers_panel(),
						  this->menu_data->viewport, this);
	if (submenu) {
		menu.addMenu(submenu);
	}

	submenu = a_acquire_trwlayer_track_menu(this->get_window(), this->get_window()->get_layers_panel(),
						this->menu_data->viewport, this);
	if (submenu) {
		menu.addMenu(submenu);
	}


	qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("&Tracks List..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_list_dialog_cb()));
	qa->setEnabled((bool) (this->tracks.items.size() + this->routes.items.size()));

	qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("&Waypoints List..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoint_list_dialog_cb()));
	qa->setEnabled((bool) (this->waypoints.items.size()));

	QMenu * external_submenu = menu.addMenu(QIcon::fromTheme("EXECUTE"), tr("Externa&l"));
	/* TODO: Should use selected layer's centre - rather than implicitly using the current viewport. */
	external_tools_add_menu_items_to_menu(this->get_window(), external_submenu, NULL);
}




void SlavGPS::layer_trw_sublayer_menu_waypoint_track_route_properties(LayerTRW * parent_layer, QMenu & menu)
{
	QAction * qa = NULL;

	qa = menu.addAction(QIcon::fromTheme("document-properties"), QObject::tr("&Properties"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (properties_item_cb()));

	if (parent_layer->menu_data->sublayer->type_id == "sg.trw.track") {
		const Track * trk = parent_layer->tracks.items.at(parent_layer->menu_data->sublayer->uid);
		if (trk && trk->properties_dialog) {
			qa->setEnabled(false);
		}
	}
	if (parent_layer->menu_data->sublayer->type_id == "sg.trw.route") {
		const Track * trk = parent_layer->routes.items.at(parent_layer->menu_data->sublayer->uid);
		if (trk && trk->properties_dialog) {
			qa->setEnabled(false);
		}
	}
}




void SlavGPS::layer_trw_sublayer_menu_track_route_profile(LayerTRW * parent_layer, QMenu & menu)
{
	QAction * qa = NULL;

	qa = menu.addAction(QIcon::fromTheme("document-properties"), QObject::tr("P&rofile"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (profile_item_cb()));

	if (parent_layer->menu_data->sublayer->type_id == "sg.trw.track") {
		const Track * trk = parent_layer->tracks.items.at(parent_layer->menu_data->sublayer->uid);
		if (trk && trk->profile_dialog) {
			qa->setEnabled(false);
		}
	}
	if (parent_layer->menu_data->sublayer->type_id == "sg.trw.route") {
		const Track * trk = parent_layer->routes.items.at(parent_layer->menu_data->sublayer->uid);
		if (trk && trk->profile_dialog) {
			qa->setEnabled(false);
		}
	}
}




void SlavGPS::layer_trw_sublayer_menu_waypoint_track_route_edit(LayerTRW * parent_layer, QMenu & menu)
{
	QAction * qa = NULL;

	qa = menu.addAction(QIcon::fromTheme("edit-cut"), QObject::tr("Cut"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (cut_sublayer_cb()));

	qa = menu.addAction(QIcon::fromTheme("edit-copy"), QObject::tr("Copy"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (copy_sublayer_cb()));

	qa = menu.addAction(QIcon::fromTheme("edit-delete"), QObject::tr("Delete"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (delete_sublayer_cb()));

	if (parent_layer->menu_data->sublayer->type_id == "sg.trw.waypoint") {

		/* Always create separator as now there is always at least the transform menu option. */
		menu.addSeparator();

		/* Could be a right-click using the tool. */
		if (parent_layer->get_window()->get_layers_panel() != NULL) {
			qa = menu.addAction(QIcon::fromTheme("go-jump"), QObject::tr("&Go to this Waypoint"));
			QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (go_to_selected_waypoint_cb()));
		}

		Waypoint * wp = parent_layer->waypoints.items.at(parent_layer->menu_data->sublayer->uid);

		if (wp && !wp->name.isEmpty()) {
			if (is_valid_geocache_name(wp->name.toUtf8().constData())) {
				qa = menu.addAction(QIcon::fromTheme("go-jump"), QObject::tr("&Visit Geocache Webpage"));
				QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (waypoint_geocache_webpage_cb()));
			}
#ifdef VIK_CONFIG_GEOTAG
			qa = menu.addAction(QIcon::fromTheme("go-jump"), QObject::tr("Geotag &Images..."));
			QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (geotagging_waypoint_cb()));
			qa->setToolTip(QObject::tr("Geotag multiple images against this waypoint"));
#endif
		}


		if (wp && !wp->image.isEmpty()) {
			/* Set up image parameter. */
			parent_layer->menu_data->string = wp->image;

			qa = menu.addAction(QIcon::fromTheme("vik-icon-Show Picture"), QObject::tr("&Show Picture...")); /* TODO: icon. */
			QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (show_picture_cb()));

#ifdef VIK_CONFIG_GEOTAG
			{
				QMenu * geotag_submenu = menu.addMenu(QIcon::fromTheme("view-refresh"), QObject::tr("Update Geotag on &Image"));

				qa = geotag_submenu->addAction(QObject::tr("&Update"));
				QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (geotagging_waypoint_mtime_update_cb()));

				qa = geotag_submenu->addAction(QObject::tr("Update and &Keep File Timestamp"));
				QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (geotagging_waypoint_mtime_keep_cb()));
			}
#endif
		}

		if (wp && wp->has_any_url()) {
			qa = menu.addAction(QIcon::fromTheme("applications-internet"), QObject::tr("Visit &Webpage"));
			QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (waypoint_webpage_cb()));
		}
	}
}




void SlavGPS::layer_trw_sublayer_menu_waypoints_tracks_routes_paste(LayerTRW * parent_layer, QMenu & menu)
{
	QAction * qa = NULL;

	qa = menu.addAction(QIcon::fromTheme("edit-paste"), QObject::tr("Paste"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (paste_sublayer_cb()));
#ifdef K
	/* TODO: only enable if suitable item is in clipboard - want to determine *which* sublayer type. */
	if (a_clipboard_type() == VIK_CLIPBOARD_DATA_SUBLAYER) {
		qa->setEnabled(true);
	} else {
		qa->setEnabled(false);
	}
#endif
}




void SlavGPS::layer_trw_sublayer_menu_waypoints_waypoint_new(LayerTRW * parent_layer, QMenu & menu)
{
	QAction * qa = NULL;

	qa = menu.addAction(QIcon::fromTheme("document-new"), QObject::tr("&New Waypoint..."));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (new_waypoint_cb()));

}




void SlavGPS::layer_trw_sublayer_menu_waypoints_A(LayerTRW * parent_layer, QMenu & menu)
{
	QAction * qa = NULL;

	qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), QObject::tr("&View All Waypoints"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (full_view_waypoints_cb()));

	qa = menu.addAction(QIcon::fromTheme("edit-find"), QObject::tr("Find &Waypoint..."));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (find_waypoint_dialog_cb()));

	qa = menu.addAction(QIcon::fromTheme("list-remove"), QObject::tr("Delete &All Waypoints"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (delete_all_waypoints_cb()));

	qa = menu.addAction(QObject::tr("&Delete Waypoints From Selection..."));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (delete_selected_waypoints_cb()));

	{
		QMenu * vis_submenu = menu.addMenu(QObject::tr("&Visibility"));

		qa = vis_submenu->addAction(QIcon::fromTheme("list-add"), QObject::tr("&Show All Waypoints"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (waypoints_visibility_on_cb()));

		qa = vis_submenu->addAction(QIcon::fromTheme("list-remove"), QObject::tr("&Hide All Waypoints"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (waypoints_visibility_off_cb()));

		qa = vis_submenu->addAction(QObject::tr("&Toggle"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (waypoints_visibility_toggle_cb()));
	}

	qa = menu.addAction(QObject::tr("&Waypoints List..."));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (waypoint_list_dialog_cb()));
}




void SlavGPS::layer_trw_sublayer_menu_tracks_A(LayerTRW * parent_layer, QMenu & menu)
{
	QAction * qa = NULL;

	if (parent_layer->current_trk && parent_layer->current_trk->type_id == "sg.trw.track") {
		qa = menu.addAction(QObject::tr("&Finish Track"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (finish_track_cb()));

		menu.addSeparator();
	}

	qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), QObject::tr("&View All Tracks"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (full_view_tracks_cb()));

	qa = menu.addAction(QIcon::fromTheme("document-new"), QObject::tr("&New Track"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (new_track_cb()));
	/* Make it available only when a new track is *not* already in progress. */
	qa->setEnabled(!parent_layer->current_trk);

	qa = menu.addAction(QIcon::fromTheme("list-remove"), QObject::tr("Delete &All Tracks"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (delete_all_tracks_cb()));

	qa = menu.addAction(QObject::tr("&Delete Tracks From Selection..."));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (delete_selected_tracks_cb()));

	{
		QMenu * vis_submenu = menu.addMenu(QObject::tr("&Visibility"));

		qa = vis_submenu->addAction(QIcon::fromTheme("list-add"), QObject::tr("&Show All Tracks"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (tracks_visibility_on_cb()));

		qa = vis_submenu->addAction(QIcon::fromTheme("list-remove"), QObject::tr("&Hide All Tracks"));
		QObject::QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (tracks_visibility_off_cb()));

		qa = vis_submenu->addAction(QObject::tr("&Toggle"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (tracks_visibility_toggle_cb()));
	}

	qa = menu.addAction(QObject::tr("&Tracks List..."));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (track_list_dialog_single_cb()));

	qa = menu.addAction(QObject::tr("&Statistics"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (tracks_stats_cb()));
}




void SlavGPS::layer_trw_sublayer_menu_routes_A(LayerTRW * parent_layer, QMenu & menu)
{
	QAction * qa = NULL;

	if (parent_layer->current_trk && parent_layer->current_trk->type_id == "sg.trw.route") {
		qa = menu.addAction(QObject::tr("&Finish Route"));
		/* Reuse finish track method. */
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (finish_track_cb()));

		menu.addSeparator();
	}

	qa = menu.addAction(QIcon::fromTheme("ZOOM_FIT"), QObject::tr("&View All Routes"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (full_view_routes_cb()));

	qa = menu.addAction(QIcon::fromTheme("document-new"), QObject::tr("&New Route"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (new_route_cb()));
	/* Make it available only when a new track is *not* already in progress. */
	qa->setEnabled(!parent_layer->current_trk);

	qa = menu.addAction(QIcon::fromTheme("list-delete"), QObject::tr("Delete &All Routes"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (delete_all_routes_cb()));

	qa = menu.addAction(QIcon::fromTheme("INDEX"), QObject::tr("&Delete Routes From Selection..."));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (delete_selected_routes_cb()));

	{
		QMenu * vis_submenu = menu.addMenu(QObject::tr("&Visibility"));

		qa = vis_submenu->addAction(QIcon::fromTheme("list-add"), QObject::tr("&Show All Routes"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (routes_visibility_on_cb()));

		qa = vis_submenu->addAction(QIcon::fromTheme("list-delete"), QObject::tr("&Hide All Routes"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (routes_visibility_off_cb()));

		qa = vis_submenu->addAction(QIcon::fromTheme("view-refresh"), QObject::tr("&Toggle"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (routes_visibility_toggle_cb()));
	}

	qa = menu.addAction(QIcon::fromTheme("INDEX"), QObject::tr("&List Routes..."));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (track_list_dialog_single_cb()));


	qa = menu.addAction(QObject::tr("&Statistics"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (routes_stats_cb()));
}




void SlavGPS::layer_trw_sublayer_menu_tracks_routes_waypoints_sort(LayerTRW * parent_layer, QMenu & menu)
{
	QAction * qa = NULL;

	QMenu * sort_submenu = menu.addMenu(QIcon::fromTheme("view-refresh"), QObject::tr("&Sort"));

	qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-ascending"), QObject::tr("Name &Ascending"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (sort_order_a2z_cb()));

	qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-descending"), QObject::tr("Name &Descending"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (sort_order_z2a_cb()));

	qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-ascending"), QObject::tr("Date Ascending"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (sort_order_timestamp_ascend_cb()));

	qa = sort_submenu->addAction(QIcon::fromTheme("view-sort-descending"), QObject::tr("Date Descending"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (sort_order_timestamp_descend_cb()));
}




void SlavGPS::layer_trw_sublayer_menu_track_route_misc(LayerTRW * parent_layer, QMenu & menu, QMenu * upload_submenu)
{
	QAction * qa = NULL;

	if (parent_layer->current_trk && parent_layer->menu_data->sublayer->type_id == "sg.trw.track" && parent_layer->current_trk->type_id == "sg.trw.track") {
		qa = menu.addAction(QObject::tr("&Finish Track"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (finish_track_cb()));
		menu.addSeparator();
	} else if (parent_layer->current_trk && parent_layer->menu_data->sublayer->type_id == "sg.trw.route" && parent_layer->current_trk->type_id == "sg.trw.route") {
		qa = menu.addAction(QObject::tr("&Finish Route"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (finish_track_cb()));
		menu.addSeparator();
	}

	if (parent_layer->menu_data->sublayer->type_id == "sg.trw.track") {
		qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), QObject::tr("&View Track"));
	} else {
		qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), QObject::tr("&View Route"));
	}
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (auto_track_view_cb()));

	qa = menu.addAction(QObject::tr("&Statistics"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (track_statistics_cb()));

	{
		QMenu * goto_submenu = menu.addMenu(QIcon::fromTheme("go-jump"), QObject::tr("&Goto"));

		qa = goto_submenu->addAction(QIcon::fromTheme("go-first"), QObject::tr("&Startpoint"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (goto_track_startpoint_cb()));

		qa = goto_submenu->addAction(QIcon::fromTheme("go-jump"), QObject::tr("\"&Center\""));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (goto_track_center_cb()));

		qa = goto_submenu->addAction(QIcon::fromTheme("go-last"), QObject::tr("&Endpoint"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (goto_track_endpoint_cb()));

		qa = goto_submenu->addAction(QIcon::fromTheme("go-top"), QObject::tr("&Highest Altitude"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (goto_track_max_alt_cb()));

		qa = goto_submenu->addAction(QIcon::fromTheme("go-bottom"), QObject::tr("&Lowest Altitude"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (goto_track_min_alt_cb()));

		/* Routes don't have speeds. */
		if (parent_layer->menu_data->sublayer->type_id == "sg.trw.track") {
			qa = goto_submenu->addAction(QIcon::fromTheme("media-seek-forward"), QObject::tr("&Maximum Speed"));
			QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (goto_track_max_speed_cb()));
		}
	}

	{

		QMenu * combine_submenu = menu.addMenu(QIcon::fromTheme("CONNECT"), QObject::tr("Co&mbine"));

		/* Routes don't have times or segments... */
		if (parent_layer->menu_data->sublayer->type_id == "sg.trw.track") {
			qa = combine_submenu->addAction(QObject::tr("&Merge By Time..."));
			QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (merge_by_timestamp_cb()));

			qa = combine_submenu->addAction(QObject::tr("Merge &Segments"));
			QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (merge_by_segment_cb()));
		}

		qa = combine_submenu->addAction(QObject::tr("Merge &With Other Tracks..."));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (merge_with_other_cb()));

		if (parent_layer->menu_data->sublayer->type_id == "sg.trw.track") {
			qa = combine_submenu->addAction(QObject::tr("&Append Track..."));
		} else {
			qa = combine_submenu->addAction(QObject::tr("&Append Route..."));
		}
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (append_track_cb()));

		if (parent_layer->menu_data->sublayer->type_id == "sg.trw.track") {
			qa = combine_submenu->addAction(QObject::tr("Append &Route..."));
		} else {
			qa = combine_submenu->addAction(QObject::tr("Append &Track..."));
		}
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (append_other_cb()));
	}



	{
		QMenu * split_submenu = menu.addMenu(QIcon::fromTheme("DISCONNECT"), QObject::tr("&Split"));

		/* Routes don't have times or segments... */
		if (parent_layer->menu_data->sublayer->type_id == "sg.trw.track") {
			qa = split_submenu->addAction(QObject::tr("&Split By Time..."));
			QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (split_by_timestamp_cb()));

			/* ATM always enable parent_layer entry - don't want to have to analyse the track before displaying the menu - to keep the menu speedy. */
			qa = split_submenu->addAction(QObject::tr("Split Se&gments"));
			QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (split_segments_cb()));
		}

		qa = split_submenu->addAction(QObject::tr("Split By &Number of Points..."));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (split_by_n_points_cb()));

		qa = split_submenu->addAction(QObject::tr("Split at &Trackpoint"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (split_at_trackpoint_cb()));
		/* Make it available only when a trackpoint is selected. */
		qa->setEnabled(parent_layer->selected_tp.valid);
	}



	{
		QMenu * insert_submenu = menu.addMenu(QIcon::fromTheme("list-add"), QObject::tr("&Insert Points"));

		qa = insert_submenu->addAction(QIcon::fromTheme(""), QObject::tr("Insert Point &Before Selected Point"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (insert_point_before_cb()));
		/* Make it available only when a point is selected. */
		qa->setEnabled(parent_layer->selected_tp.valid);

		qa = insert_submenu->addAction(QIcon::fromTheme(""), QObject::tr("Insert Point &After Selected Point"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (insert_point_after_cb()));
		/* Make it available only when a point is selected. */
		qa->setEnabled(parent_layer->selected_tp.valid);
	}


	{
		QMenu * delete_submenu = menu.addMenu(QIcon::fromTheme("list-delete"), QObject::tr("Delete Poi&nts"));

		qa = delete_submenu->addAction(QIcon::fromTheme("list-delete"), QObject::tr("Delete &Selected Point"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (delete_point_selected_cb()));
		/* Make it available only when a point is selected. */
		qa->setEnabled(parent_layer->selected_tp.valid);

		qa = delete_submenu->addAction(QObject::tr("Delete Points With The Same &Position"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (delete_points_same_position_cb()));

		qa = delete_submenu->addAction(QObject::tr("Delete Points With The Same &Time"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (delete_points_same_time_cb()));
	}

	{
		QMenu * transform_submenu = menu.addMenu(QIcon::fromTheme("CONVERT"), QObject::tr("&Transform"));
		{
			QMenu * dem_submenu = transform_submenu->addMenu(QIcon::fromTheme("vik-icon-DEM Download"), QObject::tr("&Apply DEM Data"));

			qa = dem_submenu->addAction(QObject::tr("&Overwrite"));
			QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (apply_dem_data_all_cb()));
			qa->setToolTip(QObject::tr("Overwrite any existing elevation values with DEM values"));

			qa = dem_submenu->addAction(QObject::tr("&Keep Existing"));
			QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (apply_dem_data_only_missing_cb()));
			qa->setToolTip(QObject::tr("Keep existing elevation values, only attempt for missing values"));
		}

		{
			QMenu * smooth_submenu = transform_submenu->addMenu(QObject::tr("&Smooth Missing Elevation Data"));

			qa = smooth_submenu->addAction(QObject::tr("&Interpolated"));
			QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (missing_elevation_data_interp_cb()));
			qa->setToolTip(QObject::tr("Interpolate between known elevation values to derive values for the missing elevations"));

			qa = smooth_submenu->addAction(QObject::tr("&Flat"));
			QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (missing_elevation_data_flat_cb()));
			qa->setToolTip(QObject::tr("Set unknown elevation values to the last known value"));
		}

		if (parent_layer->menu_data->sublayer->type_id == "sg.trw.track") {
			qa = transform_submenu->addAction(QIcon::fromTheme("CONVERT"), QObject::tr("C&onvert to a Route"));
		} else {
			qa = transform_submenu->addAction(QIcon::fromTheme("CONVERT"), QObject::tr("C&onvert to a Track"));
		}
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (convert_track_route_cb()));

		/* Routes don't have timestamps - so these are only available for tracks. */
		if (parent_layer->menu_data->sublayer->type_id == "sg.trw.track") {
			qa = transform_submenu->addAction(QObject::tr("&Anonymize Times"));
			QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (anonymize_times_cb()));
			qa->setToolTip(QObject::tr("Shift timestamps to a relative offset from 1901-01-01"));

			qa = transform_submenu->addAction(QObject::tr("&Interpolate Times"));
			QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (interpolate_times_cb()));
			qa->setToolTip(QObject::tr("Reset trackpoint timestamps between the first and last points such that track is traveled at equal speed"));
		}
	}


	if (parent_layer->menu_data->sublayer->type_id == "sg.trw.track") {
		qa = menu.addAction(QIcon::fromTheme("go-back"), QObject::tr("&Reverse Track"));
	} else {
		qa = menu.addAction(QIcon::fromTheme("go-back"), QObject::tr("&Reverse Route"));
	}
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (reverse_cb()));

	if (parent_layer->menu_data->sublayer->type_id == "sg.trw.route") {
		qa = menu.addAction(QIcon::fromTheme("edit-find"), QObject::tr("Refine Route..."));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (route_refine_cb()));
	}

	/* ATM Parent_Layer function is only available via the layers panel, due to the method in finding out the maps in use. */
	if (parent_layer->get_window()->get_layers_panel()) {
		if (parent_layer->menu_data->sublayer->type_id == "sg.trw.track") {
			qa = menu.addAction(QIcon::fromTheme("vik-icon-Maps Download"), QObject::tr("Down&load Maps Along Track..."));
		} else {
			qa = menu.addAction(QIcon::fromTheme("vik-icon-Maps Download"), QObject::tr("Down&load Maps Along Route..."));
		}
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (download_map_along_track_cb()));
	}

	if (parent_layer->menu_data->sublayer->type_id == "sg.trw.track") {
		qa = menu.addAction(QIcon::fromTheme("document-save-as"), QObject::tr("&Export Track as GPX..."));
	} else {
		qa = menu.addAction(QIcon::fromTheme("document-save-as"), QObject::tr("&Export Route as GPX..."));
	}
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (export_gpx_track_cb()));

	if (parent_layer->menu_data->sublayer->type_id == "sg.trw.track") {
		qa = menu.addAction(QIcon::fromTheme("list-add"), QObject::tr("E&xtend Track End"));
	} else {
		qa = menu.addAction(QIcon::fromTheme("list-add"), QObject::tr("E&xtend Route End"));
	}
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (extend_track_end_cb()));

	if (parent_layer->menu_data->sublayer->type_id == "sg.trw.route") {
		qa = menu.addAction(QIcon::fromTheme("vik-icon-Route Finder"), QObject::tr("Extend &Using Route Finder"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (extend_track_end_route_finder_cb()));
	}

	/* ATM can't upload a single waypoint but can do waypoints to a GPS. */
	if (parent_layer->menu_data->sublayer->type_id != "sg.trw.waypoint") {

		qa = upload_submenu->addAction(QIcon::fromTheme("go-forward"), QObject::tr("&Upload to GPS..."));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (gps_upload_any_cb()));
	}
}




void SlavGPS::layer_trw_sublayer_menu_track_waypoint_diary_astro(LayerTRW * parent_layer, QMenu & menu, QMenu * external_submenu)
{
	QAction * qa = NULL;

	if (have_diary_program) {
		qa = external_submenu->addAction(QIcon::fromTheme("SPELL_CHECK"), QObject::tr("&Diary"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (diary_cb()));
		qa->setToolTip(QObject::tr("Open diary program at this date"));
	}

	if (have_astro_program) {
		qa = external_submenu->addAction(QObject::tr("&Astronomy"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (astro_cb()));
		qa->setToolTip(QObject::tr("Open astronomy program at this date and location"));
	}
}




void SlavGPS::layer_trw_sublayer_menu_all_add_external_tools(LayerTRW * parent_layer, QMenu & menu, QMenu * external_submenu)
{
	if (parent_layer->selected_tp.valid || parent_layer->current_wp) {
		/* For the selected point. */
		Coord * coord = NULL;
		if (parent_layer->selected_tp.valid) {
			coord = &(*parent_layer->selected_tp.iter)->coord;
		} else {
			coord = &parent_layer->current_wp->coord;
		}
		external_tools_add_menu_items_to_menu(parent_layer->get_window(), external_submenu, coord);
	} else {
		/* Otherwise for the selected sublayer.
		   TODO: Should use selected items centre - rather than implicitly using the current viewport. */
		external_tools_add_menu_items_to_menu(parent_layer->get_window(), external_submenu, NULL);
	}
}




void SlavGPS::layer_trw_sublayer_menu_route_google_directions(LayerTRW * parent_layer, QMenu & menu)
{
	QAction * qa = NULL;

	qa = menu.addAction(QIcon::fromTheme("applications-internet"), QObject::tr("&View Google Directions"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (google_route_webpage_cb()));
}




void SlavGPS::layer_trw_sublayer_menu_track_misc(LayerTRW * parent_layer, QMenu & menu, QMenu * upload_submenu)
{
	QAction * qa = NULL;

#ifdef VIK_CONFIG_OPENSTREETMAP
	qa = upload_submenu->addAction(QIcon::fromTheme("go-up"), QObject::tr("Upload to &OSM..."));
	/* Convert internal pointer into track. */
	parent_layer->menu_data->misc = parent_layer->tracks.items.at(parent_layer->menu_data->sublayer->uid);
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (osm_traces_upload_track_cb()));
#endif

	/* Currently filter with functions all use shellcommands and thus don't work in Windows. */
#ifndef WINDOWS
	qa = menu.addAction(QIcon::fromTheme("INDEX"), QObject::tr("Use with &Filter"));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (track_use_with_filter_cb()));
#endif


	/* ATM This function is only available via the layers panel, due to needing a panel. */
	if (parent_layer->get_window()->get_layers_panel()) {
		QMenu * submenu = a_acquire_track_menu(parent_layer->get_window(), parent_layer->get_window()->get_layers_panel(),
						       parent_layer->menu_data->viewport,
						       parent_layer->tracks.items.at(parent_layer->menu_data->sublayer->uid));
		if (submenu) {
			/* kamilFIXME: .addMenu() does not make menu take ownership of submenu. */
			menu.addMenu(submenu);
		}
	}

#ifdef VIK_CONFIG_GEOTAG
	qa = menu.addAction(QObject::tr("Geotag _Images..."));
	QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (geotagging_track_cb()));
#endif
}




void SlavGPS::layer_trw_sublayer_menu_track_route_edit_trackpoint(LayerTRW * parent_layer, QMenu & menu)
{
	QAction * qa = NULL;

	/* Only show on viewport popmenu when a trackpoint is selected. */
	if (!parent_layer->get_window()->get_layers_panel() && parent_layer->selected_tp.valid) {

		menu.addSeparator();

		qa = menu.addAction(QIcon::fromTheme("document-properties"), QObject::tr("&Edit Trackpoint"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (edit_trackpoint_cb()));
	}
}




void SlavGPS::layer_trw_sublayer_menu_waypoints_waypoint_transform(LayerTRW * parent_layer, QMenu & menu)
{
	QAction * qa = NULL;

	QMenu * transform_submenu = menu.addMenu(QIcon::fromTheme("CONVERT"), QObject::tr("&Transform"));
	{
		QMenu * dem_submenu = transform_submenu->addMenu(QIcon::fromTheme("vik-icon-DEM Download"), QObject::tr("&Apply DEM Data"));

		qa = dem_submenu->addAction(QObject::tr("&Overwrite"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (apply_dem_data_wpt_all_cb()));
		qa->setToolTip(QObject::tr("Overwrite any existing elevation values with DEM values"));

		qa = dem_submenu->addAction(QObject::tr("&Keep Existing"));
		QObject::connect(qa, SIGNAL (triggered(bool)), parent_layer, SLOT (apply_dem_data_wpt_only_missing_cb()));
		qa->setToolTip(QObject::tr("Keep existing elevation values, only attempt for missing values"));
	}
}




/* Panel can be NULL if necessary - i.e. right-click from a tool. */
/* Viewpoint is now available instead. */
bool LayerTRW::sublayer_add_menu_items(QMenu & menu)
{
	QAction * qa = NULL;
	bool rv = false;


	if (this->menu_data->sublayer->type_id == "sg.trw.waypoint"
	    || this->menu_data->sublayer->type_id == "sg.trw.track"
	    || this->menu_data->sublayer->type_id == "sg.trw.route") {

		rv = true;
		layer_trw_sublayer_menu_waypoint_track_route_properties(this, menu);
	}


	if (this->menu_data->sublayer->type_id == "sg.trw.track"
	    || this->menu_data->sublayer->type_id == "sg.trw.route") {

		layer_trw_sublayer_menu_track_route_profile(this, menu);
	}


	if (this->menu_data->sublayer->type_id == "sg.trw.waypoint"
	    || this->menu_data->sublayer->type_id == "sg.trw.track"
	    || this->menu_data->sublayer->type_id == "sg.trw.route") {

		layer_trw_sublayer_menu_waypoint_track_route_edit(this, menu);
	}


	if (this->menu_data->sublayer->type_id == "sg.trw.waypoints"
	    || this->menu_data->sublayer->type_id == "sg.trw.tracks"
	    || this->menu_data->sublayer->type_id == "sg.trw.routes") {

		layer_trw_sublayer_menu_waypoints_tracks_routes_paste(this, menu);
		menu.addSeparator();
	}


	if (this->get_window()->get_layers_panel() && (this->menu_data->sublayer->type_id == "sg.trw.waypoints" || this->menu_data->sublayer->type_id == "sg.trw.waypoint")) {
		rv = true;
		layer_trw_sublayer_menu_waypoints_waypoint_new(this, menu);
	}


	if (this->menu_data->sublayer->type_id == "sg.trw.waypoints") {
		layer_trw_sublayer_menu_waypoints_A(this, menu);
	}


	if (this->menu_data->sublayer->type_id == "sg.trw.tracks") {
		rv = true;
		layer_trw_sublayer_menu_tracks_A(this, menu);
	}


	if (this->menu_data->sublayer->type_id == "sg.trw.routes") {
		rv = true;
		layer_trw_sublayer_menu_routes_A(this, menu);
	}


	if (this->menu_data->sublayer->type_id == "sg.trw.waypoints"
	    || this->menu_data->sublayer->type_id == "sg.trw.tracks"
	    || this->menu_data->sublayer->type_id == "sg.trw.routes") {

		layer_trw_sublayer_menu_tracks_routes_waypoints_sort(this, menu);
	}


	QMenu * upload_submenu = menu.addMenu(QIcon::fromTheme("go-up"), tr("&Upload"));

	if (this->menu_data->sublayer->type_id == "sg.trw.track" || this->menu_data->sublayer->type_id == "sg.trw.route") {
		layer_trw_sublayer_menu_track_route_misc(this, menu, upload_submenu);
	}


	QMenu * external_submenu = menu.addMenu(QIcon::fromTheme("EXECUTE"), tr("Externa&l"));

	/* These are only made available if a suitable program is installed. */
	if ((have_astro_program || have_diary_program)
	    && (this->menu_data->sublayer->type_id == "sg.trw.track" || this->menu_data->sublayer->type_id == "sg.trw.waypoint")) {

		layer_trw_sublayer_menu_track_waypoint_diary_astro(this, menu, external_submenu);
	}


	layer_trw_sublayer_menu_all_add_external_tools(this, menu, external_submenu);


#ifdef VIK_CONFIG_GOOGLE
	if (this->menu_data->sublayer->type_id == "sg.trw.route" && (this->is_valid_google_route(this->menu_data->sublayer->uid))) {
		layer_trw_sublayer_menu_route_google_directions(this, menu);
	}
#endif


	/* Some things aren't usable with routes. */
	if (this->menu_data->sublayer->type_id == "sg.trw.track") {
		layer_trw_sublayer_menu_track_misc(this, menu, upload_submenu);
	}


	if (this->menu_data->sublayer->type_id == "sg.trw.track" || this->menu_data->sublayer->type_id == "sg.trw.route") {
		layer_trw_sublayer_menu_track_route_edit_trackpoint(this, menu);
	}


	if (this->menu_data->sublayer->type_id == "sg.trw.waypoints" || this->menu_data->sublayer->type_id == "sg.trw.waypoint") {
		layer_trw_sublayer_menu_waypoints_waypoint_transform(this, menu);
	}


	return rv;
}
