/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Project started in 2016 by forking viking project.
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2007, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2007-2008, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2012-2014, Rob Norris <rw_norris@hotmail.com>
 * Copyright (c) 2016 Kamil Ignacak <acerion@wp.pl>
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

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <time.h>

#include <QDebug>
#include <QLineEdit>

#include "window.h"
#include "layer_trw_track_internal.h"
#include "layer_trw_track_properties_dialog.h"
#include "layer_trw.h"
#include "vikutils.h"
#include "util.h"
#include "ui_util.h"
#include "dialog.h"
#include "application_state.h"
#include "globals.h"
#include "preferences.h"




using namespace SlavGPS;




void SlavGPS::track_properties_dialog(Window * parent, Track * trk, bool start_on_stats)
{
	TrackPropertiesDialog dialog(QObject::tr("Track Profile"), trk, start_on_stats, parent);
	dialog.create_properties_page();
	dialog.create_statistics_page();
	trk->set_properties_dialog(&dialog);
	dialog.exec();
	trk->clear_properties_dialog();
}




TrackPropertiesDialog::TrackPropertiesDialog(QString const & title, Track * a_trk, bool start_on_stats, Window * a_parent) : QDialog(a_parent)
{
	this->setWindowTitle(tr("%1 - Track Properties").arg(a_trk->name));

	this->trk = a_trk;

	this->button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(this->button_box, &QDialogButtonBox::accepted, this, &TrackPropertiesDialog::dialog_accept_cb);
	connect(this->button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

	this->tabs = new QTabWidget();
	this->vbox = new QVBoxLayout;

	QLayout * old_layout = NULL;

	this->properties_form = new QFormLayout();
	this->properties_area = new QWidget();
	old_layout = this->properties_area->layout();
	delete old_layout;
	this->properties_area->setLayout(properties_form);
	this->tabs->addTab(this->properties_area, tr("Properties"));

	this->statistics_form = new QFormLayout();
	this->statistics_area = new QWidget();
	old_layout = this->statistics_area->layout();
	delete old_layout;
	this->statistics_area->setLayout(statistics_form);
	this->tabs->addTab(this->statistics_area, tr("Statistics"));

	this->vbox->addWidget(this->tabs);
	this->vbox->addWidget(this->button_box);


	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);

	if (start_on_stats) {
		this->tabs->setCurrentIndex(1);
	}
}




void TrackPropertiesDialog::create_properties_page(void)
{
	this->w_comment = new QLineEdit(this);
	if (!this->trk->comment.isEmpty()) {
		this->w_comment->insert(this->trk->comment);
	}
	this->properties_form->addRow(tr("Comment:"), this->w_comment);


	this->w_description = new QLineEdit(this);
	if (!this->trk->description.isEmpty()) {
		this->w_description->insert(this->trk->description);
	}
	this->properties_form->addRow(tr("Description:"), this->w_description);


	this->w_source = new QLineEdit(this);
	if (!this->trk->source.isEmpty()) {
		this->w_source->insert(this->trk->source);
	}
	this->properties_form->addRow(tr("Source:"), this->w_source);


	this->w_type = new QLineEdit(this);
	if (!this->trk->type.isEmpty()) {
		this->w_type->insert(this->trk->type);
	}
	this->properties_form->addRow(tr("Type:"), this->w_type);


	/* TODO: use this->trk->color. */
       	this->w_color = new SGColorButton(this->trk->color, NULL);
	this->properties_form->addRow(tr("Color:"), this->w_color);


	QStringList options;
	options << tr("No")
		<< tr("Centre")
		<< tr("Start only")
		<< tr("End only")
		<< tr("Start and End")
		<< tr("Centre, Start and End");
	this->w_namelabel = new QComboBox(NULL);
	this->w_namelabel->insertItems(0, options);
	this->w_namelabel->setCurrentIndex((int) this->trk->draw_name_mode);
	this->properties_form->addRow(tr("Draw Name:"), this->w_namelabel);


	this->w_number_distlabels = new QSpinBox();
	this->w_number_distlabels->setMinimum(0);
	this->w_number_distlabels->setMaximum(100);
	this->w_number_distlabels->setSingleStep(1);
	this->w_number_distlabels->setToolTip(tr("Maximum number of distance labels to be shown"));
	this->w_number_distlabels->setValue(this->trk->max_number_dist_labels);
	this->properties_form->addRow(tr("Distance Labels:"), this->w_number_distlabels);
}




