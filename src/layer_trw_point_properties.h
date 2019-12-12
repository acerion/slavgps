/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_LAYER_TRW_POINT_PROPERTIES_H_
#define _SG_LAYER_TRW_POINT_PROPERTIES_H_




#include <QWidget>
#include <QDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QGridLayout>




#include "widget_timestamp.h"
#include "widget_measurement_entry.h"
#include "widget_coord.h"




namespace SlavGPS {




	class PointPropertiesWidget : public QWidget {
		Q_OBJECT
	public:
		PointPropertiesWidget(QWidget * parent = nullptr);

		sg_ret build_widgets(CoordMode coord_mode, QWidget * parent_widget);

		/* Erase all contents from widgets, as if nothing was
		   presented by the widgets. */
		void clear_widgets(void);

	public slots:
		sg_ret set_coord_mode(CoordMode coord_mode);

	protected:
		int widgets_row = 0;

		QLineEdit * name_entry = nullptr;
		CoordEntryWidget * coord_widget = nullptr;
		MeasurementEntry_2<Altitude, Altitude_ll, HeightUnit> * altitude_widget = nullptr;
		TimestampWidget * timestamp_widget = nullptr;

		/* Buttons will be in two rows. */
		QDialogButtonBox * button_box_upper = nullptr;
		QDialogButtonBox * button_box_lower = nullptr;

		QGridLayout * grid = nullptr;
		QVBoxLayout * vbox = nullptr;

		QString debug_id;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TRW_POINT_PROPERTIES_H_ */
