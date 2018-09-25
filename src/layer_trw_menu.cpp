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
#include "layer_trw_track_internal.h"
#include "layer_trw_menu.h"
#include "preferences.h"
#include "acquire.h"
#include "external_tools.h"
#include "external_tool_datasources.h"




using namespace SlavGPS;




extern Tree * g_tree;




#define VIK_CONFIG_GEONAMES
#define VIK_CONFIG_GEOTAG


#define POINTS 1
#define LINES 2

/* This is how it knows when you click if you are clicking close to a trackpoint. */
#define TRACKPOINT_SIZE_APPROX 5
#define WAYPOINT_SIZE_APPROX 5

#define MIN_STOP_LENGTH 15
#define MAX_STOP_LENGTH 86400




extern bool g_have_astro_program;
extern bool g_have_diary_program;
extern bool have_geojson_export;




void LayerTRW::add_menu_items(QMenu & menu)
{
	QAction * qa = NULL;

	menu.addSeparator();

	if (this->get_track_creation_in_progress()) {
		qa = menu.addAction(tr("&Finish Track"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (finish_track_cb()));
		menu.addSeparator();
	} else if (this->get_route_creation_in_progress()) {
		qa = menu.addAction(tr("&Finish Route"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (finish_route_cb()));
		menu.addSeparator();
	} else {
		; /* Neither Track nor Route is being created at the moment. */
	}


	qa = menu.addAction(QIcon::fromTheme("zoom-fit-best"), tr("&View Layer"));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (move_viewport_to_show_all_cb()));

	{
		QMenu * view_submenu = menu.addMenu(QIcon::fromTheme("edit-find"), tr("V&iew"));

		qa = view_submenu->addAction(tr("View All &Tracks"));
		connect(qa, SIGNAL (triggered(bool)), &this->tracks, SLOT (move_viewport_to_show_all_cb()));

		qa = view_submenu->addAction(tr("View All &Routes"));
		connect(qa, SIGNAL (triggered(bool)), &this->routes, SLOT (move_viewport_to_show_all_cb()));

		qa = view_submenu->addAction(tr("View All &Waypoints"));
		connect(qa, SIGNAL (triggered(bool)), &this->waypoints, SLOT (move_viewport_to_show_all_cb()));
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

		bool creation_in_progress = this->get_track_creation_in_progress() || this->get_route_creation_in_progress();

		qa = new_submenu->addAction(QIcon::fromTheme("document-new"), tr("New &Waypoint..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (new_waypoint_cb()));

		qa = new_submenu->addAction(QIcon::fromTheme("document-new"), tr("New &Track"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (new_track_cb()));
		/* Make it available only when a new track is *not* already in progress. */
		qa->setEnabled(!creation_in_progress);

		qa = new_submenu->addAction(QIcon::fromTheme("document-new"), tr("New &Route"));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (new_route_cb()));
		/* Make it available only when a new route is *not* already in progress. */
		qa->setEnabled(!creation_in_progress);
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

		qa = acquire_submenu->addAction(tr("From &OSM Traces..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_osm_cb()));

		qa = acquire_submenu->addAction(tr("From &My OSM Traces..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (acquire_from_osm_my_traces_cb()));

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

		ExternalToolDataSource::add_menu_items(acquire_submenu, this->get_window());
	}



	{
		QMenu * upload_submenu = menu.addMenu(QIcon::fromTheme("go-up"), tr("&Upload"));

		qa = upload_submenu->addAction(QIcon::fromTheme("go-next"), tr("Upload Layer to &GPS..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (upload_to_gps_cb()));

		qa = upload_submenu->addAction(QIcon::fromTheme("go-up"), tr("Upload Layer to &OSM..."));
		connect(qa, SIGNAL (triggered(bool)), this, SLOT (upload_to_osm_traces_cb()));
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




	Acquire::set_context(this->get_window(), g_tree->tree_get_items_tree(), g_tree->tree_get_main_viewport(), this, NULL);

	/* kamilFIXME: .addMenu() does not make menu take ownership of the submenu. */

	QMenu * submenu = Acquire::create_bfilter_layer_menu();
	if (submenu) {
		menu.addMenu(submenu);
	}

	submenu = Acquire::create_bfilter_layer_track_menu();
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
	/* TODO_LATER: Should use selected layer's centre - rather than implicitly using the current viewport. */
	ExternalTools::add_menu_items(external_submenu, this->get_window(), NULL);
}




void SlavGPS::layer_trw_sublayer_menu_all_add_external_tools(LayerTRW * parent_layer, QMenu * external_submenu)
{
	/* Try adding submenu items with external tools pre-configured for selected Trackpoint. */
	const Track * track = parent_layer->get_edited_track();
	if (track && track->selected_tp_iter.valid) {
		/* For the selected Trackpoint. */
		const Coord * coord = &(*track->selected_tp_iter.iter)->coord;
		ExternalTools::add_menu_items(external_submenu, parent_layer->get_window(), coord);
		return;
	}

	/* Try adding submenu items with external tools pre-configured for selected Waypoint. */
	const Waypoint * wp = parent_layer->get_edited_wp();
	if (wp) {
		/* For the selected Waypoint. */
		const Coord * coord = &wp->coord;
		ExternalTools::add_menu_items(external_submenu, parent_layer->get_window(), coord);
		return;
	}

	/* Otherwise add submenu items with external tools pre-configured for selected sublayer.
	   TODO_LATER: Should use selected items centre - rather than implicitly using the current viewport. */
	ExternalTools::add_menu_items(external_submenu, parent_layer->get_window(), NULL);
}




#ifdef K_OLD_IMPLEMENTATION
/* Saving original version of the function for possible future reference. */

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


	if (g_tree->tree_get_items_tree() && (this->menu_data->sublayer->type_id == "sg.trw.waypoints" || this->menu_data->sublayer->type_id == "sg.trw.waypoint")) {
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
	if ((g_have_astro_program || g_have_diary_program)
	    && (this->menu_data->sublayer->type_id == "sg.trw.track" || this->menu_data->sublayer->type_id == "sg.trw.waypoint")) {

		layer_trw_sublayer_menu_track_waypoint_diary_astro(this, menu, external_submenu);
	}


	layer_trw_sublayer_menu_all_add_external_tools(this, external_submenu);


#ifdef VIK_CONFIG_GOOGLE
	if (this->menu_data->sublayer->type_id == "sg.trw.route" && ((Track *) this->menu_data->sublayer)->is_valid_google_route())) {
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
#endif /* #ifdef K_OLD_IMPLEMENTATION */
