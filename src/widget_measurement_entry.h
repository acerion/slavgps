/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_WIDGET_MEASUREMENT_ENTRY_H_
#define _SG_WIDGET_MEASUREMENT_ENTRY_H_




#include <QObject>
#include <QFrame>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QHBoxLayout>




#include "coords.h"
#include "variant.h"
#include "ui_builder.h"




namespace SlavGPS {




	//typedef class Measurement<HeightUnit, Altitude_ll> Altitude;




	class MeasurementEntryWidget : public QFrame {
		Q_OBJECT
	public:
		MeasurementEntryWidget(const SGVariant & value_iu, const ParameterScale<double> * scale, QWidget * parent = NULL);

		/* Set and get value in user units (i.e. units set in
		   program's preferences).. */
		void set_value_iu(const SGVariant & measurement_iu);
		SGVariant get_value_iu(void) const;

		void set_tooltip(const QString & tooltip);

	signals:

	private slots:

	private:
		QHBoxLayout * hbox = NULL;
		QDoubleSpinBox * spin = NULL;

		/* For storing type of value presented in widget. */
		SGVariantType type_id;
	};




	class MeasurementDisplayWidget : public QFrame {
		Q_OBJECT
	public:
		MeasurementDisplayWidget(QWidget * parent = NULL);

		/* @altitude must be in User Units (hence _uu postfix). */
		void set_value_uu(const Altitude & altitude);

		void set_tooltip(const QString & tooltip);

	private:
		QVBoxLayout * vbox = NULL;
		QLabel * label = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WIDGET_MEASUREMENT_ENTRY_H_ */
