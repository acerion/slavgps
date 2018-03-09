/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
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
#include <QDebug>




#include "measurements.h"
#include "widget_lat_lon_entry.h"




using namespace SlavGPS;




#define PREFIX " Widget LatLon Entry:" << __FUNCTION__ << __LINE__ << ">"




SGLatLonEntry::SGLatLonEntry(QWidget * parent)
{
	this->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);

	this->grid = new QGridLayout();

	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->grid);

	int row = 0;

	this->lat_spin = new QDoubleSpinBox();
	this->lat_spin->setDecimals(SG_PRECISION_LATITUDE);
	this->lat_spin->setMinimum(-90.0);
	this->lat_spin->setMaximum(90.0);
	this->lat_spin->setSingleStep(0.05);
	this->lat_spin->setToolTip(QObject::tr("Coordinate: latitude")); /* Default tooltip. */
	this->lat_label = new QLabel(QObject::tr("Latitude:")); /* Default label. */
	this->grid->addWidget(this->lat_label, row, 0);
	this->grid->addWidget(this->lat_spin, row, 1);
	row++;

	this->lon_spin = new QDoubleSpinBox();
	this->lon_spin->setDecimals(SG_PRECISION_LONGITUDE);
	this->lon_spin->setMinimum(-180.0);
	this->lon_spin->setMaximum(180.0);
	this->lon_spin->setSingleStep(0.05);
	this->lon_spin->setValue(0.0);
	this->lon_spin->setToolTip(QObject::tr("Coordinate: longitude")); /* Default tooltip. */
	this->lon_label = new QLabel(QObject::tr("Longitude:")); /* Default label. */
	this->grid->addWidget(this->lon_label, row, 0);
	this->grid->addWidget(this->lon_spin, row, 1);
	row++;

	/* Ensure the first entry field has focus so we can start
	   typing straight away.  User of this widget has to call
	   SGLatLonEntry::setFocus() after putting the widget in
	   layout. */
	this->setFocusProxy(this->lat_spin);
}




void SGLatLonEntry::set_value(const LatLon & lat_lon)
{
	this->lat_spin->setValue(lat_lon.lat);
	this->lon_spin->setValue(lat_lon.lon);
}




LatLon SGLatLonEntry::get_value(void) const
{
	LatLon lat_lon;

	lat_lon.lat = this->lat_spin->value();
	lat_lon.lon = this->lon_spin->value();

	return lat_lon;
}




void SGLatLonEntry::set_text(const QString & latitude_label, const QString & latitude_tooltip, const QString & longitude_label, const QString & longitude_tooltip)
{
	this->lat_spin->setToolTip(latitude_tooltip);
	this->lat_label->setText(latitude_label);

	this->lon_spin->setToolTip(longitude_tooltip);
	this->lon_label->setText(longitude_label);
}
