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




#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "widget_slider.h"




using namespace SlavGPS;




SGSlider::SGSlider(const ParameterScale & scale, Qt::Orientation orientation, QWidget * parent) : QWidget(parent)
{
	this->slider.setRange(scale.min, scale.max);
	this->slider.setSingleStep(scale.step);
	// gtk_scale_set_digits(GTK_SCALE(rv), scale->digits);
	this->slider.setValue(scale.min);
	this->slider.setOrientation(orientation);

	QBoxLayout * layout = NULL;
	if (orientation == Qt::Horizontal) {
		layout = new QHBoxLayout;
	} else {
		layout = new QVBoxLayout;
	}

	layout->addWidget(&this->slider);
	layout->addWidget(&this->label);

	this->setLayout(layout);

	connect(&this->slider, SIGNAL(valueChanged(int)), this, SLOT(value_changed_cb(int)));
}




SGSlider::~SGSlider()
{
}




void SGSlider::set_value(int val)
{
	this->slider.setValue(val);
	this->value_changed_cb(val);
}




int SGSlider::get_value(void)
{
	return this->slider.value();
}



void SGSlider::value_changed_cb(int val)
{
	this->label.setText(QString("%1").arg(val));
}