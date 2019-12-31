/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2008, Hein Ragas <viking@ragas.nl>
 * Copyright (C) 2010-2014, Rob Norris <rw_norris@hotmail.com>
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




#include <QHeaderView>




#include "widget_list_selection.h"
#include "tree_view.h"
#include "geonames_search.h"
#include "layer_trw_track_internal.h"
#include "layer_trw_waypoint.h"
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "List Selection Widget"




ListSelectionWidget::ListSelectionWidget(ListSelectionMode selection_mode, QWidget * a_parent) : QTableView(a_parent)
{
	switch (selection_mode) {
	case ListSelectionMode::SingleItem:
		this->setSelectionMode(QAbstractItemView::SingleSelection);
		break;
	case ListSelectionMode::MultipleItems:
		this->setSelectionMode(QAbstractItemView::ExtendedSelection);
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unhandled selection mode" << (int) selection_mode;
		break;
	};

	this->horizontalHeader()->setStretchLastSection(true);
	this->verticalHeader()->setVisible(false);
	this->setWordWrap(false);
	this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	this->setTextElideMode(Qt::ElideRight);

	this->setShowGrid(false);
	this->setModel(&this->model);

	this->setSelectionMode(QAbstractItemView::ExtendedSelection);
	this->setSelectionBehavior(QAbstractItemView::SelectRows);
}




ListSelectionWidget::~ListSelectionWidget()
{
	qDebug() << SG_PREFIX_I;
}




void ListSelectionWidget::set_headers(const QStringList & header_labels)
{
	//this->model.setItemPrototype(new SGItem());

	for (int i = 0; i < header_labels.size(); i++) {
		QStandardItem * header_item = new QStandardItem(header_labels.at(i));
		this->model.setHorizontalHeaderItem(i, header_item);

		this->horizontalHeader()->setSectionHidden(i, false);
		this->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Interactive);
	}

	return;
}




QStringList ListSelectionWidget::get_headers_for_track(void)
{
	QStringList headers;
	headers << QObject::tr("Name") << QObject::tr("Comment");
	return headers;
}




QStringList ListSelectionWidget::get_headers_for_waypoint(void)
{
	QStringList headers;
	headers << QObject::tr("Name") << QObject::tr("Comment");
	return headers;
}




QStringList ListSelectionWidget::get_headers_for_geoname(void)
{
	QStringList headers;
	headers << QObject::tr("Name") << QObject::tr("Feature") << QObject::tr("Lat/Lon");
	return headers;
}




QStringList ListSelectionWidget::get_headers_for_string(void)
{
	QStringList headers;
	headers << QObject::tr("Name");
	return headers;
}




ListSelectionRow::ListSelectionRow()
{

}




ListSelectionRow::ListSelectionRow(QString const & text_)
{
	QStandardItem * item = NULL;

	item = new QStandardItem(text_);
	item->setData(QVariant::fromValue(text_), RoleLayerData);
	item->setToolTip(text_);
	item->setEditable(false);
	this->items << item;
}




ListSelectionRow::ListSelectionRow(Geoname * geoname)
{
	QStandardItem * item = NULL;

	item = new QStandardItem(geoname->name);
	item->setData(QVariant::fromValue(geoname), RoleLayerData);
	if (!geoname->comment.isEmpty()) {
		item->setToolTip(geoname->comment);
	}
	item->setEditable(false);
	this->items << item;

	item = new QStandardItem(geoname->feature);
	item->setEditable(false);
	this->items << item;

	item = new QStandardItem(geoname->lat_lon.to_string());
	item->setEditable(false);
	this->items << item;
}




ListSelectionRow::ListSelectionRow(Track * trk)
{
	QStandardItem * item = NULL;

	item = new QStandardItem(trk->get_name());
	item->setData(QVariant::fromValue(trk), RoleLayerData);
	item->setToolTip(trk->get_tooltip());
	item->setEditable(false);
	this->items << item;

	item = new QStandardItem(trk->comment);
	item->setEditable(false);
	this->items << item;
}




ListSelectionRow::ListSelectionRow(Waypoint * wp)
{
	QStandardItem * item = NULL;

	item = new QStandardItem(wp->get_name());
	item->setData(QVariant::fromValue(wp), RoleLayerData);
	item->setToolTip(wp->get_tooltip());
	item->setEditable(false);
	this->items << item;

	item = new QStandardItem(wp->comment);
	item->setEditable(false);
	this->items << item;
}
