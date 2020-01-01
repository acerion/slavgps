/*
 * SlavGPS -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2006, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2013-2015, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2020, Kamil Ignacak <acerion@wp.pl>
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




#include "external_tool_datasources.h"
#include "layer_trw.h"
#include "layer_trw_track_internal.h"
#include "window.h"




using namespace SlavGPS;




#define SG_MODULE "LayerTRW Import Menu"




extern std::map<SGObjectTypeID, DataSource *, SGObjectTypeID::compare> g_babel_filters;
extern Track * g_babel_filter_track;
extern AcquireContext * g_acquire_context;




sg_ret LayerTRWImporter::add_import_into_existing_layer_submenu(QMenu & submenu)
{
	if (nullptr == this->m_existing_trw) {
		qDebug() << SG_PREFIX_E << "Trying to add submenu items when existing TRW is not set";
		return sg_ret::err;
	}


	QAction * qa = nullptr;

	qa = submenu.addAction(QObject::tr("From &GPS..."));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_into_existing_layer_from_gps_cb()));

	/* FIXME: only add menu when at least a routing engine has support for Directions. */
	qa = submenu.addAction(QObject::tr("From &Directions..."));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_into_existing_layer_from_routing_cb()));

	qa = submenu.addAction(QObject::tr("From &OSM Traces..."));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_into_existing_layer_from_osm_cb()));

	qa = submenu.addAction(QObject::tr("From &My OSM Traces..."));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_into_existing_layer_from_osm_my_traces_cb()));

	qa = submenu.addAction(QObject::tr("From &URL..."));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_into_existing_layer_from_url_cb()));

#ifdef VIK_CONFIG_GEONAMES
	{
		QMenu * wikipedia_submenu = submenu.addMenu(QIcon::fromTheme("list-add"), QObject::tr("From &Wikipedia Waypoints"));

		qa = wikipedia_submenu->addAction(QIcon::fromTheme("zoom-fit-best"), QObject::tr("Within &Layer Bounds"));
		QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_into_existing_layer_from_wikipedia_waypoints_layer_cb()));

		qa = wikipedia_submenu->addAction(QIcon::fromTheme("zoom-original"), QObject::tr("Within &Current View"));
		QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_into_existing_layer_from_wikipedia_waypoints_viewport_cb()));
	}
#endif


#ifdef VIK_CONFIG_GEOCACHES
	qa = submenu.addAction(QObject::tr("From Geo&caching..."));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_into_existing_layer_from_geocache_cb()));
#endif

#ifdef VIK_CONFIG_GEOTAG
	qa = submenu.addAction(QObject::tr("From Geotagged &Images..."));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_into_existing_layer_from_geotagged_images_cb()));
#endif

	qa = submenu.addAction(QObject::tr("From &File (With GPSBabel)..."));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_into_existing_layer_from_file_cb()));
	qa->setToolTip(QObject::tr("From &File (With GPSBabel)..."));

	ExternalToolDataSource::add_menu_items(submenu, this->m_existing_trw->get_window()->get_main_gis_view());

	return sg_ret::ok;
}





sg_ret LayerTRWImporter::add_import_into_new_layer_submenu(QMenu & submenu)
{
	QAction * qa = nullptr;


	qa = submenu.addAction(QObject::tr("From &GPS..."));
	qa->setToolTip(QObject::tr("Transfer data from a GPS device"));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_into_new_layer_from_gps_cb(void)));

	qa = submenu.addAction(QObject::tr("From &File (With GPSBabel)..."));
	qa->setToolTip(QObject::tr("Import File With GPSBabel..."));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_into_new_layer_from_file_cb(void)));

	qa = submenu.addAction(QObject::tr("&Directions..."));
	qa->setToolTip(QObject::tr("Get driving directions"));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_into_new_layer_from_routing_cb(void)));

	qa = submenu.addAction(QObject::tr("Import Geo&JSON File..."));
	qa->setToolTip(QObject::tr("Import GeoJSON file"));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_into_new_layer_from_geojson_cb(void)));

	qa = submenu.addAction(QObject::tr("&OSM Traces..."));
	qa->setToolTip(QObject::tr("Get traces from OpenStreetMap"));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_into_new_layer_from_osm_cb(void)));

	qa = submenu.addAction(QObject::tr("&My OSM Traces..."));
	qa->setToolTip(QObject::tr("Get Your Own Traces from OpenStreetMap"));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_into_new_layer_from_my_osm_cb(void)));

