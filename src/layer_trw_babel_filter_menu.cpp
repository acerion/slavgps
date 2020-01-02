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




//#include "external_tool_datasources.h"
#include "layer_trw.h"
#include "layer_trw_babel_filter.h"
#include "layer_trw_track_internal.h"
#include "window.h"




using namespace SlavGPS;




#define SG_MODULE "LayerTRW Babel Filter Menu"




extern std::map<SGObjectTypeID, DataSource *, SGObjectTypeID::compare> g_babel_filters;
extern Track * g_babel_filter_track;
extern AcquireContext * g_acquire_context_b;



sg_ret LayerTRWBabelFilter::add_babel_filters_to_submenu(QMenu & menu, DataSourceInputType filter_type)
{
	for (auto iter = g_babel_filters.begin(); iter != g_babel_filters.end(); iter++) {
		const SGObjectTypeID filter_id = iter->first;
		DataSource * filter = iter->second;

		if (filter->input_type != filter_type) {
			qDebug() << SG_PREFIX_I << "Not adding filter" << filter->window_title << "to menu, type not matched";
			continue;
		}
		qDebug() << SG_PREFIX_I << "Adding filter" << filter->window_title << "to menu";

		QAction * action = new QAction(filter->window_title);

		/* The property will be used later to lookup a bfilter. */
		QVariant property;
		property.setValue(filter_id);
		action->setProperty("property_babel_filter_id", property);

		QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (apply_babel_filter_cb(void)));
		menu.addAction(action);
	}

	return sg_ret::ok;
}




sg_ret LayerTRWBabelFilter::add_babel_filters_for_layer_submenu(QMenu & submenu)
{
	Acquire::set_context(this->ctx.m_window, this->ctx.m_gisview, this->ctx.m_parent_layer, this->ctx.m_trw);
	Acquire::set_target(this->ctx.m_trw, nullptr);

	this->add_babel_filters_to_submenu(submenu, DataSourceInputType::TRWLayer);

	g_acquire_context_b->m_trk = g_babel_filter_track;
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
		if (sg_ret::ok != this->add_babel_filters_to_submenu(*filter_with_submenu, DataSourceInputType::TRWLayerWithTrack)) {
			return sg_ret::err;
		}
	}

	return sg_ret::ok;
}




/**
   @brief Create a "Filter" sub menu intended for rightclicking on a TRW track
*/
sg_ret LayerTRWBabelFilter::add_babel_filters_for_track_submenu(QMenu & submenu)
{
	Acquire::set_context(this->ctx.m_window, this->ctx.m_gisview, this->ctx.m_parent_layer, this->ctx.m_trw);
	Acquire::set_target(this->ctx.m_trw, this->ctx.m_trk);

	return this->add_babel_filters_to_submenu(submenu, DataSourceInputType::TRWLayerWithTrack);
}
