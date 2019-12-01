/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013-2015, Rob Norris <rw_norris@hotmail.com>
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




#include <QDebug>
#include <QHeaderView>




#include "viewport_internal.h"
#include "acquire.h"
#include "geonames_search.h"
#include "util.h"
#include "datasource_wikipedia.h"
#include "babel.h"
#include "globals.h"
#include "layer_trw.h"
#include "widget_list_selection.h"




using namespace SlavGPS;




#define SG_MODULE "DataSource Wikipedia"





DataSourceWikipedia::DataSourceWikipedia()
{
	qDebug() << SG_PREFIX_I;

	this->window_title = QObject::tr("Create Waypoints from Wikipedia Articles");
	this->layer_title = QObject::tr("Wikipedia Waypoints");
	this->mode = DataSourceMode::AutoLayerManagement;
	this->input_type = DataSourceInputType::None;
	this->autoview = false;
	this->keep_dialog_open = false; /* false = don't keep dialog open after success. Not even using the dialog. */

	qDebug() << SG_PREFIX_I;


	this->list_selection_widget = new ListSelectionWidget(ListSelectionMode::SingleItem, NULL);
	this->list_selection_widget->set_headers(ListSelectionWidget::get_headers_for_geoname());
#if 0



	ListSelectionMode selection_mode = ListSelectionMode::SingleItem;
	switch (selection_mode) {
	case ListSelectionMode::SingleItem:
		this->list_selection_widget->setSelectionMode(QAbstractItemView::SingleSelection);
		break;
	case ListSelectionMode::MultipleItems:
		this->list_selection_widget->setSelectionMode(QAbstractItemView::ExtendedSelection);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled selection mode" << (int) selection_mode;
		break;
	};

	this->list_selection_widget->horizontalHeader()->setStretchLastSection(true);
	this->list_selection_widget->verticalHeader()->setVisible(false);
	this->list_selection_widget->setWordWrap(false);
	this->list_selection_widget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	this->list_selection_widget->setTextElideMode(Qt::ElideRight);

	this->list_selection_widget->setShowGrid(false);
	this->list_selection_widget->setModel(&this->list_selection_widget->model);

	this->list_selection_widget->setSelectionMode(QAbstractItemView::ExtendedSelection);
	this->list_selection_widget->setSelectionBehavior(QAbstractItemView::SelectRows);

	qDebug() << SG_PREFIX_I;
#endif
}




SGObjectTypeID DataSourceWikipedia::get_source_id(void) const
{
	return DataSourceWikipedia::source_id();
}
SGObjectTypeID DataSourceWikipedia::source_id(void)
{
	return SGObjectTypeID("sg.datasource.wikipedia");
}




/**
   Process selected files and try to generate waypoints storing them in the given trw.
*/
LoadStatus DataSourceWikipedia::acquire_into_layer(LayerTRW * trw, AcquireContext * acquire_context, AcquireProgressDialog * progr_dialog)
{
	if (!trw) {
		qDebug() << SG_PREFIX_E << "Missing TRW layer";
		return LoadStatus::Code::InternalError;
	}

	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@  context" << (quintptr) acquire_context;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@    layer" << (quintptr) acquire_context->target_trw;
	qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@  gisview" << (quintptr) acquire_context->gisview;


	qDebug() << "---- will generate all geonames list";
	std::list<Geoname *> all_geonames = Geonames::generate_geonames(acquire_context->gisview->get_bbox(), progr_dialog);
	if (0 == all_geonames.size()) {
		/* Not an error situation. Info for user has been displayed in progr_dialog. */
		return LoadStatus::Code::Success;
	}

	emit progr_dialog->set_central_widget(this->list_selection_widget);
	sleep(1);

	qDebug() << "---- will show all geonames list for selection";
	qDebug() << SG_PREFIX_I;
	//const QStringList headers = { QObject::tr("Select the articles you want to add.") };
	std::list<Geoname *> selected = Geonames::select_geonames(all_geonames, progr_dialog, *this->list_selection_widget);

	qDebug() << "---- returned with selected geonames";

	for (auto iter = selected.begin(); iter != selected.end(); iter++) {

		qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@  context" << (quintptr) acquire_context;
		qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@    layer" << (quintptr) acquire_context->target_trw;
		qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@  gisview" << (quintptr) acquire_context->gisview;

		const Geoname * geoname = *iter;
		Waypoint * wp = geoname->create_waypoint(trw->get_coord_mode());
		trw->add_waypoint(wp);

		qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@  context" << (quintptr) acquire_context;
		qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@    layer" << (quintptr) acquire_context->target_trw;
		qDebug() << SG_PREFIX_I << "@@@@@@@@@@@@@@@@  gisview" << (quintptr) acquire_context->gisview;
	}

	return LoadStatus::Code::Success;
}





int DataSourceWikipedia::run_config_dialog(AcquireContext * acquire_context)
{
	/* Fake acquire options, needed by current implementation of acquire.cpp. */
	this->acquire_options = new AcquireOptions;

	return QDialog::Accepted;
}




AcquireProgressDialog * DataSourceWikipedia::create_progress_dialog(const QString & title)
{
	AcquireProgressDialog * progress_dialog = DataSource::create_progress_dialog(title);
	progress_dialog->list_selection_widget = this->list_selection_widget;

	return progress_dialog;
}
