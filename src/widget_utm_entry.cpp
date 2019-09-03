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




#include <cassert>




#include <QLabel>
#include <QDebug>




#include "widget_utm_entry.h"
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "Widget UTM Entry"




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

	this->band_letter_combo = new QComboBox();
	this->band_letter_combo->addItems(UTM::get_band_symbols());
	this->band_letter_combo->setCurrentText("N");
	this->grid->addWidget(new QLabel(QObject::tr("Band Letter:")), row, 0);
	this->grid->addWidget(this->band_letter_combo, row, 1);
	row++;

	/* Ensure the first entry field has focus so we can start
	   typing straight away.  User of this widget has to call
	   SGUtmEntry::setFocus() after putting the widget in
	   layout. */
	this->setFocusProxy(this->easting_spin);
}




sg_ret UTMEntryWidget::set_value(const UTM & utm, bool block_signal)
{
	assert (UTM::is_band_letter(utm.get_band_letter()));

	if (block_signal) {
		this->easting_spin->blockSignals(true);
		this->northing_spin->blockSignals(true);
		this->zone_spin->blockSignals(true);
		this->band_letter_combo->blockSignals(true);
	}

	this->easting_spin->setValue(utm.easting);
	this->northing_spin->setValue(utm.northing);
	this->zone_spin->setValue(utm.get_zone());
	this->band_letter_combo->setCurrentText(QString(utm.get_band_as_letter()));

	if (block_signal) {
		this->easting_spin->blockSignals(false);
		this->northing_spin->blockSignals(false);
		this->zone_spin->blockSignals(false);
		this->band_letter_combo->blockSignals(false);
	}

	return sg_ret::ok;
}




UTM UTMEntryWidget::get_value(void) const
{
	UTM utm;

	utm.set_easting(this->easting_spin->value());
	utm.set_northing(this->northing_spin->value());
	utm.set_zone(this->zone_spin->value());

	const QString text = this->band_letter_combo->currentText();
	if (1 != text.size()) {
		qDebug() << SG_PREFIX_E << "Unexpectedly long text in combo:" << text;
	} else {
		utm.set_band_letter(text.at(0).toUpper().toLatin1());
		qDebug() << SG_PREFIX_I << "UTM band letter conversion" << text << "->" << utm.get_band_as_letter();
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
