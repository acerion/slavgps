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

#include "track_properties_dialog.h"
#include "layer_trw.h"
#include "vikutils.h"
#include "util.h"
#include "ui_util.h"
#include "dialog.h"
#include "settings.h"
#include "globals.h"
#include "preferences.h"
#include "track.h"




using namespace SlavGPS;




void SlavGPS::track_properties_dialog(Window * parent,
				      LayerTRW * layer,
				      Track * trk,
				      bool start_on_stats)
{
	TrackPropertiesDialog dialog(QString("Track Profile"), layer, trk, start_on_stats, parent);
	dialog.create_properties_page();
	dialog.create_statistics_page();
	trk->set_properties_dialog(&dialog);
	dialog.exec();
	trk->clear_properties_dialog();
}




TrackPropertiesDialog::TrackPropertiesDialog(QString const & title, LayerTRW * a_layer, Track * a_trk, bool start_on_stats, Window * a_parent) : QDialog(a_parent)
{
	this->setWindowTitle(tr("%1 - Track Properties").arg(a_trk->name));

	this->trw = a_layer;
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
	this->tabs->addTab(this->properties_area, _("Properties"));

	this->statistics_form = new QFormLayout();
	this->statistics_area = new QWidget();
	old_layout = this->statistics_area->layout();
	delete old_layout;
	this->statistics_area->setLayout(statistics_form);
	this->tabs->addTab(this->statistics_area, _("Statistics"));

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
	if (this->trk->comment) {
		this->w_comment->insert(this->trk->comment);
	}
	this->properties_form->addRow(QString("Comment:"), this->w_comment);


	this->w_description = new QLineEdit(this);
	if (this->trk->description) {
		this->w_description->insert(this->trk->description);
	}
	this->properties_form->addRow(QString("Description:"), this->w_description);


	this->w_source = new QLineEdit(this);
	if (this->trk->source) {
		this->w_source->insert(this->trk->source);
	}
	this->properties_form->addRow(QString("Source:"), this->w_source);


	this->w_type = new QLineEdit(this);
	if (this->trk->type) {
		this->w_type->insert(this->trk->type);
	}
	this->properties_form->addRow(QString("Type:"), this->w_type);


	/* TODO: use this->trk->color. */
       	this->w_color = new SGColorButton(this->trk->color, NULL);
	this->properties_form->addRow(QString("Color:"), this->w_color);


	QStringList options;
	options << _("No")
		<< _("Centre")
		<< _("Start only")
		<< _("End only")
		<< _("Start and End")
		<< _("Centre, Start and End");
	this->w_namelabel = new QComboBox(NULL);
	this->w_namelabel->insertItems(0, options);
	this->w_namelabel->setCurrentIndex((int) this->trk->draw_name_mode);
	this->properties_form->addRow(QString("Draw Name:"), this->w_namelabel);


	this->w_number_distlabels = new QSpinBox();
	this->w_number_distlabels->setMinimum(0);
	this->w_number_distlabels->setMaximum(100);
	this->w_number_distlabels->setSingleStep(1);
	this->w_number_distlabels->setToolTip(tr("Maximum number of distance labels to be shown"));
	this->w_number_distlabels->setValue(this->trk->max_number_dist_labels);
	this->properties_form->addRow(QString("Distance Labels:"), this->w_number_distlabels);
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
	this->statistics_form->addRow(QString("Track Length:"), this->w_track_length);


	unsigned long tp_count = this->trk->get_tp_count();
	snprintf(tmp_buf, sizeof(tmp_buf), "%lu", tp_count);
	this->w_tp_count = ui_label_new_selectable(tmp_buf, this);
	this->statistics_form->addRow(QString("Trackpoints:"), this->w_tp_count);


	unsigned int seg_count = this->trk->get_segment_count() ;
	snprintf(tmp_buf, sizeof(tmp_buf), "%u", seg_count);
	this->w_segment_count = ui_label_new_selectable(tmp_buf, this);
	this->statistics_form->addRow(QString("Segments:"), this->w_segment_count);


	snprintf(tmp_buf, sizeof(tmp_buf), "%lu", this->trk->get_dup_point_count());
	this->w_duptp_count = ui_label_new_selectable(tmp_buf, this);
	this->statistics_form->addRow(QString("Duplicate Points:"), this->w_duptp_count);


	SpeedUnit speed_units = Preferences::get_unit_speed();
	double tmp_speed = this->trk->get_max_speed();
	if (tmp_speed == 0) {
		tmp_string = QObject::tr("No Data");
	} else {
		tmp_string = get_speed_string(tmp_speed, speed_units);
	}
	this->w_max_speed = ui_label_new_selectable(tmp_string, this);
	this->statistics_form->addRow(QString("Max Speed:"), this->w_max_speed);


	tmp_speed = this->trk->get_average_speed();
	if (tmp_speed == 0) {
		tmp_string = QObject::tr("No Data");
	} else {
		tmp_string = get_speed_string(tmp_speed, speed_units);
	}
	this->w_avg_speed = ui_label_new_selectable(tmp_string, this);
	this->statistics_form->addRow(QString("Avg. Speed:"), this->w_avg_speed);


	/* Use 60sec as the default period to be considered stopped.
	   This is the TrackWaypoint draw stops default value 'trw->stop_length'.
	   However this variable is not directly accessible - and I don't expect it's often changed from the default
	   so ATM just put in the number. */
	tmp_speed = this->trk->get_average_speed_moving(60);
	if (tmp_speed == 0) {
		tmp_string = QObject::tr("No Data");
	} else {
		tmp_string = get_speed_string(tmp_speed, speed_units);
	}
	this->w_mvg_speed = ui_label_new_selectable(tmp_string, this);
	this->statistics_form->addRow(QString("Moving Avg. Speed:"), this->w_mvg_speed);


	switch (distance_unit) {
	case DistanceUnit::KILOMETRES:
		/* Even though kilometres, the average distance between points is going to be quite small so keep in metres. */
		snprintf(tmp_buf, sizeof(tmp_buf), "%.2f m", (tp_count - seg_count) == 0 ? 0 : tr_len / (tp_count - seg_count));
		break;
	case DistanceUnit::MILES:
		snprintf(tmp_buf, sizeof(tmp_buf), "%.3f miles", (tp_count - seg_count) == 0 ? 0 : VIK_METERS_TO_MILES(tr_len / (tp_count - seg_count)));
		break;
	case DistanceUnit::NAUTICAL_MILES:
		snprintf(tmp_buf, sizeof(tmp_buf), "%.3f NM", (tp_count - seg_count) == 0 ? 0 : VIK_METERS_TO_NAUTICAL_MILES(tr_len / (tp_count - seg_count)));
		break;
	default:
		qDebug() << "EE: Track Properties Dialog: can't get distance unit for 'avg. dist between tps.'; distance_unit = " << (int) distance_unit;
	}
	this->w_avg_dist = ui_label_new_selectable(tmp_buf, this);
	this->statistics_form->addRow(QString("Avg. Dist. Between TPs:"), this->w_avg_dist);


	double min_alt, max_alt;
	int elev_points = 100; /* this->trk->size()? */
	double * altitudes = this->trk->make_elevation_map(elev_points);
	if (!altitudes) {
		min_alt = VIK_DEFAULT_ALTITUDE;
		max_alt = VIK_DEFAULT_ALTITUDE;
	} else {
		minmax_array(altitudes, &min_alt, &max_alt, true, elev_points);
	}
	free(altitudes);
	altitudes = NULL;

	HeightUnit height_unit = Preferences::get_unit_height();
	if (min_alt == VIK_DEFAULT_ALTITUDE) {
		snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
	} else {
		switch (height_unit) {
		case HeightUnit::METRES:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.0f m - %.0f m", min_alt, max_alt);
			break;
		case HeightUnit::FEET:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.0f feet - %.0f feet", VIK_METERS_TO_FEET(min_alt), VIK_METERS_TO_FEET(max_alt));
			break;
		default:
			snprintf(tmp_buf, sizeof(tmp_buf), "--");
			qDebug() << "EE: Track Properties Dialog: can't get height unit for 'elevation range'; height_unit = " << (int) height_unit;
		}
	}
	this->w_elev_range = ui_label_new_selectable(tmp_buf, this);
	this->statistics_form->addRow(QString("Elevation Range:"), this->w_elev_range);


	this->trk->get_total_elevation_gain(&max_alt, &min_alt);
	if (min_alt == VIK_DEFAULT_ALTITUDE) {
		snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
	} else {
		switch (height_unit) {
		case HeightUnit::METRES:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.0f m / %.0f m", max_alt, min_alt);
			break;
		case HeightUnit::FEET:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.0f feet / %.0f feet", VIK_METERS_TO_FEET(max_alt), VIK_METERS_TO_FEET(min_alt));
			break;
		default:
			snprintf(tmp_buf, sizeof(tmp_buf), "--");
			qDebug() << "EE: Track Properties Dialog: can't get height unit for 'total elevation gain/loss'; height_unit = " << (int) height_unit;
		}
	}
	this->w_elev_gain = ui_label_new_selectable(tmp_buf, this);
	this->statistics_form->addRow(QString("Total Elevation Gain/Loss:"), this->w_elev_gain);


	if (!this->trk->empty()
	    && (*this->trk->trackpointsB->begin())->timestamp) {

		time_t t1 = (*this->trk->trackpointsB->begin())->timestamp;
		time_t t2 = (*std::prev(this->trk->trackpointsB->end()))->timestamp;

		/* Notional center of a track is simply an average of the bounding box extremities. */
		struct LatLon center = { (this->trk->bbox.north + this->trk->bbox.south) / 2, (this->trk->bbox.east + trk->bbox.west) / 2 };
		const Coord coord(center, this->trw->get_coord_mode());
		this->tz = vu_get_tz_at_location(&coord);


		char * msg = vu_get_time_string(&t1, "%c", &coord, this->tz);
		this->w_time_start = ui_label_new_selectable(msg, this);
		free(msg);
		this->statistics_form->addRow(QString("Start:"), this->w_time_start);


		msg = vu_get_time_string(&t2, "%c", &coord, this->tz);
		this->w_time_end = ui_label_new_selectable(msg, this);
		free(msg);
		this->statistics_form->addRow(QString("End:"), this->w_time_end);


		int total_duration_s = (int)(t2-t1);
		int segments_duration_s = (int) this->trk->get_duration(false);
		int total_duration_m = total_duration_s/60;
		int segments_duration_m = segments_duration_s/60;
		snprintf(tmp_buf, sizeof(tmp_buf), _("%d minutes - %d minutes moving"), total_duration_m, segments_duration_m);
		this->w_time_dur = ui_label_new_selectable(tmp_buf, this);
		this->statistics_form->addRow(QString("Duration:"), this->w_time_dur);

		/* A tooltip to show in more readable hours:minutes. */
		char tip_buf_total[20];
		unsigned int h_tot = total_duration_s/3600;
		unsigned int m_tot = (total_duration_s - h_tot*3600)/60;
		snprintf(tip_buf_total, sizeof(tip_buf_total), "%d:%02d", h_tot, m_tot);
		char tip_buf_segments[20];
		unsigned int h_seg = segments_duration_s/3600;
		unsigned int m_seg = (segments_duration_s - h_seg*3600)/60;
		snprintf(tip_buf_segments, sizeof(tip_buf_segments), "%d:%02d", h_seg, m_seg);
		char * tip = g_strdup_printf(_("%s total - %s in segments"), tip_buf_total, tip_buf_segments);
		this->w_time_dur->setToolTip(tip);
		free(tip);
	} else {
		this->w_time_start = ui_label_new_selectable(tr("No Data"), this);
		this->statistics_form->addRow(QString("Start:"), this->w_time_start);

		this->w_time_end = ui_label_new_selectable(tr("No Data"), this);
		this->statistics_form->addRow(QString("End:"), this->w_time_end);

		this->w_time_dur = ui_label_new_selectable(tr("No Data"), this);
		this->statistics_form->addRow(QString("Duration:"), this->w_time_dur);
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

	this->trw->update_treeview(this->trk);
	this->trw->emit_changed();

	this->accept();
}
