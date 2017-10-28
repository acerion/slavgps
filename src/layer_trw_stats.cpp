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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cassert>
#include <time.h>

#include <QDebug>

#include "window.h"
#include "layer_aggregate.h"
#include "layer_trw.h"
#include "layer_trw_stats.h"
#include "layer_trw_track_internal.h"
#include "ui_util.h"
#include "settings.h"
#include "globals.h"
#include "vikutils.h"
#include "util.h"




using namespace SlavGPS;




#define N_LABELS 12
static const char * labels[N_LABELS] = {
	"Number of Tracks",
	"Date Range",
	"Total Length",
	"Average Length",
	"Max Speed",
	"Avg. Speed",
	"Minimum Altitude",
	"Maximum Altitude",
	"Total Elevation Gain/Loss",
	"Avg. Elevation Gain/Loss",
	"Total Duration",
	"Avg. Duration",
};




/**
 * Return a widget to hold the stats information in a table grid layout
 */
static QGridLayout * create_stats_table(QDialog * parent)
{
	QGridLayout * grid = new QGridLayout(parent);

	for (int row = 0; row < N_LABELS; row++) {
		QLabel * label = new QLabel(QObject::tr(labels[row]));
		grid->addWidget(label, row, 0); /* 0 == first column. */


		QLabel * value = new QLabel("");
		grid->addWidget(value, row, 1); /* 1 == second column. */
	}

	return grid;
}




/**
 * Display given statistics in table widget
 */
