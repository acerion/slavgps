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

#ifndef _SG_WIDGET_COORD_H_
#define _SG_WIDGET_COORD_H_




#include <QObject>
#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>




#include "coord.h"
#include "widget_lat_lon_entry.h"
#include "widget_utm_entry.h"




namespace SlavGPS {




	class Coord;




	class CoordDisplayWidget : public QFrame {
		Q_OBJECT
	public:
		CoordDisplayWidget(QWidget * parent = NULL);
		void set_value(const Coord & coord);

	private:
		QVBoxLayout * vbox = NULL;
		QLabel * lat_lon_label = NULL;
		QLabel * utm_label = NULL;
	};





	/* This widget is not based on QFrame, because the entry
	   widgets that are members of this class already provide
	   frame UI. */
	class CoordEntryWidget : public QWidget {
		Q_OBJECT
	public:
		CoordEntryWidget(CoordMode coord_mode, QWidget * parent = NULL);

		/**
		   @param block_signal: in normal conditions the
		   underlying widget will emit signal when a value is
		   set. This parameter set to true may block emitting
		   the signal when value is set. E.g. when widget is
		   shown first time and its initial value is set, we
		   don't want to emit signal.
		*/
		sg_ret set_value(const Coord & coord, bool block_signal = false);
		Coord get_value(void) const;

		/* Erase all contents from widget, as if nothing was
		   presented by the widget. */
		void clear_widget(void);

	signals:
		void value_changed(void);

	private slots:
		void value_changed_cb(void);

	private:
		LatLonEntryWidget * lat_lon_entry = NULL;
		UTMEntryWidget * utm_entry = NULL;
		QVBoxLayout * vbox = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WIDGET_COORD_H_ */
