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




MeasurementEntryWidget::MeasurementEntryWidget(const SGVariant & value_iu, const ParameterScale<double> * scale, QWidget * parent)
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
		qDebug() << SG_PREFIX_I << "Setting scale: min =" << scale->min << "max =" << scale->max << "step =" << scale->step << "n_digits =" << scale->n_digits;
		this->spin->setDecimals(scale->n_digits);
		this->spin->setMinimum(scale->min);
		this->spin->setMaximum(scale->max);
		this->spin->setSingleStep(scale->step);
	} else {
		qDebug() << SG_PREFIX_I << "Not setting scale";
	}

	if (value_iu.is_valid()) {
		qDebug() << SG_PREFIX_I << "Using initial value from function argument";
		this->set_value_iu(value_iu);
	} else if (scale) {
		qDebug() << SG_PREFIX_I << "Using initial value from scale";
		this->set_value_iu(scale->initial);
	} else {
		qDebug() << SG_PREFIX_N << "Not using any initial value";
		; /* Pass, don't set initial value. */
	}

	this->hbox->addWidget(this->spin);

	/* Ensure the entry field has focus so we can start typing
	   straight away.  User of this widget has to call
	   ::setFocus() after putting the widget in layout. */
	this->setFocusProxy(this->spin);
}




void MeasurementEntryWidget::set_value_iu(const SGVariant & value_iu)
{
	switch (value_iu.type_id) {
	case SGVariantType::AltitudeType:
		{
			const Altitude altitude_iu = value_iu.get_altitude();
			if (altitude_iu.is_valid()) {
				const HeightUnit height_unit = Preferences::get_unit_height();
				const Altitude altitude_uu = altitude_iu.convert_to_unit(height_unit);

				qDebug() << SG_PREFIX_I << "Setting value of altitude iu" << altitude_iu << ", in user units:" << altitude_uu;

				this->spin->setValue(altitude_uu.get_ll_value());
				this->spin->setSuffix(QString(" %1").arg(Altitude::get_unit_full_string(height_unit)));
			} else {
				qDebug() << SG_PREFIX_I << "Clearing value of altitude";
				this->spin->clear();
				this->spin->setSuffix("");
			}
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Support for variant type id" << value_iu.type_id << "not implemented yet";
		break;
	}

	/* Save type of variant and user unit of measurement (even if value of measurement is empty). */
	this->type_id = value_iu.type_id;
}




SGVariant MeasurementEntryWidget::get_value_iu(void) const
{
	SGVariant result_iu;

	switch (this->type_id) {
	case SGVariantType::AltitudeType:
		{
			/* Since the value in the widget was presented
			   to user, it must have been in user
			   units. Now convert to internal unit. */
			const Altitude altitude_uu(this->spin->value(), Preferences::get_unit_height());
			result_iu = SGVariant(altitude_uu.convert_to_unit(HeightUnit::Metres));
		}
		break;
	default:
		qDebug() << SG_PREFIX_E << "Support for variant type id" << this->type_id << "not implemented yet";
		break;
	}

	return result_iu;
}




void MeasurementEntryWidget::set_tooltip(const QString & tooltip)
{
	this->spin->setToolTip(tooltip);
}




MeasurementDisplayWidget::MeasurementDisplayWidget(QWidget * parent)
{
	this->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);

	this->vbox = new QVBoxLayout();

	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);

	this->label = new QLabel(SG_MEASUREMENT_INVALID_VALUE_STRING); /* Default label. */
	this->vbox->addWidget(this->label);
}




void MeasurementDisplayWidget::set_value_uu(const Altitude & altitude)
{
	this->label->setText(altitude.to_string());
}




void MeasurementDisplayWidget::set_tooltip(const QString & tooltip)
{
	this->label->setToolTip(tooltip);
}