void TRWStatsDialog::display_stats(TrackStatistics & stats)
{
	QGridLayout * grid = this->stats_table;
	const int col = 1; /* We will be modifying only column of values. */


	int cnt = 0;
	QString tmp_string;


	/* 0: Number of Tracks */
	((QLabel *) grid->itemAtPosition(0, col)->widget())->setText(QString("%1").arg(stats.count));


	if (stats.count == 0) {
		/* Blank all other fields. */
		for (int row = 1; row < N_LABELS; row++) {
			((QLabel *) grid->itemAtPosition(row, col)->widget())->setText("--");
		}
		return;
	}


#ifdef K
	/* 1: Date Range */

	/* Check for potential date range. */
	/* Test if the same day by comparing the date string of the timestamp. */
	GDate* gdate_start = g_date_new();
	g_date_set_time_t (gdate_start, stats.start_time);
	char time_start[32];
	g_date_strftime(time_start, sizeof(time_start), "%x", gdate_start);
	g_date_free(gdate_start);

	GDate* gdate_end = g_date_new ();
	g_date_set_time_t (gdate_end, stats.end_time);
	char time_end[32];
	g_date_strftime(time_end, sizeof(time_end), "%x", gdate_end);
	g_date_free(gdate_end);

	if (stats.start_time == stats.end_time) {
		tmp_string = tr("No Data");
	} else if (strncmp(time_start, time_end, 32)) {
		tmp_string = tr("%1 --> %2").arg(time_start).arg(time_end);
	} else {
		tmp_string = time_start;
	}
	((QLabel *) grid->itemAtPosition(1, col)->widget())->setText(tmp_string);
#endif


	/* 2: Total Length */
	tmp_string = get_distance_string(stats.length, Preferences::get_unit_distance());
	((QLabel *) grid->itemAtPosition(2, col)->widget())->setText(tmp_string);


	/* 3: Average Length */
	tmp_string = get_distance_string(stats.length / stats.count, Preferences::get_unit_distance());
	((QLabel *) grid->itemAtPosition(3, col)->widget())->setText(tmp_string);


	/* 4: Max Speed */
	SpeedUnit speed_unit = Preferences::get_unit_speed();
	if (stats.max_speed > 0) {
		tmp_string = get_speed_string(stats.max_speed, speed_unit);
	} else {
		tmp_string = "--";
	}
	((QLabel *) grid->itemAtPosition(4, col)->widget())->setText(tmp_string);


	/* 5: Avg. Speed */
	if (stats.duration > 0) {
		tmp_string = get_speed_string(stats.length / stats.duration, speed_unit);
	} else {
		tmp_string = "--";
	}
	((QLabel *) grid->itemAtPosition(5, col)->widget())->setText(tmp_string);


	switch (Preferences::get_unit_height()) {
		/* Note always round off height value output since sub unit accuracy is overkill. */
	case HeightUnit::FEET:

		/* 6: Minimum Altitude */
		if (stats.min_alt != VIK_VAL_MIN_ALT) {
			tmp_string = tr("%1 feet").arg((int) round(VIK_METERS_TO_FEET(stats.min_alt)));
		} else {
			tmp_string = "--";
		}
		((QLabel *) grid->itemAtPosition(6, col)->widget())->setText(tmp_string);


		/* 7: Maximum Altitude */
		if (stats.max_alt != VIK_VAL_MAX_ALT) {
			tmp_string = tr("%1 feet").arg((int) round(VIK_METERS_TO_FEET(stats.max_alt)));
		} else {
			tmp_string = "--";
		}
		((QLabel *) grid->itemAtPosition(7, col)->widget())->setText(tmp_string);


		/* 8: Total Elevation Gain/Loss */
		tmp_string = tr("%1 feet / %2 feet").arg((int) round(VIK_METERS_TO_FEET(stats.elev_gain))).arg((int) round(VIK_METERS_TO_FEET(stats.elev_loss)));
		((QLabel *) grid->itemAtPosition(8, col)->widget())->setText(tmp_string);


		/* 9: Avg. Elevation Gain/Loss */
		tmp_string = tr("%1 feet / %2 feet").arg((int) round(VIK_METERS_TO_FEET(stats.elev_gain/stats.count))).arg((int) round(VIK_METERS_TO_FEET(stats.elev_loss/stats.count)));
		((QLabel *) grid->itemAtPosition(9, col)->widget())->setText(tmp_string);

		break;
	default:
		/* HeightUnit::METRES */

		/* 6: Minimum Altitude */
		if (stats.min_alt != VIK_VAL_MIN_ALT) {
			tmp_string = tr("%1 m").arg((int) round(stats.min_alt));
		} else {
			tmp_string = "--";
		}
		((QLabel *) grid->itemAtPosition(6, col)->widget())->setText(tmp_string);


		/* 7: Maximum Altitude */
		if (stats.max_alt != VIK_VAL_MAX_ALT) {
			tmp_string = tr("%1 m").arg((int) round(stats.max_alt));
		} else {
			tmp_string = "--";
		}
		((QLabel *) grid->itemAtPosition(7, col)->widget())->setText(tmp_string);


		/* 8: Total Elevation Gain/Loss */
		tmp_string = tr("%1 m / %2 m").arg((int) round(stats.elev_gain)).arg((int) round(stats.elev_loss));
		((QLabel *) grid->itemAtPosition(8, col)->widget())->setText(tmp_string);


		/* 9: Avg. Elevation Gain/Loss */
		tmp_string = tr("%1 m / %2 m").arg((int) round(stats.elev_gain/stats.count)).arg((int) round(stats.elev_loss/stats.count));
		((QLabel *) grid->itemAtPosition(9, col)->widget())->setText(tmp_string);

		break;
	}


	/* 10: Total Duration. */
	int days    = (int) (stats.duration / (60 * 60 * 24));
	int hours   = (int) floor((stats.duration - (days * 60 * 60 * 24)) / (60 * 60));
	int minutes = (int) ((stats.duration - (days * 60 * 60 * 24) - (hours * 60 * 60)) / 60);
	tmp_string = tr("%1:%2:%3 days:hrs:mins").arg(days).arg(hours, 2, 10, (QChar) '0').arg(minutes, 2, 10, (QChar) '0');
	((QLabel *) grid->itemAtPosition(10, col)->widget())->setText(tmp_string);


	/* 11: Average Duration. */
	int avg_dur = stats.duration / stats.count;
	hours   = (int) floor(avg_dur / (60 * 60));
	minutes = (int) ((avg_dur - (hours * 60 * 60)) / 60);
	tmp_string = tr("%1:%2 hrs:mins").arg(hours).arg(minutes, 2, 10, (QChar) '0');
	((QLabel *) grid->itemAtPosition(11, col)->widget())->setText(tmp_string);
}




/**
   @brief Collect statistics for each item in this->tracks list

   @stats: statistics object to be filled
   @include_invisible: whether to include invisible layers and tracks
*/
void TRWStatsDialog::collect_stats(TrackStatistics & stats, bool include_invisible)
{
	for (auto iter = this->tracks->begin(); iter != this->tracks->end(); iter++) {
		Track * trk = *iter;
		LayerTRW * trw = trk->get_parent_layer_trw();
		assert (trw->type == LayerType::TRW);
		qDebug() << "II: Layer TRW Stats: collecting stats with layer/tracks/routes/include visibility:"
			 << trw->visible
			 << trw->get_tracks_visibility()
			 << trw->get_routes_visibility()
			 << include_invisible;
		stats.add_track_maybe(trk, trw->visible, trw->get_tracks_visibility(), trw->get_routes_visibility(), include_invisible);
	}

	return;
}




