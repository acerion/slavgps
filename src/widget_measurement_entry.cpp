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




#include "widget_measurement_entry.h"
#include "globals.h"
#include "preferences.h"




using namespace SlavGPS;




#define SG_MODULE "Widget Measurement Entry"




MeasurementEntryWidget::MeasurementEntryWidget(const SGVariant & value_uu, const ParameterScale<double> * scale, QWidget * parent)
{
	this->setFrameStyle(QFrame::NoFrame);

	this->hbox = new QHBoxLayout();;
	this->hbox->setContentsMargins(0, 0, 0, 0);

	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->hbox);


	/* Order of calls is important. Use setDecimals() before using setValue(). */
	this->spin = new QDoubleSpinBox();
	if (scale) {
		this->spin->setMinimum(scale->min);
		this->spin->setMaximum(scale->max);
		this->spin->setSingleStep(scale->step);
		this->spin->setDecimals(scale->n_digits);
	}

	if (value_uu.is_valid()) {
		this->set_value_uu(value_uu);
	} else if (scale) {
		this->set_value_uu(scale->initial); /* FIXME: ::initial is not in user units. */
	} else {
		; /* Pass, don't set initial value. */
	}

	this->hbox->addWidget(this->spin);

	/* Ensure the entry field has focus so we can start typing
	   straight away.  User of this widget has to call
	   ::setFocus() after putting the widget in layout. */
	this->setFocusProxy(this->spin);

	/* Save type of variant and user unit of measurement (even if value of measurement is empty). */
	this->storage_uu = value_uu;
}




void MeasurementEntryWidget::set_value_uu(const SGVariant & value_uu)
{
	switch (value_uu.type_id) {
	case SGVariantType::Altitude:
		{
			const Altitude & altitude_uu = value_uu.get_altitude();
			if (altitude_uu.is_valid()) {
				this->spin->setValue(altitude_uu.get_value());
				this->spin->setSuffix(QString(" %1").arg(Altitude::get_unit_full_string(altitude_uu.get_unit())));
			} else {
				this->spin->clear();
				this->spin->setSuffix("");
			}
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Support for variant type id" << value_uu.type_id << "not implemented yet";
		break;
	}

	this->storage_uu = value_uu;
}




SGVariant MeasurementEntryWidget::get_value_uu(void) const
{
	SGVariant result;

	switch (this->storage_uu.type_id) {
	case SGVariantType::Altitude:
		if (this->storage_uu.get_altitude().is_valid()) {

			/* Since the value in the widget was presented
			   to user, it must have been in user
			   units. Verify this. */
			if (this->storage_uu.get_altitude().get_unit() != Preferences::get_unit_height()) {
				qDebug() << SG_PREFIX_E << "Unit mismatch:" << (int) this->storage_uu.get_altitude().get_unit() << "!=" << (int) Preferences::get_unit_height();
			}

			const Altitude altitude_uu(this->spin->value(), this->storage_uu.get_altitude().get_unit());
			result = SGVariant(altitude_uu);
		} else {
			/* This may happen e.g. when user
			   manually adds waypoints to TRW
			   layer. "waypoints properties"
			   dialog will initially display empty
			   altitude, and only after user
			   enters a value we can save it with
			   some unit. */
			const Altitude altitude_uu(this->spin->value(), Preferences::get_unit_height());
			result = SGVariant(altitude_uu);
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Support for variant type id" << this->storage_uu.type_id << "not implemented yet";
		break;
	}

	return result;
}




void MeasurementEntryWidget::set_tooltip(const QString & tooltip)
{
	this->spin->setToolTip(tooltip);
}
