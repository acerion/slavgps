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




#include "measurements.h"
#include "widget_lat_lon_entry.h"




using namespace SlavGPS;




#define SG_MODULE "Widget LatLon Entry"




LatLonEntryWidget::LatLonEntryWidget(QWidget * parent)
{
	this->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);

	this->grid = new QGridLayout();

	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->grid);

	int row = 0;

	this->lat_entry = new LatEntryWidget(SGVariant(0.0, SGVariantType::Latitude));
	this->lat_label = new QLabel(QObject::tr("Latitude:")); /* Default label. */
	this->grid->addWidget(this->lat_label, row, 0);
	this->grid->addWidget(this->lat_entry, row, 1);
	row++;

	this->lon_entry = new LonEntryWidget(SGVariant(0.0, SGVariantType::Longitude));
	this->lon_label = new QLabel(QObject::tr("Longitude:")); /* Default label. */
	this->grid->addWidget(this->lon_label, row, 0);
	this->grid->addWidget(this->lon_entry, row, 1);
	row++;

	/* Ensure the first entry field has focus so we can start
	   typing straight away.  User of this widget has to call
	   LatLonEntryWidget::setFocus() after putting the widget in
	   layout. */
	this->setFocusProxy(this->lat_entry);


	QObject::connect(this->lat_entry, SIGNAL (valueChanged(double)), this, SLOT (value_changed_cb()));
	QObject::connect(this->lon_entry, SIGNAL (valueChanged(double)), this, SLOT (value_changed_cb()));
}




void LatLonEntryWidget::set_value(const LatLon & lat_lon)
{
	this->lat_entry->setValue(lat_lon.lat);
	this->lon_entry->setValue(lat_lon.lon);
}




LatLon LatLonEntryWidget::get_value(void) const
{
	LatLon lat_lon;

	lat_lon.lat = this->lat_entry->value();
	lat_lon.lon = this->lon_entry->value();

	return lat_lon;
}




void LatLonEntryWidget::set_text(const QString & latitude_label, const QString & latitude_tooltip, const QString & longitude_label, const QString & longitude_tooltip)
{
	this->lat_entry->setToolTip(latitude_tooltip);
	this->lat_label->setText(latitude_label);

	this->lon_entry->setToolTip(longitude_tooltip);
	this->lon_label->setText(longitude_label);
}




void LatLonEntryWidget::value_changed_cb(void) /* Slot. */
{
	emit this->value_changed();
}




LatEntryWidget::LatEntryWidget(const SGVariant & value, QWidget * parent) : QDoubleSpinBox(parent)
{
	this->setDecimals(SG_LATITUDE_PRECISION);
	this->setMinimum(SG_LATITUDE_MIN);
	this->setMaximum(SG_LATITUDE_MAX);
	this->setSingleStep(0.05);
	this->setValue(value.get_latitude().get_value());
	this->setToolTip(QObject::tr("Coordinate: latitude")); /* Default tooltip. */
}




LonEntryWidget::LonEntryWidget(const SGVariant & value, QWidget * parent) : QDoubleSpinBox(parent)
{
	this->setDecimals(SG_LONGITUDE_PRECISION);
	this->setMinimum(SG_LONGITUDE_MIN);
	this->setMaximum(SG_LONGITUDE_MAX);
	this->setSingleStep(0.05);
	this->setValue(value.get_longitude().get_value());
	this->setToolTip(QObject::tr("Coordinate: longitude")); /* Default tooltip. */
}
