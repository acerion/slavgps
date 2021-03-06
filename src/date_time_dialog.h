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

#ifndef _SG_DATE_TIME_DIALOG_H_
#define _SG_DATE_TIME_DIALOG_H_




#include <QDialog>
#include <QVBoxLayout>
#include <QCalendarWidget>
#include <QTimeEdit>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMouseEvent>




#include "coord.h"




namespace SlavGPS {




	bool date_time_dialog(QString const & title, time_t initial_timestamp, time_t & result_timestamp, QWidget * parent = NULL);





	class SGDateTimeDialog : public QDialog {

	friend class SGDateTimeButton;

	public:
		SGDateTimeDialog(QDateTime const & date_time, bool show_clock, QWidget * parent = NULL);
		~SGDateTimeDialog();
		Time get_timestamp(void) const;

		void set_date_time(QDateTime const & date_time);
		QDateTime get_date_time(void) const;

		static QDate date_dialog(QString const & title, const QDate & initial_date, QWidget * parent = NULL);

	private:
		QVBoxLayout * vbox = NULL;
		QCalendarWidget * calendar = NULL;
		QTimeEdit * clock = NULL;
		QDialogButtonBox * button_box = NULL;
	};




	class SGDateTimeButton : public QPushButton {
		Q_OBJECT
	public:
		SGDateTimeButton(QWidget * parent);
		SGDateTimeButton(const Time & date_time, QWidget * parent);
		~SGDateTimeButton();
		Time get_value(void);

		void set_label(const Time & value, const Coord & coord);
		void clear_label(void);

		/* Set coordinate of an object, for which a timestamp is being displayed. */
		void set_coord(const Coord & coord);

		void set_date_time_format(Qt::DateFormat date_time_format);

	protected:
		/* Reimplemented from QAbstractButton */
		virtual void mousePressEvent(QMouseEvent * ev);

	private slots:
		void open_dialog_cb(void);
		void copy_formatted_time_string_cb(void);
		void clear_time_cb(void);

	private:
		SGDateTimeDialog * dialog = NULL;
		Time timestamp;
		Coord coord; /* Coordinates of object, for which a timestamp is being displayed. */

		Qt::DateFormat date_time_format = Qt::ISODate;

	signals:
		void value_is_set(const Time & timestamp);
		void value_is_reset();
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DATE_TIME_DIALOG_H_ */
