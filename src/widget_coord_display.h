/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_WIDGET_COORD_H_
#define _SG_WIDGET_COORD_H_




#include <QObject>
#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>




#include "coord.h"




namespace SlavGPS {




	class LatLonEntryWidget;
	class UTMEntryWidget;




	class CoordDisplayWidget : public QFrame {
		Q_OBJECT
	public:
		CoordDisplayWidget(QWidget * parent = nullptr);
		void set_value(const Coord & coord);

	private:
		QVBoxLayout * m_vbox = nullptr;
		QLabel * m_lat_lon_label = nullptr;
		QLabel * m_utm_label = nullptr;
	};




	class CoordEntryWidget : public QWidget {
		Q_OBJECT
	public:
		CoordEntryWidget(CoordMode coord_mode, QWidget * parent = nullptr);

		/**
		   This widget is (or at least should be) aware of
		   program-wide coordinate mode selection made by user in UI.

		   If the widget is configured to use UTM mode, but
		   coordinate passed to ::set_value() method is in in
		   LatLon mode, the method will return error. The
		   error is also returned for reverse situation
		   (LatLon mode of widget, UTM mode of method's
		   argument.

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

		/**
		   @brief (Re)build the widget so that its sub-widgets
		   can be used to display coordinates in given @param
		   coord_mode

		   If the widget already is in specified @param
		   coord_mode, nothing happens.

		   Use this method to change coord mode of widget when
		   user changes coord mode in main window.
		*/
		sg_ret set_coord_mode(const CoordMode coord_mode);

		CoordMode get_coord_mode(void) const { return this->m_coord_mode; }

	signals:
		void value_changed(void);

	private slots:
		void value_changed_cb(void);

	private:
		LatLonEntryWidget * m_lat_lon_entry = nullptr;
		UTMEntryWidget * m_utm_entry = nullptr;
		QVBoxLayout * m_vbox = nullptr;

		CoordMode m_coord_mode = CoordMode::Invalid; /* Initial value is invalid - widgets aren't constructed yet. */
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WIDGET_COORD_H_ */
