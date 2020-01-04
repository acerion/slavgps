/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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




#include <QRunnable>
#include <QThreadPool>




#include "babel.h"
#include "datasources.h"
#include "dialog.h"
#include "geonames_search.h"
#include "gpx.h"
#include "layer_aggregate.h"
#include "layer_trw.h"
#include "layer_trw_babel_filter.h"
#include "layer_trw_track_internal.h"
#include "layers_panel.h"
#include "util.h"
#include "viewport_internal.h"
#include "widget_list_selection.h"
#include "window.h"





using namespace SlavGPS;




#define SG_MODULE "LayerTRW Babel Filter"




extern Babel babel;




std::map<SGObjectTypeID, DataSource *, SGObjectTypeID::compare> g_babel_filters;
Track * g_babel_filter_track = nullptr;




LayerTRWBabelFilter::LayerTRWBabelFilter(Window * window, GisViewport * gisview, Layer * parent_layer, LayerTRW * trw)
{
	/* Some tests to detect mixing of function arguments. */
	if (LayerKind::Aggregate != parent_layer->m_kind && LayerKind::GPS != parent_layer->m_kind) {
		qDebug() << SG_PREFIX_E << "Parent layer has wrong kind" << parent_layer->m_kind;
	}
	if (LayerKind::TRW != trw->m_kind) {
		qDebug() << SG_PREFIX_E << "'trw' layer has wrong kind" << trw->m_kind;
	}

	this->ctx.m_window = window;
	this->ctx.m_gisview = gisview;
	this->ctx.m_parent_layer = parent_layer;
	this->ctx.m_trw = trw;
}




void LayerTRWBabelFilter::apply_babel_filter_cb(void)
{
	QAction * qa = (QAction *) QObject::sender();

	QVariant property = qa->property("property_babel_filter_id");
	const SGObjectTypeID filter_id = property.value<SGObjectTypeID>();
	qDebug() << SG_PREFIX_I << "Callback called for babel filter" << filter_id;

	auto iter = g_babel_filters.find(filter_id);
	if (iter == g_babel_filters.end()) {
		qDebug() << SG_PREFIX_E << "Can't find babel filter with id" << filter_id;
		return;
	}

	AcquireContext acquire_context;
	acquire_context = this->ctx;
	Acquire::acquire_from_source(iter->second, iter->second->mode, acquire_context);

	return;
}




void LayerTRWBabelFilter::set_babel_filter_track(Track * trk)
{
	/*
	  TODO_LATER g_babel_filter_track will have to be un-set when
	  the track is deleted.
	*/
	if (g_babel_filter_track) {
		g_babel_filter_track->free();
	}

	g_babel_filter_track = trk;
	trk->ref();
}




void LayerTRWBabelFilter::init(void)
{
	/*** Input is LayerTRW. ***/
	LayerTRWBabelFilter::register_babel_filter(new BFilterSimplify());
	LayerTRWBabelFilter::register_babel_filter(new BFilterCompress());
	LayerTRWBabelFilter::register_babel_filter(new BFilterDuplicates());
	LayerTRWBabelFilter::register_babel_filter(new BFilterManual());

	/*** Input is a Track and a LayerTRW. ***/
	LayerTRWBabelFilter::register_babel_filter(new BFilterPolygon());
	LayerTRWBabelFilter::register_babel_filter(new BFilterExcludePolygon());
}




void LayerTRWBabelFilter::uninit(void)
{
	for (auto iter = g_babel_filters.begin(); iter != g_babel_filters.end(); iter++) {
		delete iter->second;
	}
}




sg_ret LayerTRWBabelFilter::register_babel_filter(DataSource * bfilter)
{
	if (bfilter->get_source_id().is_empty()) {
		qDebug() << SG_PREFIX_E << "bfilter with empty type id";
		return sg_ret::err;
	}

	auto iter = g_babel_filters.find(bfilter->get_source_id());
	if (iter != g_babel_filters.end()) {
		qDebug() << SG_PREFIX_E << "Duplicate bfilter with type id" << bfilter->get_source_id();
		return sg_ret::err;
	}

	qDebug() << SG_PREFIX_I << "Registering babel filter type id" << bfilter->get_source_id();
	g_babel_filters.insert({ bfilter->get_source_id(), bfilter });

	return sg_ret::err;
}
