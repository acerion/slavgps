/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2006, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2012-2015, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_WIDGET_SLIDER_H_
#define _SG_WIDGET_SLIDER_H_




#include <QObject>
#include <QSlider>
#include <QLabel>




#include "ui_builder.h"




namespace SlavGPS {




	class SliderWidget : public QWidget {
		Q_OBJECT
	public:
		SliderWidget(const ParameterScale<int> & scale, Qt::Orientation orientation, QWidget * parent = NULL);
		SliderWidget(const ParameterScale<double> & scale, Qt::Orientation orientation, QWidget * parent = NULL);
		~SliderWidget();

		void set_value(int val);
		int get_value(void);

	private:
		//int orientation;
		//QHBoxLayout * hbox = NULL;
		//QVBoxLayout * vbox = NULL;

		QLabel label;
		QSlider slider;

	private slots:
		void value_changed_cb(int val);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WIDGET_SLIDER_H_ */