#ifdef VIK_CONFIG_GEONAMES
	qa = submenu.addAction(QObject::tr("From &Wikipedia Waypoints"));
	qa->setToolTip(QObject::tr("Create waypoints from Wikipedia items in the current view"));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_into_new_layer_from_wikipedia_cb(void)));
#endif


#ifdef VIK_CONFIG_GEOCACHES
	qa = submenu.addAction(QObject::tr("Geo&caches..."));
	qa->setToolTip(QObject::tr("Get Geocaches from geocaching.com"));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_into_new_layer_from_gc_cb(void)));
#endif

#ifdef VIK_CONFIG_GEOTAG
	qa = submenu.addAction(QObject::tr("From Geotagged &Images..."));
	qa->setToolTip(QObject::tr("Create waypoints from geotagged images"));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_into_new_layer_from_geotag_cb(void)));
#endif

	qa = submenu.addAction(QObject::tr("From &URL..."));
	qa->setToolTip(QObject::tr("Get a file from URL"));
	QObject::connect(qa, SIGNAL (triggered(bool)), this, SLOT (import_into_new_layer_from_url_cb(void)));

	ExternalToolDataSource::add_menu_items(submenu, this->m_gisview);

	return sg_ret::ok;
}




int LayerTRWImporter::create_babel_filter_menu(QMenu & menu, DataSourceInputType filter_type)
{
	int n = 0;
	for (auto iter = g_babel_filters.begin(); iter != g_babel_filters.end(); iter++) {
		const SGObjectTypeID filter_id = iter->first;
		DataSource * filter = iter->second;

		if (filter->input_type != filter_type) {
			qDebug() << SG_PREFIX_I << "Not adding filter" << filter->window_title << "to menu, type not matched";
			continue;
		}
		qDebug() << SG_PREFIX_I << "Adding filter" << filter->window_title << "to menu";

		QAction * action = new QAction(filter->window_title, g_acquire_context);

		/* The property will be used later to lookup a bfilter. */
		QVariant property;
		property.setValue(filter_id);
		action->setProperty("property_babel_filter_id", property);

		QObject::connect(action, SIGNAL (triggered(bool)), g_acquire_context, SLOT (filter_trwlayer_cb(void)));
		menu.addAction(action);
		n++;
	}

	return n;
}








sg_ret LayerTRWImporter::add_babel_filters_for_layer_submenu(QMenu & submenu)
{
	Acquire::set_context(this->m_window, this->m_gisview, this->m_parent_layer, this->m_existing_trw);
	Acquire::set_target(this->m_existing_trw, nullptr);

	this->create_babel_filter_menu(submenu, DataSourceInputType::TRWLayer);

	g_acquire_context->target_trk = g_babel_filter_track;
	if (nullptr == g_babel_filter_track) {
		/* Build empty submenu to suggest to user that it's
		   possible to select a track and do filtering with
		   the track. TODO_LATER: make the item inactive. */
		const QString menu_label = QObject::tr("Filter with selected track");
		submenu.addMenu(menu_label);
	} else {
		/* Create a sub menu intended for rightclicking on a
		   TRWLayer's menu called "Filter with Track
		   "TRACKNAME"..." */
		const QString menu_label = QObject::tr("Filter with %1").arg(g_babel_filter_track->get_name());
		QMenu * filter_with_submenu = submenu.addMenu(menu_label);
		if (0 == this->create_babel_filter_menu(*filter_with_submenu, DataSourceInputType::TRWLayerTrack)) {
			return sg_ret::err;
		}
	}

	return sg_ret::ok;
}




/**
   @brief Create a "Filter" sub menu intended for rightclicking on a TRW track
*/
sg_ret LayerTRWImporter::add_babel_filters_for_track_submenu(QMenu & submenu)
{
	Acquire::set_context(this->m_window, this->m_gisview, this->m_parent_layer, this->m_existing_trw);
	Acquire::set_target(this->m_existing_trw, this->m_babel_filter_trk);

	return 0 == this->create_babel_filter_menu(submenu, DataSourceInputType::Track) ? sg_ret::err : sg_ret::ok;
}
