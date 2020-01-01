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
 */




#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <cassert>




#include "external_tool_datasources.h"
#include "external_tools.h"
#include "layer_trw.h"
#include "layer_trw_import.h"
#include "layer_trw_menu.h"
#include "layer_trw_track_internal.h"
#include "layers_panel.h"
#include "preferences.h"
#include "util.h"
#include "window.h"




using namespace SlavGPS;




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




sg_ret LayerTRW::menu_add_type_specific_operations(QMenu & menu, bool in_tree_view)
{
	QAction * qa = NULL;
	if (nullptr == this->layer_trw_importer) {
		Layer * parent_layer = this->get_owning_layer();
		this->layer_trw_importer = new LayerTRWImporter(this->get_window(), ThisApp::get_main_gis_view(), parent_layer);
	}


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



	QMenu * import_submenu = menu.addMenu(QIcon::fromTheme("go-down"), QObject::tr("&Import into this layer"));
	this->layer_trw_importer->add_import_into_existing_layer_submenu(*import_submenu);



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



	QMenu * filter_submenu = menu.addMenu(QIcon::fromTheme("TODO - icon"), QObject::tr("&Filter"));
	this->layer_trw_importer->add_babel_filters_for_layer_submenu(*filter_submenu);



	qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("&Tracks and Routes List..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (track_and_route_list_dialog_cb()));
	qa->setEnabled((bool) (this->tracks.size() + this->routes.size()));

	qa = menu.addAction(QIcon::fromTheme("INDEX"), tr("&Waypoints List..."));
	connect(qa, SIGNAL (triggered(bool)), this, SLOT (waypoint_list_dialog_cb()));
	qa->setEnabled((bool) (this->waypoints.size()));

	QMenu * external_submenu = menu.addMenu(QIcon::fromTheme("EXECUTE"), tr("Externa&l"));
	ExternalTools::add_menu_items(external_submenu, this->get_window()->get_main_gis_view(), NULL);

	return sg_ret::ok;
}




void SlavGPS::layer_trw_sublayer_menu_all_add_external_tools(LayerTRW * parent_layer, QMenu * external_submenu)
{
	GisViewport * gisview = parent_layer->get_window()->get_main_gis_view();


	/* Try adding submenu items with external tools pre-configured
	   for selected Trackpoint. */
	const Track * track = parent_layer->selected_track_get();
	if (track && 1 == track->get_selected_children().get_count()) {

		const TrackpointReference & tp_ref = track->get_selected_children().front();
		if (tp_ref.m_iter_valid) {
			const Coord coord = (*tp_ref.m_iter)->coord;
			ExternalTools::add_menu_items(external_submenu, gisview, &coord);
			return;
		}
	}


	/* There were no selected tracks (or at least no tracks, for
	   which we can get coordinates). Try adding submenu items
	   with external tools pre-configured for selected
	   Waypoint. */
	const Waypoint * wp = parent_layer->selected_wp_get();
	if (wp) {
		const Coord coord = wp->get_coord();
		ExternalTools::add_menu_items(external_submenu, gisview, &coord);
		return;
	}


	/* There were no selected waypoints. Add submenu items with
	   external tools pre-configured for selected sublayer. */
	ExternalTools::add_menu_items(external_submenu, gisview, NULL);
}
