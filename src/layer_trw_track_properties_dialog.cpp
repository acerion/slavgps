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




//#include <cmath>
//#include <cstdlib>
//#include <cstring>
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
#include "preferences.h"
#include "measurements.h"




using namespace SlavGPS;




#define PREFIX ": Layer TRW Track Properties:" << __FUNCTION__ << __LINE__ << ">"




void SlavGPS::track_properties_dialog(Track * trk, Window * parent)
{
	TrackPropertiesDialog dialog(QObject::tr("Track Properties"), trk, parent);
	dialog.create_properties_page();
	trk->set_properties_dialog(&dialog);
	dialog.exec();
	trk->clear_properties_dialog();
}




void SlavGPS::track_statistics_dialog(Track * trk, Window * parent)
{
	TrackStatisticsDialog dialog(QObject::tr("Track Statistics"), trk, parent);
	dialog.create_statistics_page();
	dialog.exec();
}




TrackPropertiesDialog::TrackPropertiesDialog(QString const & title, Track * a_trk, Window * a_parent) : BasicDialog(a_parent)
{
	this->setWindowTitle(tr("%1 - Track Properties").arg(a_trk->name));

	this->trk = a_trk;
}





TrackStatisticsDialog::TrackStatisticsDialog(QString const & title, Track * a_trk, Window * a_parent) : BasicDialog(a_parent)
{
	this->setWindowTitle(tr("%1 - Track Statistics").arg(a_trk->name));

	this->trk = a_trk;
}




