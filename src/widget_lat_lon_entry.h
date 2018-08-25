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

#ifndef _SG_WIDGET_LAT_LON_ENTRY_H_
#define _SG_WIDGET_LAT_LON_ENTRY_H_




#include <QObject>
#include <QFrame>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QGridLayout>
#include <QLabel>




#include "coords.h"




namespace SlavGPS {




	class LatLonEntryWidget : public QFrame {
		Q_OBJECT
	public:
		LatLonEntryWidget(QWidget * parent = NULL);

		void set_value(const LatLon & lat_lon);
		LatLon get_value(void) const;

		void set_text(const QString & latitude_label, const QString & latitude_tooltip, const QString & longitude_label, const QString & longitude_tooltip);

	signals:
		void value_changed(void);

	private slots:
		void value_changed_cb(void);

	public:
		QGridLayout * grid = NULL;

		QDoubleSpinBox * lat_spin = NULL;
		QDoubleSpinBox * lon_spin = NULL;

		QLabel * lat_label = NULL;
		QLabel * lon_label = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WIDGET_LAT_LON_ENTRY_H_ */
