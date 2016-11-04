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
 *
 */
#ifndef _SG_DATE_TIME_EDIT_H_
#define _SG_DATE_TIME_EDIT_H_




#include <QDialog>
#include <QVBoxLayout>
#include <QCalendarWidget>
#include <QTimeEdit>
#include <QDateTime>
#include <QDialogButtonBox>




namespace SlavGPS {




	time_t datetime_edit_dialog(QWidget * parent, QString const & title, time_t date_time);




	class SGDateTime : public QDialog {
	public:
		SGDateTime(QWidget * parent, QDateTime const & date_time);
		~SGDateTime();
		time_t get_timestamp();

	private:
		QVBoxLayout * vbox = NULL;
		QCalendarWidget * calendar = NULL;
		QTimeEdit * clock = NULL;
		QDialogButtonBox * button_box = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DATE_TIME_EDIT_H_ */
