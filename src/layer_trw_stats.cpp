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




#include <cassert>
#include <vector>
#include <time.h>




#include <QDateTime>
#include <QDebug>




#include "window.h"
#include "layer_aggregate.h"
#include "layer_trw.h"
#include "layer_trw_stats.h"
#include "layer_trw_track_internal.h"
#include "ui_util.h"
#include "application_state.h"
#include "vikutils.h"
#include "util.h"
#include "preferences.h"




using namespace SlavGPS;




#define SG_MODULE "Layer TRW Stats"
#define NONE_TEXT "--"

/* We have here a two-column table. First column is with names of parameters,
   the second column is with values of parameter. */
#define NAME_COLUMN   0
#define VALUE_COLUMN  1




static const std::vector<SGLabelID> labels = {
	SGLabelID(QObject::tr("Number of Tracks"),             (int) TRWStatsRow::NumOfTracks),
	SGLabelID(QObject::tr("Date Range"),                   (int) TRWStatsRow::DateChange),
	SGLabelID(QObject::tr("Total Length"),                 (int) TRWStatsRow::TotalLength),
	SGLabelID(QObject::tr("Average Length"),               (int) TRWStatsRow::AverageLength),
	SGLabelID(QObject::tr("Maximum Speed"),                (int) TRWStatsRow::MaximumSpeed),
	SGLabelID(QObject::tr("Average Speed"),                (int) TRWStatsRow::AverageSpeed),
	SGLabelID(QObject::tr("Minimum Altitude"),             (int) TRWStatsRow::MininumAltitude),
	SGLabelID(QObject::tr("Maximum Altitude"),             (int) TRWStatsRow::MaximumAltitude),
	SGLabelID(QObject::tr("Total Elevation Gain/Loss"),    (int) TRWStatsRow::TotalElevationDelta),
	SGLabelID(QObject::tr("Average Elevation Gain/Loss"),  (int) TRWStatsRow::AverageElevationDelta),
	SGLabelID(QObject::tr("Total Duration"),               (int) TRWStatsRow::TotalDuration),
	SGLabelID(QObject::tr("Average Duration"),             (int) TRWStatsRow::AverageDuration),
};




StatsTable::StatsTable(QDialog * parent) : QGridLayout(parent)
{
	for (unsigned row = 0; row < labels.size(); row++) {
		QLabel * name = new QLabel(labels[row].label);
		this->addWidget(name, row, NAME_COLUMN);

		QLabel * value = new QLabel("");
		this->addWidget(value, row, VALUE_COLUMN);
	}
}




StatsTable::~StatsTable()
{
	qDebug() << SG_PREFIX_D << "stats table destructor called";
}




QLabel * StatsTable::get_value_label(TRWStatsRow row)
{
	QLabel * label = dynamic_cast<QLabel *>(this->itemAtPosition((int) row, VALUE_COLUMN)->widget());
	return label;
}




