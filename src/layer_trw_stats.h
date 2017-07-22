/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013 Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_LAYER_TRW_STATS_H_
#define _SG_LAYER_TRW_STATS_H_




#include <list>

#include <QObject>
#include <QDialog>
#include <QWidget>
#include <QGridLayout>
#include <QCheckBox>

#include "globals.h"
#include "track.h"
#include "track_statistics.h"




namespace SlavGPS {




	class Window;
	class Layer;
	class track_layer_t;




	class TRWStatsDialog : public QDialog {
		Q_OBJECT
	public:
		TRWStatsDialog() {};
		TRWStatsDialog(QWidget * parent_) : QDialog(parent_) {};
		~TRWStatsDialog();


		QGridLayout * stats_table = NULL;
		QCheckBox * checkbox = NULL;
		std::list<track_layer_t *> * tracks_and_layers = NULL;
		Layer * layer = NULL; /* Just a reference. */
		SublayerType sublayer_type;

		void collect_stats(TrackStatistics & stats, bool include_invisible);
		void display_stats(TrackStatistics & stats);

	public slots:
		void include_invisible_toggled_cb(int state);

	};




	void layer_trw_show_stats(Window * window, const QString & name, Layer * layer, SublayerType sublayer_type);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TRW_STATS_H_ */
