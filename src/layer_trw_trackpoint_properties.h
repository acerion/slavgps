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

#ifndef _SG_LAYER_TRW_TRACKPOINT_PROPERTIES_H_
#define _SG_LAYER_TRW_TRACKPOINT_PROPERTIES_H_




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
#include "widget_coord.h"
#include "layer_trw_track.h"
#include "layer_trw_point_properties.h"




namespace SlavGPS {




	class Track;
	class Trackpoint;




	class TpPropertiesWidget : public PointPropertiesWidget {
		Q_OBJECT
	public:
		TpPropertiesWidget(QWidget * parent = NULL);

		sg_ret build_widgets(CoordMode coord_mode, QWidget * parent_widget);

		/* Erase all contents from widgets, as if nothing was
		   presented by the widgets. */
		void clear_widgets(void);

		void build_buttons(QWidget * parent_widget);


		QLabel * course = NULL;
		QLabel * diff_dist = NULL;
		QLabel * diff_time = NULL;
		QLabel * diff_speed = NULL; /* Speed calculated from distance and time diff between two trackpoints. */
		QLabel * gps_speed = NULL; /* GPS speed taken from given trackpoint. */
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

		sg_ret dialog_data_set(Track * track);
		void dialog_data_reset(void);
		void set_dialog_title(const QString & title);

		/* Dialog action codes. */
		enum class Action {
			InsertTpAfter,
			DeleteSelectedPoint,
			SplitAtSelectedTp,
			PreviousPoint,
			NextPoint,
		};

	public slots:
		void clicked_cb(int response);

		/* Called to inform the dialog that a change of
		   selection in main tree view has been made. */
		void tree_view_selection_changed_cb(void);

	private slots:
		bool sync_name_entry_to_current_point_cb(const QString & name);
		void sync_coord_widget_to_current_point_cb(void);
		void sync_altitude_widget_to_current_point_cb(void);
		bool sync_timestamp_widget_to_current_point_cb(const Time & timestamp);
		bool sync_empty_timestamp_widget_to_current_point_cb(void);

	signals:
		/* Coordinates of one of track's trackpoints has changed its coordinates. */
		void point_coordinates_changed(void);

	private:
		void update_timestamp_widget(const Trackpoint * tp);

		Trackpoint * current_point = NULL;
		Track * current_track = NULL;

		/* Don't pass values currently set/entered in widgets
		   to currently edited point, because the entering is
		   being done as part of initializing properties
		   dialog. */
		bool skip_syncing_to_current_point = false;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TRW_TRACKPOINT_PROPERTIES_H_ */
