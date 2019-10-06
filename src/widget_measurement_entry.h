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








	class MeasurementEntryWidget_2 : public QFrame {
		Q_OBJECT
	public:
		MeasurementEntryWidget_2(QWidget * parent = NULL);

		QHBoxLayout * hbox = NULL;
		QLabel * label = NULL;
		QDoubleSpinBox * spin = NULL;

	public slots:
		void value_changed_cb(double) { emit this->value_changed(); }

	signals:
		void value_changed(void);
	};




	/*
	  Tm: type of measurement (e.g. Altitude)
	  Tu: type of unit of measurements (e.g. HeightUnit)
	*/
	template <class Tm, class Tu>
	class MeasurementEntry_2 {
	public:
		MeasurementEntry_2(const Tm & value_iu, const MeasurementScale<Tm> * scale, QWidget * parent = NULL)
		{
			this->meas_widget = new MeasurementEntryWidget_2(parent);

			QObject::connect(this->meas_widget->spin, SIGNAL (valueChanged(double)), this->meas_widget, SLOT (value_changed_cb(double)));

			if (value_iu.is_valid()) {
				qDebug() << "II    Measurement Entry 2: Using initial value from function argument";
				this->set_value_iu(value_iu);
			} else if (scale) {
				qDebug() << "II    Measurement Entry 2: Using initial value from scale";
				this->set_value_iu(scale->m_initial);
			} else {
				qDebug() << "NN    Measurement Entry 2: Not using any initial value";
				; /* Pass, don't set initial value. */
			}

			if (scale) {
				/* Order of calls is important. Use setDecimals() before using setValue(). */
				qDebug() << "II    Measurement Entry 2: Setting scale: min =" << scale->m_min << "max =" << scale->m_max << "step =" << scale->m_step << "n_digits =" << scale->m_n_digits;
				this->meas_widget->spin->setDecimals(scale->m_n_digits);
				this->meas_widget->spin->setMinimum(scale->m_min.get_ll_value());
				this->meas_widget->spin->setMaximum(scale->m_max.get_ll_value());
				this->meas_widget->spin->setSingleStep(scale->m_step.get_ll_value());
			} else {
				qDebug() << "II    Measurement Entry 2: Not setting scale";
			}
		}




		/* Set value in user units (i.e. units set in
		   program's preferences). */
		void set_value_iu(const Tm & value_iu)
		{
			if (value_iu.is_valid()) {
				const Tu user_unit = Tm::get_user_unit();
				const Tm value_uu = value_iu.convert_to_unit(user_unit);

				qDebug() << "II    Measurement Entry 2: Setting value of measurement iu" << value_iu << ", in user units:" << value_uu;

				this->meas_widget->spin->setValue(value_uu.get_ll_value());
				this->meas_widget->spin->setSuffix(QString(" %1").arg(Tm::get_unit_full_string(user_unit)));
			} else {
				qDebug() << "NN    Measurement Entry 2: Value passed as argument is invalid, clearing value of measurement";
				this->meas_widget->spin->clear();
				this->meas_widget->spin->setSuffix("");
			}
		}




		/* Get value in user units (i.e. units set in
		   program's preferences). */
		Tm get_value_iu(void) const
		{
			/* Since the value in the widget was presented
			   to user, it must have been in user
			   units. Now convert to internal unit. */
			const Tm value_uu(this->meas_widget->spin->value(), Tm::get_user_unit());
			Tm result_iu = value_uu.convert_to_unit(Tm::get_internal_unit());

			return result_iu;
		}




		void set_tooltip(const QString & tooltip)
		{
			this->meas_widget->spin->setToolTip(tooltip);
		}



		/* Erase all contents from widget, as if nothing was
		   presented by the widget. */
		void clear_widget(void)
		{
			this->meas_widget->spin->clear();
			this->meas_widget->spin->setSuffix("");
		}


		MeasurementEntryWidget_2 * meas_widget = NULL;
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
