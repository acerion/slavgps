/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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




#include <QDebug>




#include "layer_trw_point_properties.h"
#include "window.h"




using namespace SlavGPS;




#define SG_MODULE "Point Properties"




PointPropertiesWidget::PointPropertiesWidget(QWidget * parent) : QWidget(parent)
{
	this->button_box_upper = new QDialogButtonBox();
	this->button_box_lower = new QDialogButtonBox();

	this->vbox = new QVBoxLayout;
	this->grid = new QGridLayout();
	this->vbox->addLayout(this->grid);
	this->vbox->addWidget(this->button_box_upper);
	this->vbox->addWidget(this->button_box_lower);

	/* -1: insert at the end; +1: give more "priority" to the
            stretch than to other widgets in vbox. */
	this->vbox->insertStretch(-1, +1);

	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);
}




sg_ret PointPropertiesWidget::build_widgets(CoordMode coord_mode, __attribute__((unused)) QWidget * parent_widget)
{
	const int left_col = 0;
	const int right_col = 1;


	this->name_entry = new QLineEdit("");
	this->grid->addWidget(new QLabel(tr("Name:")), this->widgets_row, left_col);
	this->grid->addWidget(this->name_entry, this->widgets_row, right_col);

	this->widgets_row++;

	this->coord_widget = new CoordEntryWidget(coord_mode);
	this->grid->addWidget(this->coord_widget, this->widgets_row, left_col, 1, 2);
	connect(ThisApp::main_window(), SIGNAL(coord_mode_changed(CoordMode)), this, SLOT(set_coord_mode(CoordMode)));

	this->widgets_row++;

	const AltitudeType::Unit height_unit = AltitudeType::Unit::internal_unit();
	MeasurementScale<Altitude> scale_alti(SG_ALTITUDE_RANGE_MIN,
						  SG_ALTITUDE_RANGE_MAX,
						  0.0,
						  1,
						  height_unit,
						  SG_ALTITUDE_PRECISION);
	this->altitude_widget = new MeasurementEntry_2<Altitude>(Altitude(0, height_unit), &scale_alti, this);
	this->altitude_widget->meas_widget->label->setText(tr("Altitude:"));
	this->grid->addWidget(this->altitude_widget->meas_widget, this->widgets_row, left_col, 1, 2);

	this->widgets_row++;

	this->timestamp_widget = new TimestampWidget();
	this->grid->addWidget(this->timestamp_widget, this->widgets_row, left_col, 1, 2);

	this->widgets_row++;

	return sg_ret::ok;
}




void PointPropertiesWidget::clear_widgets(void)
{
	this->name_entry->setText("");
	this->coord_widget->clear_widget();
	this->altitude_widget->clear_widget();
	this->timestamp_widget->clear_widget();
}




/**
   @brief Change coordinate mode of "coordinates" widget used by this
   class

   Rebuild the "coordinates" widget so that it can be used to display
   either Lat/Lon or UTM coordinates.

   @reviewed-on 2019-12-11
*/
sg_ret PointPropertiesWidget::set_coord_mode(CoordMode coord_mode)
{
	qDebug() << SG_PREFIX_SLOT << "Received change of coord mode to" << coord_mode << "in" << this->debug_id;
	return this->coord_widget->set_coord_mode(coord_mode);
}
