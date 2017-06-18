/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_LAYER_TRW_TPWIN_H_
#define _SG_LAYER_TRW_TPWIN_H_




#include <list>
#include <cstdint>

#include <QWidget>
#include <QDialog>
#include <QFormLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QSignalMapper>
#include <QDateTimeEdit>

#include "track.h"




namespace SlavGPS {



	/* Dialog response codes. */
	enum {
		SG_TRACK_CLOSE,
		SG_TRACK_INSERT,
		SG_TRACK_DELETE,
		SG_TRACK_SPLIT,
		SG_TRACK_BACK,
		SG_TRACK_FORWARD,

		SG_TRACK_CHANGED
	};




	class PropertiesDialogTP : public QDialog {
		Q_OBJECT
	public:
		PropertiesDialogTP();
		PropertiesDialogTP(QWidget * parent);
		~PropertiesDialogTP();

		void set_tp(Track * list, std::list<Trackpoint *>::iterator * iter, char const * track_name, bool is_route);
		void set_track_name(char const * track_name);
		void set_empty();

		QSignalMapper * signalMapper = NULL;

	private slots:
		void sync_ll_to_tp_cb(void);
		void sync_alt_to_tp_cb(void);
		void sync_timestamp_to_tp_cb(void);
		bool set_name_cb(void);
		void datetime_clicked_cb(void);

	private:

		void update_times(Trackpoint * tp);

		Trackpoint * cur_tp = NULL;
		bool sync_to_tp_block = false;
		QWidget * parent = NULL;

		QDialogButtonBox * button_box = NULL;

		QPushButton * button_close = NULL;
		QPushButton * button_insert_after = NULL;
		QPushButton * button_delete = NULL;
		QPushButton * button_split_here = NULL;
		QPushButton * button_back = NULL;
		QPushButton * button_forward = NULL;

		QVBoxLayout * vbox = NULL;
		QHBoxLayout * hbox = NULL;

		QWidget * left_area = NULL;
		QWidget * right_area = NULL;

		QLineEdit * trkpt_name = NULL;
		QDoubleSpinBox * lat = NULL;
		QDoubleSpinBox * lon = NULL;
		QDoubleSpinBox * alt = NULL;
		QLabel * course = NULL;
		QSpinBox * timestamp = NULL;
		QPushButton * datetime = NULL;

		QLabel * diff_dist = NULL;
		QLabel * diff_time = NULL;
		QLabel * diff_speed = NULL;
		QLabel * speed = NULL;
		QLabel * vdop = NULL;
		QLabel * hdop = NULL;
		QLabel * pdop = NULL;
		QLabel * sat = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TRW_TPWIN_H_ */