void TrackPropertiesDialog::create_properties_page(void)
{
	int row = 0;

	this->w_comment = new QLineEdit(this);
	if (!this->trk->comment.isEmpty()) {
		this->w_comment->insert(this->trk->comment);
	}
	this->grid->addWidget(new QLabel(tr("Comment:")), row, 0);
	this->grid->addWidget(this->w_comment, row, 1);
	row++;


	this->w_description = new QLineEdit(this);
	if (!this->trk->description.isEmpty()) {
		this->w_description->insert(this->trk->description);
	}
	this->grid->addWidget(new QLabel(tr("Description:")), row, 0);
	this->grid->addWidget(this->w_description, row, 1);
	row++;


	this->w_source = new QLineEdit(this);
	if (!this->trk->source.isEmpty()) {
		this->w_source->insert(this->trk->source);
	}
	this->grid->addWidget(new QLabel(tr("Source:")), row, 0);
	this->grid->addWidget(this->w_source, row, 1);
	row++;


	this->w_type = new QLineEdit(this);
	if (!this->trk->type.isEmpty()) {
		this->w_type->insert(this->trk->type);
	}
	this->grid->addWidget(new QLabel(tr("Type:")), row, 0);
	this->grid->addWidget(this->w_type, row, 1);
	row++;


       	this->w_color = new ColorButtonWidget(this->trk->color, NULL);
	this->grid->addWidget(new QLabel(tr("Color:")), row, 0);
	this->grid->addWidget(this->w_color, row, 1);
	row++;


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
	this->grid->addWidget(new QLabel(tr("Draw Name:")), row, 0);
	this->grid->addWidget(this->w_namelabel, row, 1);
	row++;


	this->w_number_distlabels = new QSpinBox();
	this->w_number_distlabels->setMinimum(0);
	this->w_number_distlabels->setMaximum(100);
	this->w_number_distlabels->setSingleStep(1);
	this->w_number_distlabels->setToolTip(tr("Maximum number of distance labels to be shown"));
	this->w_number_distlabels->setValue(this->trk->max_number_dist_labels);
	this->grid->addWidget(new QLabel(tr("Distance Labels:")), row, 0);
	this->grid->addWidget(this->w_number_distlabels, row, 1);
	row++;

	connect(this->button_box, &QDialogButtonBox::accepted, this, &TrackPropertiesDialog::dialog_accept_cb);
	connect(this->button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

}




void TrackStatisticsDialog::create_statistics_page(void)
{
	QString tmp_string;
	int row = 0;

	/* NB This value not shown yet - but is used by internal calculations. */
	this->track_length = this->trk->get_length();
	this->track_length_inc_gaps = this->trk->get_length_including_gaps();
	DistanceUnit distance_unit = Preferences::get_unit_distance();


	const double tr_len = this->track_length;
	tmp_string = get_distance_string(tr_len, distance_unit);
	this->w_track_length = ui_label_new_selectable(tmp_string, this);
	this->grid->addWidget(new QLabel(tr("Track Length:")), row, 0);
	this->grid->addWidget(this->w_track_length, row, 1);
	row++;


	const unsigned long tp_count = this->trk->get_tp_count();
	tmp_string = QString("%1").arg(tp_count);
	this->w_tp_count = ui_label_new_selectable(tmp_string, this);
	this->grid->addWidget(new QLabel(tr("Trackpoints:")), row, 0);
	this->grid->addWidget(this->w_tp_count, row, 1);
	row++;


	const unsigned int seg_count = this->trk->get_segment_count() ;
	tmp_string = QString("%1").arg(seg_count);
	this->w_segment_count = ui_label_new_selectable(tmp_string, this);
	this->grid->addWidget(new QLabel(tr("Segments:")), row, 0);
	this->grid->addWidget(this->w_segment_count, row, 1);
	row++;


	tmp_string = QString("%1").arg(this->trk->get_dup_point_count());
	this->w_duptp_count = ui_label_new_selectable(tmp_string, this);
	this->grid->addWidget(new QLabel(tr("Duplicate Points:")), row, 0);
	this->grid->addWidget(this->w_duptp_count, row, 1);
	row++;


	SpeedUnit speed_units = Preferences::get_unit_speed();
	double tmp_speed = this->trk->get_max_speed();
	if (tmp_speed == 0) {
		tmp_string = QObject::tr("No Data");
	} else {
		tmp_string = get_speed_string(tmp_speed, speed_units);
	}
	this->w_max_speed = ui_label_new_selectable(tmp_string, this);
	this->grid->addWidget(new QLabel(QObject::tr("Max Speed:")), row, 0);
	this->grid->addWidget(this->w_max_speed, row, 1);
	row++;


	tmp_speed = this->trk->get_average_speed();
	if (tmp_speed == 0) {
		tmp_string = tr("No Data");
	} else {
		tmp_string = get_speed_string(tmp_speed, speed_units);
	}
	this->w_avg_speed = ui_label_new_selectable(tmp_string, this);
	this->grid->addWidget(new QLabel(tr("Average Speed:")), row, 0);
	this->grid->addWidget(this->w_avg_speed, row, 1);
	row++;


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
	this->grid->addWidget(new QLabel(tr("Moving Average Speed:")), row, 0);
	this->grid->addWidget(this->w_mvg_speed, row, 1);
	row++;


	QString result;
	switch (distance_unit) {
	case DistanceUnit::Kilometres:
		/* Even though kilometres, the average distance between points is going to be quite small so keep in metres. */
		result = tr("%1 m").arg((tp_count - seg_count) == 0 ? 0 : tr_len / (tp_count - seg_count), 0, 'f', 2);
		break;
	case DistanceUnit::Miles:
		result = tr("%1 miles").arg((tp_count - seg_count) == 0 ? 0 : VIK_METERS_TO_MILES(tr_len / (tp_count - seg_count)), 0, 'f', 3);
		break;
	case DistanceUnit::NauticalMiles:
		result = tr("%1 NM").arg((tp_count - seg_count) == 0 ? 0 : VIK_METERS_TO_NAUTICAL_MILES(tr_len / (tp_count - seg_count)), 0, 'f', 3);
		break;
	default:
		qDebug() << "EE" PREFIX << "invalid distance unit" << (int) distance_unit;
		break;
	}
	this->w_avg_dist = ui_label_new_selectable(result, this);
	this->grid->addWidget(new QLabel(tr("Average Distance Between Trackpoints:")), row, 0);
	this->grid->addWidget(this->w_avg_dist, row, 1);
	row++;


	int elev_points = 100; /* this->trk->size()? */
	TrackData altitudes = this->trk->make_track_data_altitude_over_distance(elev_points);
	if (!altitudes.valid) {
		altitudes.y_min = VIK_DEFAULT_ALTITUDE;
		altitudes.y_max = VIK_DEFAULT_ALTITUDE;
	} else {
		altitudes.calculate_min_max();
	}

	result = "";
	const HeightUnit height_unit = Preferences::get_unit_height();
	if (altitudes.y_min == VIK_DEFAULT_ALTITUDE) {
		result = tr("No Data");
	} else {
		switch (height_unit) {
		case HeightUnit::Metres:
			result = tr("%1 m - %2 m").arg(altitudes.y_min, 0, 'f', 0).arg(altitudes.y_max, 0, 'f', 0);
			break;
		case HeightUnit::Feet:
			result = tr("%1 feet - %2 feet").arg(VIK_METERS_TO_FEET(altitudes.y_min), 0, 'f', 0).arg(VIK_METERS_TO_FEET(altitudes.y_max), 0, 'f', 0);
			break;
		default:
			result = tr("--");
			qDebug() << "EE" PREFIX << "invalid height unit" << (int) height_unit;
			break;
		}
	}
	this->w_elev_range = ui_label_new_selectable(result, this);
	this->grid->addWidget(new QLabel(tr("Elevation Range:")), row, 0);
	this->grid->addWidget(this->w_elev_range, row, 1);
	row++;


	this->trk->get_total_elevation_gain(&altitudes.y_max, &altitudes.y_min);
	if (altitudes.y_min == VIK_DEFAULT_ALTITUDE) {
		result = tr("No Data");
	} else {
		switch (height_unit) {
		case HeightUnit::Metres:
			result = tr("%1 m / %2 m").arg(altitudes.y_max, 0, 'f', 0).arg(altitudes.y_min, 0, 'f', 0);
			break;
		case HeightUnit::Feet:
			result = tr("%1 feet / %2 feet").arg(VIK_METERS_TO_FEET(altitudes.y_max), 0, 'f', 0).arg(VIK_METERS_TO_FEET(altitudes.y_min), 0, 'f', 0);
			break;
		default:
			result = tr("--");
			qDebug() << "EE" PREFIX << "invalid height unit" << (int) height_unit;
			break;
		}
	}
	this->w_elev_gain = ui_label_new_selectable(result, this);
	this->grid->addWidget(new QLabel(tr("Total Elevation Gain/Loss:")), row, 0);
	this->grid->addWidget(this->w_elev_gain, row, 1);
	row++;


	if (!this->trk->empty()
	    && (*this->trk->trackpoints.begin())->timestamp) {

		time_t t1 = (*this->trk->trackpoints.begin())->timestamp;
		time_t t2 = (*std::prev(this->trk->trackpoints.end()))->timestamp;

		/* Notional center of a track is simply an average of the bounding box extremities. */
		const LatLon center((this->trk->bbox.north + this->trk->bbox.south) / 2, (this->trk->bbox.east + trk->bbox.west) / 2);
		LayerTRW * parent_layer = (LayerTRW *) this->trk->get_owning_layer();
		const Coord coord(center, parent_layer->get_coord_mode());
		this->tz = TZLookup::get_tz_at_location(coord);


		QString msg = SGUtils::get_time_string(t1, Qt::TextDate, coord, this->tz);
		this->w_time_start = ui_label_new_selectable(msg, this);
		this->grid->addWidget(new QLabel(tr("Start:")), row, 0);
		this->grid->addWidget(this->w_time_start, row, 1);
		row++;


		msg = SGUtils::get_time_string(t2, Qt::TextDate, coord, this->tz);
		this->w_time_end = ui_label_new_selectable(msg, this);
		this->grid->addWidget(new QLabel(tr("End:")), row, 0);
		this->grid->addWidget(this->w_time_end, row, 1);
		row++;


		const int total_duration_s = (int) (t2 - t1);
		const int segments_duration_s = (int) this->trk->get_duration(false);
		result = tr("%1 total - %2 in segments")
			.arg(Measurements::get_duration_string(total_duration_s))
			.arg(Measurements::get_duration_string(segments_duration_s));
		this->w_time_dur = ui_label_new_selectable(result, this);
		this->grid->addWidget(new QLabel(tr("Duration:")), row, 0);
		this->grid->addWidget(this->w_time_dur, row, 1);
		row++;
	} else {
		this->w_time_start = ui_label_new_selectable(tr("No Data"), this);
		this->grid->addWidget(new QLabel(tr("Start:")), row, 0);
		this->grid->addWidget(this->w_time_start, row, 1);
		row++;

		this->w_time_end = ui_label_new_selectable(tr("No Data"), this);
		this->grid->addWidget(new QLabel(tr("End:")), row, 0);
		this->grid->addWidget(this->w_time_end, row, 1);
		row++;

		this->w_time_dur = ui_label_new_selectable(tr("No Data"), this);
		this->grid->addWidget(new QLabel(tr("Duration:")), row, 0);
		this->grid->addWidget(this->w_time_dur, row, 1);
		row++;
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

	LayerTRW * parent_layer = (LayerTRW *) this->trk->get_owning_layer();
	if (this->trk->type_id == "sg.trw.track") {
		parent_layer->get_tracks_node().update_tree_view(this->trk);
	} else {
		parent_layer->get_routes_node().update_tree_view(this->trk);
	}
	parent_layer->emit_layer_changed("TRW - Track Properties Dialog");

	this->accept();
}
