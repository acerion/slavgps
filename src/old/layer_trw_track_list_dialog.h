/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_TRACK_LIST_DIALOG_H_
#define _SG_TRACK_LIST_DIALOG_H_




#include <list>




#include <QString>
#include <QWidget>
#include <QDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QStandardItemModel>
#include <QTableView>
#include <QContextMenuEvent>




#include "measurements.h"




namespace SlavGPS {




	class Layer;
	class LayerTRW;
	class Track;
	class Viewport;




	class TrackListDialog : public QDialog {
		Q_OBJECT
	public:
		TrackListDialog(QString const & title, QWidget * parent = NULL);
		~TrackListDialog();
		void build_model(bool hide_layer_names);

		std::list<Track *> tracks;

	private slots:
		void copy_selected_cb(void);
		void track_view_cb(void);
		void track_statistics_cb(void);
		void track_properties_cb(void);
		void accept_cb(void);

	private:
		void add_row(Track * trk, DistanceUnit distance_unit, SpeedUnit speed_unit, HeightUnit height_unit);
		void contextMenuEvent(QContextMenuEvent * event);

		QWidget * parent = NULL;
		QDialogButtonBox * button_box = NULL;
		QVBoxLayout * vbox = NULL;

		QStandardItemModel * model = NULL;
		QTableView * view = NULL;

		/* Track selected in list. */
		Track * selected_track = NULL;

		Qt::DateFormat date_time_format = Qt::ISODate;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_TRACK_LIST_DIALOG_H_ */
