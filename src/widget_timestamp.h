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

#ifndef _SG_WIDGET_TIMESTAMP_H_
#define _SG_WIDGET_TIMESTAMP_H_




#include <QObject>
#include <QFrame>
#include <QSpinBox>
#include <QGridLayout>




#include "date_time_dialog.h"




namespace SlavGPS {




	class TimestampWidget : public QFrame {
		Q_OBJECT
	public:
		TimestampWidget(QWidget * parent = NULL);
		void set_timestamp(const Time & timestamp, const Coord & coord);
		void clear_widget(void);
		Time get_timestamp(void) const;

		/* Set coordinate of an object, for which a timestamp is being displayed. */
		void set_coord(const Coord & coord);

		void clear(void);

	signals:
		void value_is_set(const Time & timestamp);
		void value_is_reset(void);

	private slots:
		/* Right now there is no "reset value" control for
		   timestamp entry field.  We can only change value of
		   the field, perhaps to zero, but zero may be a valid
		   time stamp. */
		void on_timestamp_entry_value_set_cb(void);

		/* On timestamp button we can perform set and reset of
		   timestamp. */
		void on_timestamp_button_value_set_cb(void);
		void on_timestamp_button_value_reset_cb(void);

	private:
		QGridLayout * grid = NULL;
		QSpinBox * timestamp_entry = NULL;
		SGDateTimeButton * timestamp_button = NULL;

		Coord coord; /* Coordinates of object, for which a timestamp is being displayed. */
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WIDGET_TIMESTAMP_H_ */
