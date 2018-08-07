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




#include "widget_utm_entry.h"




using namespace SlavGPS;




#define PREFIX " Widget UTM Entry:" << __FUNCTION__ << __LINE__ << ">"




UTMEntryWidget::UTMEntryWidget(QWidget * parent)
{
	this->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);

	this->grid = new QGridLayout();

	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->grid);

	int row = 0;

	this->easting_spin = new QDoubleSpinBox();
	this->easting_spin->setMinimum(0.0);
	this->easting_spin->setMaximum(1500000.0);
	this->easting_spin->setSingleStep(1);
	this->easting_spin->setValue(0);
	this->easting_label = new QLabel(QObject::tr("Easting:"));
	this->grid->addWidget(this->easting_label, row, 0);
	this->grid->addWidget(this->easting_spin, row, 1);
	row++;

	this->northing_spin = new QDoubleSpinBox();
	this->northing_spin->setMinimum(0.0);
	this->northing_spin->setMaximum(9000000.0);
	this->northing_spin->setSingleStep(1);
	this->northing_spin->setValue(0);
	this->northing_label = new QLabel(QObject::tr("Northing:"));
	this->grid->addWidget(this->northing_label, row, 0);
	this->grid->addWidget(this->northing_spin, row, 1);
	row++;


	this->zone_spin = new QSpinBox();
	this->zone_spin->setMinimum(1);
	this->zone_spin->setMaximum(60);
	this->zone_spin->setSingleStep(1);
	this->zone_spin->setValue(1);
	this->grid->addWidget(new QLabel(QObject::tr("Zone:")), row, 0);
	this->grid->addWidget(this->zone_spin, row, 1);
	row++;

	this->band_letter_entry = new QLineEdit();
	this->band_letter_entry->setMaxLength(1);
	this->band_letter_entry->setText("N");
	this->grid->addWidget(new QLabel(QObject::tr("Band Letter:")), row, 0);
	this->grid->addWidget(this->band_letter_entry, row, 1);
	row++;

	/* Ensure the first entry field has focus so we can start
	   typing straight away.  User of this widget has to call
	   SGUtmEntry::setFocus() after putting the widget in
	   layout. */
	this->setFocusProxy(this->easting_spin);
}




void UTMEntryWidget::set_value(const UTM & utm)
{
	this->easting_spin->setValue(utm.easting);
	this->northing_spin->setValue(utm.northing);
	this->zone_spin->setValue(utm.zone);
	this->band_letter_entry->setText(QString(utm.band_letter));
}




UTM UTMEntryWidget::get_value(void) const
{
	UTM utm;

	utm.easting = this->easting_spin->value();
	utm.northing = this->northing_spin->value();
	utm.zone = this->zone_spin->value();

	const QString text = this->band_letter_entry->text();
	if (1 == text.size()) {
		utm.band_letter = text.at(0).toUpper().toLatin1();
		qDebug() << "II:" PREFIX << "UTM band letter conversion" << text << "->" << utm.band_letter;
	}

	return utm;
}




void UTMEntryWidget::set_text(const QString & east_label, const QString & east_tooltip, const QString & north_label, const QString & north_tooltip)
{
	this->easting_spin->setToolTip(east_tooltip);
	this->easting_label->setText(east_label);

	this->northing_spin->setToolTip(north_tooltip);
	this->northing_label->setText(north_label);
}
