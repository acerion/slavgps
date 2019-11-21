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

#ifndef _SG_WIDGET_DURATION_H_
#define _SG_WIDGET_DURATION_H_




#include <QObject>
#include <QWidget>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QFrame>




#include "ui_builder.h"
#include "measurements.h"
#include "widget_measurement_entry.h"




namespace SlavGPS {




	class DurationWidget : public QFrame {
		Q_OBJECT
	public:

		DurationWidget(const MeasurementScale<Duration, Time_ll, TimeUnit> & scale, QWidget * parent = NULL);
		~DurationWidget() {};

		sg_ret set_value(const Duration & duration);

		Duration get_value(void) const;

		/* Erase all contents from widget, as if nothing was
		   presented by the widget. */
		void clear_widget(void);

	private:
		QHBoxLayout * m_hbox = nullptr;
		QSpinBox * m_widget = nullptr;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WIDGET_DURATION_H_ */
