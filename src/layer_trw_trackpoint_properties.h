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
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QSignalMapper>
#include <QGridLayout>




#include "widget_timestamp.h"
#include "widget_measurement_entry.h"
#include "layer_trw_track.h"




namespace SlavGPS {




	class Track;
	class Trackpoint;




	/* Dialog response codes. */
	enum {
		SG_TRACK_CLOSE_DIALOG,
		SG_TRACK_INSERT_TP_AFTER,
		SG_TRACK_DELETE_CURRENT_TP,
		SG_TRACK_SPLIT_TRACK_AT_CURRENT_TP,
		SG_TRACK_GO_BACK,
		SG_TRACK_GO_FORWARD,

		SG_TRACK_CHANGED
	};




	class PropertiesDialogTP : public QDialog {
		Q_OBJECT
	public:
		PropertiesDialogTP();
		PropertiesDialogTP(QWidget * parent);
		~PropertiesDialogTP();

		void set_dialog_data(Track * track, const TrackPoints::iterator & current_tp_iter, bool is_route);
		void reset_dialog_data(void);
		void set_dialog_title(const QString & track_name);

		QSignalMapper * signal_mapper = NULL;

	private slots:
		void sync_latlon_entry_to_current_tp_cb(void);
		void sync_altitude_entry_to_current_tp_cb(void);
		void sync_timestamp_entry_to_current_tp_cb(time_t new_timestamp);
		void sync_empty_timestamp_entry_to_current_tp_cb(void);
		bool sync_name_entry_to_current_tp_cb(const QString & new_name);

	signals:
		/* Coordinates of one of track's trackpoints has changed its coordinates. */
		void trackpoint_coordinates_changed(int response);

	private:
		void update_timestamp_widget(Trackpoint * tp);

		bool set_timestamp_of_current_tp(const Time & timestamp);

		Trackpoint * cur_tp = NULL;
		bool sync_to_current_tp_block = false;

		QDialogButtonBox * button_box = NULL;

		QPushButton * button_close_dialog = NULL;
		QPushButton * button_insert_tp_after = NULL;
		QPushButton * button_delete_current_tp = NULL;
		QPushButton * button_split_track = NULL;
		QPushButton * button_go_back = NULL;
		QPushButton * button_go_forward = NULL;

		QGridLayout * grid = NULL;
		QVBoxLayout * vbox = NULL;

		QLineEdit * trkpt_name = NULL;
		QDoubleSpinBox * lat = NULL;
		QDoubleSpinBox * lon = NULL;
		MeasurementEntryWidget * alt = NULL;
		QLabel * course = NULL;

		TimestampWidget * timestamp_widget = NULL;

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