void TRWStatsDialog::include_invisible_toggled_cb(int state)
{
	bool include_invisible = (bool) state;
	qDebug() << "DD: Layer TRW Stats: include invisible items:" << include_invisible;

	/* Delete old list of items. */
	if (this->tracks) {
		/* kamilTODO: delete this->tracks. */
	}

	/* Get the latest list of items to analyse. */
	/* kamilTODO: why do we need to get the latest list on checkbox toggle? */
	if (this->layer->type == LayerType::TRW) {
		this->tracks = ((LayerTRW *) this->layer)->create_tracks_list(this->type_id);
	} else if (layer->type == LayerType::AGGREGATE) {
		this->tracks = ((LayerAggregate *) this->layer)->create_tracks_list(this->type_id);
	} else {
		assert (0);
	}

	TrackStatistics stats;
	this->collect_stats(stats, include_invisible);
	this->display_stats(stats);
}




#define VIK_SETTINGS_ANALYSIS_DO_INVISIBLE "track_analysis_do_invisible"




TRWStatsDialog::~TRWStatsDialog()
{
	/* Save current invisible value for next time. */
	bool do_invisible = this->checkbox->isChecked();
	ApplicationState::set_boolean(VIK_SETTINGS_ANALYSIS_DO_INVISIBLE, do_invisible);


	//free(this->stats_table);

	/* kamilTODO: delete this->tracks here? */
}




/**
   @name: name of object, for which the stats will be calculated
   @layer: layer containing given tracks/routes
   @type_id: type of TRW sublayer to show stats for
   @parent: parent widget

   Display a dialog with stats across many tracks.
*/
void SlavGPS::layer_trw_show_stats(const QString & name, Layer * layer, const QString & type_id_, QWidget * parent)
{
	TRWStatsDialog * dialog = new TRWStatsDialog(parent);
	dialog->setWindowTitle(QObject::tr("Statistics"));


	QVBoxLayout * vbox = new QVBoxLayout();;
	QLayout * old_layout = dialog->layout();
	delete old_layout;
	dialog->setLayout(vbox);

	QLabel * name_l = new QLabel(name);
	name_l->setStyleSheet("font-weight: bold");
	vbox->addWidget(name_l);


	/* Get previous value (if any) from the settings. */
	bool include_invisible;
	if (!ApplicationState::get_boolean(VIK_SETTINGS_ANALYSIS_DO_INVISIBLE, &include_invisible)) {
		include_invisible = true;
	}

	dialog->layer = layer;
	dialog->type_id = type_id_;

	if (layer->type == LayerType::TRW) {
		dialog->tracks = ((LayerTRW *) layer)->create_tracks_list(dialog->type_id);
	} else if (layer->type == LayerType::AGGREGATE) {
		dialog->tracks = ((LayerAggregate *) layer)->create_tracks_list(dialog->type_id);
	} else {
		qDebug() << "EE: Layer TRW Stats: wrong layer type" << (int) layer->type;
		assert (0);
	}
	dialog->stats_table = create_stats_table(dialog);
	vbox->addLayout(dialog->stats_table);

	/* Analysis seems reasonably quick
	   unless you have really large numbers of tracks (i.e. many many thousands or a really slow computer).
	   One day might store stats in the track itself... */
	TrackStatistics stats;
	dialog->collect_stats(stats, include_invisible);
	dialog->display_stats(stats);

	dialog->checkbox = new QCheckBox(QObject::tr("Include Invisible Items"), dialog);
	dialog->checkbox->setChecked(include_invisible);
	QObject::connect(dialog->checkbox, SIGNAL (stateChanged(int)), dialog, SLOT (include_invisible_toggled_cb(int)));
	vbox->addWidget(dialog->checkbox);


	QDialogButtonBox * button_box = new QDialogButtonBox();
	button_box->addButton(QDialogButtonBox::Ok);
	QObject::connect(button_box, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
	vbox->addWidget(button_box);


	dialog->exec();

	delete dialog;

	return;
}
