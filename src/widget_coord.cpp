/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2019, Kamil Ignacak <acerion@wp.pl>
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




#include <QLabel>




#include "widget_coord.h"
#include "widget_lat_lon_entry.h"
#include "widget_utm_entry.h"





using namespace SlavGPS;




#define SG_MODULE "Widget Coord"




CoordDisplayWidget::CoordDisplayWidget(__attribute__((unused)) QWidget * parent)
{
	this->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);

	this->m_vbox = new QVBoxLayout();

	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->m_vbox);

	this->m_lat_lon_label = new QLabel(QObject::tr("Latitude/Longitude")); /* Default label. */
	this->m_vbox->addWidget(this->m_lat_lon_label);

	this->m_utm_label = new QLabel(QObject::tr("UTM")); /* Default label. */
	this->m_vbox->addWidget(this->m_utm_label);
}




void CoordDisplayWidget::set_value(const Coord & coord)
{
	this->m_lat_lon_label->setText(coord.get_lat_lon().to_string());
	this->m_utm_label->setText(coord.get_utm().to_string());
}




/**
   @reviewed-on 2019-12-11
*/
CoordEntryWidget::CoordEntryWidget(CoordMode coord_mode, __attribute__((unused)) QWidget * parent)
{
	this->m_vbox = new QVBoxLayout();

	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->m_vbox);

	this->set_coord_mode(coord_mode);

	this->m_vbox->setContentsMargins(0, 0, 0, 0);
}




/**
   @reviewed-on 2019-11-11
*/
sg_ret CoordEntryWidget::set_coord_mode(const CoordMode coord_mode)
{
	if (coord_mode == this->m_coord_mode) {
		return sg_ret::ok;
	}


	if (this->m_lat_lon_entry) {
		this->m_vbox->removeWidget(this->m_lat_lon_entry);
		delete this->m_lat_lon_entry;
		this->m_lat_lon_entry = nullptr;
	} else if (this->m_utm_entry) {
		this->m_vbox->removeWidget(this->m_utm_entry);
		delete this->m_utm_entry;
		this->m_utm_entry = nullptr;
	} else {
		qDebug() << SG_PREFIX_N << "None of coord entries was set";
	}


	switch (coord_mode) {
	case CoordMode::LatLon:
		this->m_lat_lon_entry = new LatLonEntryWidget();
		break;
	case CoordMode::UTM:
		this->m_utm_entry = new UTMEntryWidget();
		break;
	default:
		/* Let's handle this safely by using LatLon as fallback. */
		qDebug() << SG_PREFIX_E << "Unexpected coord mode:" << coord_mode;
		this->m_lat_lon_entry = new LatLonEntryWidget();
		break;
	}


	if (this->m_lat_lon_entry) {
		this->m_vbox->addWidget(this->m_lat_lon_entry);
		connect(this->m_lat_lon_entry, SIGNAL (value_changed(void)), this, SLOT (value_changed_cb(void)));
	} else if (this->m_utm_entry) {
		this->m_vbox->addWidget(this->m_utm_entry);
		connect(this->m_utm_entry, SIGNAL (value_changed(void)), this, SLOT (value_changed_cb(void)));
	} else {
		qDebug() << SG_PREFIX_E << "Both widgets are NULL";
		return sg_ret::err_null_ptr;
	}


	this->m_coord_mode = coord_mode;
	return sg_ret::ok;
}





/**
   @reviewed-on 2019-12-11
*/
sg_ret CoordEntryWidget::set_value(const Coord & coord, bool block_signal)
{
	switch (coord.get_coord_mode()) {
	case CoordMode::LatLon:
		if (this->m_lat_lon_entry) {
			return this->m_lat_lon_entry->set_value(coord.get_lat_lon(), block_signal);
		} else {
			qDebug() << SG_PREFIX_E << "LatLon entry widget is NULL";
			return sg_ret::err;
		}
		break;
	case CoordMode::UTM:
		if (this->m_utm_entry) {
			return this->m_utm_entry->set_value(coord.get_utm(), block_signal);
		} else {
			qDebug() << SG_PREFIX_E << "UTM entry widget is NULL";
			return sg_ret::err;
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Unexpected coord mode:" << coord.get_coord_mode();
		return sg_ret::err;
	}
}




/**
   @reviewed-on 2019-12-11
*/
Coord CoordEntryWidget::get_value(void) const
{
	Coord result;

	if (this->m_lat_lon_entry) {
		result = Coord(this->m_lat_lon_entry->get_value(), CoordMode::LatLon);
		qDebug() << SG_PREFIX_I << "Returning value from LatLon entry:" << result;
	} else if (this->m_utm_entry) {
		result = Coord(this->m_utm_entry->get_value(), CoordMode::UTM);
		qDebug() << SG_PREFIX_I << "Returning value from UTM entry:" << result;
	} else {
		qDebug() << SG_PREFIX_E << "Both widgets are NULL";
	}

	return result;
}




/**
   @reviewed-on 2019-12-11
*/
void CoordEntryWidget::value_changed_cb(void)
{
	qDebug() << SG_PREFIX_SIGNAL << "Will now emit 'value changed' signal after change in LatLon or UTM entry widget";
	emit this->value_changed();
}




/**
   @reviewed-on 2019-12-11
*/
void CoordEntryWidget::clear_widget(void)
{
	if (this->m_lat_lon_entry) {
		this->m_lat_lon_entry->clear_widget();
	} else if (this->m_utm_entry) {
		this->m_utm_entry->clear_widget();
	} else {
		qDebug() << SG_PREFIX_E << "Both widgets are NULL";
	}
}