/**
   Display given statistics in table widget
*/
void TRWStatsDialog::display_stats(TrackStatistics & stats)
{
	QString tmp_string;
	const SpeedType::Unit speed_unit = Preferences::get_unit_speed();


	/* Number of Tracks */
	this->stats_table->get_value_label(TRWStatsRow::NumOfTracks)->setText(QString("%1").arg(stats.count));

	if (stats.count == 0) {
		/* Blank all other fields. */
		for (int row = 1; row < this->stats_table->rowCount(); row++) {
			this->stats_table->get_value_label((TRWStatsRow) row)->setText(NONE_TEXT);
		}
		return;
	}


	const AltitudeType::Unit height_unit = Preferences::get_unit_height();


	/* Date Range */

	/* Check for potential date range. */
	/* Test if the same day by comparing the date string of the timestamp. */
	/* Viking's C code used strftime()'s %x specifier: "The preferred date representation for current locale without the time". */
	const QString time_start = stats.start_time.get_time_string(Qt::SystemLocaleLongDate);
	const QString time_end = stats.end_time.get_time_string(Qt::SystemLocaleLongDate);

	if (stats.start_time == stats.end_time) {
		tmp_string = tr("No Data");
	} else if (time_start != time_end) {
		tmp_string = tr("%1 --> %2").arg(time_start).arg(time_end);
	} else {
		tmp_string = time_start;
	}
	this->stats_table->get_value_label(TRWStatsRow::DateChange)->setText(tmp_string);



	/* Total Length */
	const DistanceType::Unit distance_unit = Preferences::get_unit_distance();
	this->stats_table->get_value_label(TRWStatsRow::TotalLength)->setText(stats.length.convert_to_unit(distance_unit).to_nice_string());



	/* Average Length of all tracks. */
	Distance avg_distance = stats.length.convert_to_unit(distance_unit);
	avg_distance /= stats.count; /* Average of all tracks. */
	this->stats_table->get_value_label(TRWStatsRow::AverageLength)->setText(avg_distance.to_nice_string());



	/* Max Speed */
	this->stats_table->get_value_label(TRWStatsRow::MaximumSpeed)->setText(stats.max_speed.convert_to_unit(speed_unit).to_string());



	/* Avg. Speed */
	const bool valid = stats.sum_of_durations.is_valid() && !stats.sum_of_durations.is_zero();
	Speed avg_speed;
	avg_speed.make_speed(stats.length, stats.sum_of_durations);
	this->stats_table->get_value_label(TRWStatsRow::AverageSpeed)->setText(avg_speed.convert_to_unit(speed_unit).to_string());



	/* Minimum Altitude */
	if (stats.min_alt.is_valid()) {
		tmp_string = stats.min_alt.convert_to_unit(height_unit).to_string();
	} else {
		tmp_string = NONE_TEXT;
	}
	this->stats_table->get_value_label(TRWStatsRow::MininumAltitude)->setText(tmp_string);



	/* Maximum Altitude */
	if (stats.max_alt.is_valid()) {
		tmp_string = stats.max_alt.convert_to_unit(height_unit).to_string();
	} else {
		tmp_string = NONE_TEXT;
	}
	this->stats_table->get_value_label(TRWStatsRow::MaximumAltitude)->setText(tmp_string);



	/* Total Elevation Gain/Loss */
	tmp_string = tr("%1 / %2")
		.arg(stats.elev_gain.convert_to_unit(height_unit).to_string())
		.arg(stats.elev_loss.convert_to_unit(height_unit).to_string());
	this->stats_table->get_value_label(TRWStatsRow::TotalElevationDelta)->setText(tmp_string);



	/* Average Elevation Gain/Loss */
	const Altitude avg_gain = stats.elev_gain / stats.count;
	const Altitude avg_loss = stats.elev_loss / stats.count;
	tmp_string = tr("%1 / %2")
		.arg(avg_gain.convert_to_unit(height_unit).to_string())
		.arg(avg_loss.convert_to_unit(height_unit).to_string());
	this->stats_table->get_value_label(TRWStatsRow::AverageElevationDelta)->setText(tmp_string);


	/* Total Duration. */
	this->stats_table->get_value_label(TRWStatsRow::TotalDuration)->setText(stats.sum_of_durations.to_string());


	const Duration average_duration = stats.sum_of_durations / stats.count;
	this->stats_table->get_value_label(TRWStatsRow::AverageDuration)->setText(average_duration.to_string());
}




/**
   @brief Collect statistics for each item in this->tree_items list

   @stats: statistics object to be filled
   @include_invisible: whether to include invisible layers and tracks
*/
void TRWStatsDialog::collect_stats(TrackStatistics & stats, bool include_invisible)
{
	for (auto iter = this->tree_items.begin(); iter != this->tree_items.end(); iter++) {
		Track * trk = (Track *) *iter;
		const LayerTRW * trw = trk->get_parent_layer_trw();
		assert (trw->m_kind == LayerKind::TRW);
		qDebug() << SG_PREFIX_I << "Collecting stats with layer/tracks/routes/include visibility:"
			 << trw->is_visible()
			 << trw->get_tracks_visibility()
			 << trw->get_routes_visibility()
			 << include_invisible;
		stats.add_track_maybe(trk, trw->is_visible(), trw->get_tracks_visibility(), trw->get_routes_visibility(), include_invisible);
	}

	return;
}




void TRWStatsDialog::include_invisible_toggled_cb(int state)
{
	const bool include_invisible = (bool) state;
	qDebug() << SG_PREFIX_D << "Include invisible items:" << include_invisible;

	/* Re-use existing this->tree_items. No need to re-get them from
	   layers. this->tree_items contains both visible and invisible
	   tracks, so it's a matter of checking in collect_stats()
	   which ones to include in stats. */

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

	delete this->stats_table;
}




void SlavGPS::layer_trw_show_stats(const QString & name, Layer * layer, const std::list<SGObjectTypeID> & wanted_types, QWidget * parent)
{
	assert (layer->m_kind == LayerKind::TRW || layer->m_kind == LayerKind::Aggregate);


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

	layer->get_tree_items(dialog->tree_items, wanted_types);
	dialog->stats_table = new StatsTable(dialog);
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