void TrackPropertiesDialog::create_statistics_page(void)
{
	QString tmp_string;

	/* NB This value not shown yet - but is used by internal calculations. */
	this->track_length = this->trk->get_length();
	this->track_length_inc_gaps = this->trk->get_length_including_gaps();
	DistanceUnit distance_unit = Preferences::get_unit_distance();

	static char tmp_buf[50];

	double tr_len = this->track_length;
	tmp_string = get_distance_string(tr_len, distance_unit);
	this->w_track_length = ui_label_new_selectable(tmp_string, this);
	this->statistics_form->addRow(tr("Track Length:"), this->w_track_length);


	unsigned long tp_count = this->trk->get_tp_count();
	snprintf(tmp_buf, sizeof(tmp_buf), "%lu", tp_count);
	this->w_tp_count = ui_label_new_selectable(tmp_buf, this);
	this->statistics_form->addRow(tr("Trackpoints:"), this->w_tp_count);


	unsigned int seg_count = this->trk->get_segment_count() ;
	snprintf(tmp_buf, sizeof(tmp_buf), "%u", seg_count);
	this->w_segment_count = ui_label_new_selectable(tmp_buf, this);
	this->statistics_form->addRow(tr("Segments:"), this->w_segment_count);


	snprintf(tmp_buf, sizeof(tmp_buf), "%lu", this->trk->get_dup_point_count());
	this->w_duptp_count = ui_label_new_selectable(tmp_buf, this);
	this->statistics_form->addRow(tr("Duplicate Points:"), this->w_duptp_count);


	SpeedUnit speed_units = Preferences::get_unit_speed();
	double tmp_speed = this->trk->get_max_speed();
	if (tmp_speed == 0) {
		tmp_string = QObject::tr("No Data");
	} else {
		tmp_string = get_speed_string(tmp_speed, speed_units);
	}
	this->w_max_speed = ui_label_new_selectable(tmp_string, this);
	this->statistics_form->addRow(QObject::tr("Max Speed:"), this->w_max_speed);


	tmp_speed = this->trk->get_average_speed();
	if (tmp_speed == 0) {
		tmp_string = tr("No Data");
	} else {
		tmp_string = get_speed_string(tmp_speed, speed_units);
	}
	this->w_avg_speed = ui_label_new_selectable(tmp_string, this);
	this->statistics_form->addRow(tr("Avg. Speed:"), this->w_avg_speed);


	/* Use 60sec as the default period to be considered stopped.
	   This is the TrackWaypoint draw stops default value 'LayerTRWPainter::track_min_stop_length'.
	   However this variable is not directly accessible - and I don't expect it's often changed from the default
	   so ATM just put in the number. */
	tmp_speed = this->trk->get_average_speed_moving(60);
	if (tmp_speed == 0) {
		tmp_string = tr("No Data");
	} else {
		tmp_string = get_speed_string(tmp_speed, speed_units);
	}
	this->w_mvg_speed = ui_label_new_selectable(tmp_string, this);
	this->statistics_form->addRow(tr("Moving Avg. Speed:"), this->w_mvg_speed);


	QString result;
	switch (distance_unit) {
	case DistanceUnit::KILOMETRES:
		/* Even though kilometres, the average distance between points is going to be quite small so keep in metres. */
		result = tr("%1 m").arg((tp_count - seg_count) == 0 ? 0 : tr_len / (tp_count - seg_count), 0, 'f', 2);
		break;
	case DistanceUnit::MILES:
		result = tr("%1 miles").arg((tp_count - seg_count) == 0 ? 0 : VIK_METERS_TO_MILES(tr_len / (tp_count - seg_count)), 0, 'f', 3);
		break;
	case DistanceUnit::NAUTICAL_MILES:
		result = tr("%1 NM").arg((tp_count - seg_count) == 0 ? 0 : VIK_METERS_TO_NAUTICAL_MILES(tr_len / (tp_count - seg_count)), 0, 'f', 3);
		break;
	default:
		qDebug() << "EE: Track Properties Dialog: can't get distance unit for 'avg. dist between tps.'; distance_unit = " << (int) distance_unit;
	}
	this->w_avg_dist = ui_label_new_selectable(result, this);
	this->statistics_form->addRow(tr("Avg. Dist. Between TPs:"), this->w_avg_dist);


	int elev_points = 100; /* this->trk->size()? */
	TrackData altitudes = this->trk->make_track_data_altitude_over_distance(elev_points);
	if (!altitudes.valid) {
		altitudes.y_min = VIK_DEFAULT_ALTITUDE;
		altitudes.y_max = VIK_DEFAULT_ALTITUDE;
	} else {
		altitudes.calculate_min_max();
	}

	result = "";
	HeightUnit height_unit = Preferences::get_unit_height();
	if (altitudes.y_min == VIK_DEFAULT_ALTITUDE) {
		result = tr("No Data");
	} else {
		switch (height_unit) {
		case HeightUnit::METRES:
			result = tr("%1 m - %2 m").arg(altitudes.y_min, 0, 'f', 0).arg(altitudes.y_max, 0, 'f', 0);
			break;
		case HeightUnit::FEET:
			result = tr("%1 feet - %2 feet").arg(VIK_METERS_TO_FEET(altitudes.y_min), 0, 'f', 0).arg(VIK_METERS_TO_FEET(altitudes.y_max), 0, 'f', 0);
			break;
		default:
			result = tr("--");
			qDebug() << "EE: Track Properties Dialog: can't get height unit for 'elevation range'; height_unit = " << (int) height_unit;
		}
	}
	this->w_elev_range = ui_label_new_selectable(result, this);
	this->statistics_form->addRow(tr("Elevation Range:"), this->w_elev_range);


	this->trk->get_total_elevation_gain(&altitudes.y_max, &altitudes.y_min);
	if (altitudes.y_min == VIK_DEFAULT_ALTITUDE) {
		result = tr("No Data");
	} else {
		switch (height_unit) {
		case HeightUnit::METRES:
			result = tr("%1 m / %2 m").arg(altitudes.y_max, 0, 'f', 0).arg(altitudes.y_min, 0, 'f', 0);
			break;
		case HeightUnit::FEET:
			result = tr("%1 feet / %2 feet").arg(VIK_METERS_TO_FEET(altitudes.y_max), 0, 'f', 0).arg(VIK_METERS_TO_FEET(altitudes.y_min), 0, 'f', 0);
			break;
		default:
			result = tr("--");
			qDebug() << "EE: Track Properties Dialog: can't get height unit for 'total elevation gain/loss'; height_unit = " << (int) height_unit;
		}
	}
	this->w_elev_gain = ui_label_new_selectable(result, this);
	this->statistics_form->addRow(tr("Total Elevation Gain/Loss:"), this->w_elev_gain);


	if (!this->trk->empty()
	    && (*this->trk->trackpoints.begin())->timestamp) {

		time_t t1 = (*this->trk->trackpoints.begin())->timestamp;
		time_t t2 = (*std::prev(this->trk->trackpoints.end()))->timestamp;

		/* Notional center of a track is simply an average of the bounding box extremities. */
		const LatLon center((this->trk->bbox.north + this->trk->bbox.south) / 2, (this->trk->bbox.east + trk->bbox.west) / 2);
		LayerTRW * parent_layer = (LayerTRW *) this->trk->owning_layer;
		const Coord coord(center, parent_layer->get_coord_mode());
		this->tz = vu_get_tz_at_location(&coord);


		QString msg = SGUtils::get_time_string(t1, "%c", &coord, this->tz);
		this->w_time_start = ui_label_new_selectable(msg, this);
		this->statistics_form->addRow(tr("Start:"), this->w_time_start);


		msg = SGUtils::get_time_string(t2, "%c", &coord, this->tz);
		this->w_time_end = ui_label_new_selectable(msg, this);
		this->statistics_form->addRow(tr("End:"), this->w_time_end);


		int total_duration_s = (int)(t2-t1);
		int segments_duration_s = (int) this->trk->get_duration(false);
		int total_duration_m = total_duration_s/60;
		int segments_duration_m = segments_duration_s/60;
		result = tr("%1 minutes - %2 minutes moving").arg(total_duration_m).arg(segments_duration_m);
		this->w_time_dur = ui_label_new_selectable(result, this);
		this->statistics_form->addRow(tr("Duration:"), this->w_time_dur);

		/* A tooltip to show in more readable hours:minutes. */
		char tip_buf_total[20];
		unsigned int h_tot = total_duration_s/3600;
		unsigned int m_tot = (total_duration_s - h_tot*3600)/60;
		snprintf(tip_buf_total, sizeof(tip_buf_total), "%d:%02d", h_tot, m_tot);
		char tip_buf_segments[20];
		unsigned int h_seg = segments_duration_s/3600;
		unsigned int m_seg = (segments_duration_s - h_seg*3600)/60;
		snprintf(tip_buf_segments, sizeof(tip_buf_segments), "%d:%02d", h_seg, m_seg);
		this->w_time_dur->setToolTip(tr("%1 total - %s in segments").arg(tip_buf_total).arg(tip_buf_segments));
	} else {
		this->w_time_start = ui_label_new_selectable(tr("No Data"), this);
		this->statistics_form->addRow(tr("Start:"), this->w_time_start);

		this->w_time_end = ui_label_new_selectable(tr("No Data"), this);
		this->statistics_form->addRow(tr("End:"), this->w_time_end);

		this->w_time_dur = ui_label_new_selectable(tr("No Data"), this);
		this->statistics_form->addRow(tr("Duration:"), this->w_time_dur);
	}
}




void TrackPropertiesDialog::dialog_accept_cb(void) /* Slot. */
{
	/* FIXME: check and make sure the track still exists before doing anything to it. */

	trk->set_comment(this->w_comment->text().toUtf8().data());
	trk->set_description(this->w_description->text().toUtf8().data());
	trk->set_source(this->w_source->text().toUtf8().data());
	trk->set_type(this->w_type->text().toUtf8().data());
	trk->color = this->w_color->get_color();
	trk->draw_name_mode = (TrackDrawNameMode) this->w_namelabel->currentIndex();
	trk->max_number_dist_labels = this->w_number_distlabels->value();

	qDebug() << "II: Track Properties Dialog: selected draw name mode #" << (int) trk->draw_name_mode;

	LayerTRW * parent_layer = (LayerTRW *) this->trk->owning_layer;
	if (this->trk->type_id == "sg.trw.track") {
		parent_layer->get_tracks_node().update_tree_view(this->trk);
	} else {
		parent_layer->get_routes_node().update_tree_view(this->trk);
	}
	parent_layer->emit_layer_changed();

	this->accept();
}
