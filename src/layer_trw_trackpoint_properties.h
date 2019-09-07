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
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QSignalMapper>
#include <QGridLayout>




#include "widget_timestamp.h"
#include "widget_measurement_entry.h"
#include "widget_coord_display.h"
#include "layer_trw_track.h"
#include "layer_trw_point_properties.h"




namespace SlavGPS {




	class Track;
	class Trackpoint;




	class TpPropertiesWidget : public PointPropertiesWidget {
		Q_OBJECT
	public:
		TpPropertiesWidget(QWidget * parent = NULL);

		sg_ret build_widgets(QWidget * parent_widget);
		void reset_widgets(void);
		void build_buttons(QWidget * parent_widget);


		QLabel * course = NULL;
		QLabel * diff_dist = NULL;
		QLabel * diff_time = NULL;
		QLabel * diff_speed = NULL;
		QLabel * speed = NULL;
		QLabel * vdop = NULL;
		QLabel * hdop = NULL;
		QLabel * pdop = NULL;
		QLabel * sat = NULL;


		QSignalMapper * signal_mapper = NULL;


		QPushButton * button_insert_tp_after = NULL;
		QPushButton * button_split_track = NULL;
		QPushButton * button_delete_current_point = NULL;

		QPushButton * button_previous_point = NULL;
		QPushButton * button_next_point = NULL;
		QPushButton * button_close_dialog = NULL;
	};




	class TpPropertiesDialog : public TpPropertiesWidget {
		Q_OBJECT
	public:
		TpPropertiesDialog(CoordMode coord_mode, QWidget * parent = NULL);
		~TpPropertiesDialog();

		void set_dialog_data(Track * track, const TrackPoints::iterator & current_tp_iter, bool is_route);
		void reset_dialog_data(void);
		void set_dialog_title(const QString & track_name);

		void set_coord_mode(CoordMode coord_mode);

		/* Dialog action codes. */
		enum class Action {
			InsertTpAfter,
			DeleteSelectedTp,
			SplitAtSelectedTp,
			PreviousPoint,
			NextPoint,
		};

	public slots:
		void clicked_cb(int response);

	private slots:
		void sync_coord_entry_to_current_tp_cb(void);
		void sync_altitude_entry_to_current_tp_cb(void);
		void sync_timestamp_entry_to_current_tp_cb(const Time & timestamp);
		void sync_empty_timestamp_entry_to_current_tp_cb(void);
		bool sync_name_entry_to_current_tp_cb(const QString & new_name);

	signals:
		/* Coordinates of one of track's trackpoints has changed its coordinates. */
		void trackpoint_coordinates_changed(void);

	private:
		void update_timestamp_widget(const Trackpoint * tp);
		bool set_timestamp_of_current_tp(const Time & timestamp);

		Trackpoint * current_tp = NULL;
		Track * current_track = NULL;

		bool sync_to_current_tp_block = false;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TRW_TPWIN_H_ */
