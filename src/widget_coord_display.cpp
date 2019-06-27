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




#include "coord.h"
#include "widget_coord_display.h"




using namespace SlavGPS;




#define SG_MODULE "Widget Coord Display"




CoordDisplayWidget::CoordDisplayWidget(QWidget * parent)
{
	this->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);

	this->vbox = new QVBoxLayout();

	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);

	this->lat_lon_label = new QLabel(QObject::tr("Latitude/Longitude")); /* Default label. */
	this->vbox->addWidget(this->lat_lon_label);

	this->utm_label = new QLabel(QObject::tr("UTM")); /* Default label. */
	this->vbox->addWidget(this->utm_label);
}




void CoordDisplayWidget::set_value(const Coord & coord)
{
	this->lat_lon_label->setText(coord.get_lat_lon().to_string());
	this->utm_label->setText(coord.get_utm().to_string());
}
