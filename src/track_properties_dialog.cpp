/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2007, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2007-2008, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2012-2014, Rob Norris <rw_norris@hotmail.com>
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
 *
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

#include "dems.h"
#include "vikutils.h"
#include "util.h"
#include "ui_util.h"
#include "dialog.h"
#include "settings.h"
#include "globals.h"




using namespace SlavGPS;




void SlavGPS::track_properties_dialog(Window * parent,
				      LayerTRW * layer,
				      Track * trk,
				      bool start_on_stats)
{
	TrackPropertiesDialog dialog(QString("Track Profile"), layer, trk, parent);
	dialog.create_properties_page();
	dialog.create_statistics_page();
	dialog.exec();
}




TrackPropertiesDialog::TrackPropertiesDialog(QString const & title, LayerTRW * a_layer, Track * a_trk, Window * a_parent) : QDialog(a_parent)
{
	this->setWindowTitle(QString(_("%1 - Track Properties")).arg(a_trk->name));

	this->trw = a_layer;
	this->trk = a_trk;

#ifdef K
	static char *label_texts[] = {
		(char *) N_("<b>Comment:</b>"),
		(char *) N_("<b>Description:</b>"),
		(char *) N_("<b>Source:</b>"),
		(char *) N_("<b>Type:</b>"),
		(char *) N_("<b>Color:</b>"),
		(char *) N_("<b>Draw Name:</b>"),
		(char *) N_("<b>Distance Labels:</b>"),
	};
	static char *stats_texts[] = {
		(char *) N_("<b>Track Length:</b>"),
		(char *) N_("<b>Trackpoints:</b>"),
		(char *) N_("<b>Segments:</b>"),
		(char *) N_("<b>Duplicate Points:</b>"),
		(char *) N_("<b>Max Speed:</b>"),
		(char *) N_("<b>Avg. Speed:</b>"),
		(char *) N_("<b>Moving Avg. Speed:</b>"),
		(char *) N_("<b>Avg. Dist. Between TPs:</b>"),
		(char *) N_("<b>Elevation Range:</b>"),
		(char *) N_("<b>Total Elevation Gain/Loss:</b>"),
		(char *) N_("<b>Start:</b>"),
		(char *) N_("<b>End:</b>"),
		(char *) N_("<b>Duration:</b>"),
	};
#endif

	this->button_box = new QDialogButtonBox();
	this->button_ok = this->button_box->addButton("&OK", QDialogButtonBox::AcceptRole);
	this->button_cancel = this->button_box->addButton("&Cancel", QDialogButtonBox::RejectRole);
	this->button_delete_duplicates = this->button_box->addButton("&Delete Duplicates", QDialogButtonBox::ActionRole);
	this->button_delete_duplicates->setIcon(QIcon::fromTheme("list-delete"));
	if (trk->get_dup_point_count() <= 0) {
		this->button_delete_duplicates->setEnabled(false);
	}

#if 0
	this->signalMapper = new QSignalMapper(this);
	connect(this->button_ok,                SIGNAL (released()), signalMapper, SLOT (map()));
	connect(this->button_cancel,            SIGNAL (released()), signalMapper, SLOT (map()));
	connect(this->button_delete_duplicates, SIGNAL (released()), signalMapper, SLOT (map()));

	this->signalMapper->setMapping(this->button_ok,                SG_TRACK_CLOSE);
	this->signalMapper->setMapping(this->button_cancel,            SG_TRACK_INSERT);
	this->signalMapper->setMapping(this->button_delete_duplicates, SG_TRACK_DELETE);
#endif

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

#ifdef K
	this->trk->set_property_dialog(dialog);
	if (start_on_stats) {
		gtk_notebook_set_current_page(GTK_NOTEBOOK(this->tabs), 1);
	}
#endif
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
       	this->w_color = new SGColorButton(QColor("red"), NULL);
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
	this->w_namelabel->setCurrentIndex(this->trk->draw_name_mode);
	this->properties_form->addRow(QString("Draw Name:"), this->w_namelabel);


	this->w_number_distlabels = new QSpinBox();
	this->w_number_distlabels->setValue(this->trk->max_number_dist_labels);
	this->w_number_distlabels->setMinimum(0);
	this->w_number_distlabels->setMaximum(100);
	this->w_number_distlabels->setSingleStep(1);
	this->w_number_distlabels->setToolTip(_("Maximum number of distance labels to be shown"));
	this->properties_form->addRow(QString("Distance Labels:"), this->w_number_distlabels);
}




void TrackPropertiesDialog::create_statistics_page(void)
{
	/* NB This value not shown yet - but is used by internal calculations. */
	this->track_length = this->trk->get_length();
	this->track_length_inc_gaps = this->trk->get_length_including_gaps();
	DistanceUnit distance_unit = a_vik_get_units_distance();

	static char tmp_buf[50];

	double tr_len = this->track_length;
	get_distance_string(tmp_buf, sizeof (tmp_buf), distance_unit, tr_len);
	this->w_track_length = ui_label_new_selectable(tmp_buf, this);
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


	SpeedUnit speed_units = a_vik_get_units_speed();
	double tmp_speed = this->trk->get_max_speed();
	if (tmp_speed == 0) {
		snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
	} else {
		get_speed_string(tmp_buf, sizeof (tmp_buf), speed_units, tmp_speed);
	}
	this->w_max_speed = ui_label_new_selectable(tmp_buf, this);
	this->statistics_form->addRow(QString("Max Speed:"), this->w_max_speed);


	tmp_speed = this->trk->get_average_speed();
	if (tmp_speed == 0) {
		snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
	} else {
		get_speed_string(tmp_buf, sizeof (tmp_buf), speed_units, tmp_speed);
	}
	this->w_avg_speed = ui_label_new_selectable(tmp_buf, this);
	this->statistics_form->addRow(QString("Avg. Speed:"), this->w_avg_speed);


	/* Use 60sec as the default period to be considered stopped.
	   This is the TrackWaypoint draw stops default value 'trw->stop_length'.
	   However this variable is not directly accessible - and I don't expect it's often changed from the default
	   so ATM just put in the number. */
	tmp_speed = this->trk->get_average_speed_moving(60);
	if (tmp_speed == 0) {
		snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
	} else {
		get_speed_string(tmp_buf, sizeof (tmp_buf), speed_units, tmp_speed);
	}
	this->w_mvg_speed = ui_label_new_selectable(tmp_buf, this);
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
		fprintf(stderr, "CRITICAL: Houston, we've had a problem. distance=%d\n", distance_unit);
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

	HeightUnit height_units = a_vik_get_units_height();
	if (min_alt == VIK_DEFAULT_ALTITUDE) {
		snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
	} else {
		switch (height_units) {
		case HeightUnit::METRES:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.0f m - %.0f m", min_alt, max_alt);
			break;
		case HeightUnit::FEET:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.0f feet - %.0f feet", VIK_METERS_TO_FEET(min_alt), VIK_METERS_TO_FEET(max_alt));
			break;
		default:
			snprintf(tmp_buf, sizeof(tmp_buf), "--");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. height=%d\n", height_units);
		}
	}
	this->w_elev_range = ui_label_new_selectable(tmp_buf, this);
	this->statistics_form->addRow(QString("Elevation Range:"), this->w_elev_range);


	this->trk->get_total_elevation_gain(&max_alt, &min_alt);
	if (min_alt == VIK_DEFAULT_ALTITUDE) {
		snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
	} else {
		switch (height_units) {
		case HeightUnit::METRES:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.0f m / %.0f m", max_alt, min_alt);
			break;
		case HeightUnit::FEET:
			snprintf(tmp_buf, sizeof(tmp_buf), "%.0f feet / %.0f feet", VIK_METERS_TO_FEET(max_alt), VIK_METERS_TO_FEET(min_alt));
			break;
		default:
			snprintf(tmp_buf, sizeof(tmp_buf), "--");
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. height=%d\n", height_units);
		}
	}
	this->w_elev_gain = ui_label_new_selectable(tmp_buf, this);
	this->statistics_form->addRow(QString("Total Elevation Gain/Loss:"), this->w_elev_gain);


	if (!this->trk->empty()
	    && (*this->trk->trackpointsB->begin())->timestamp) {

		time_t t1 = (*this->trk->trackpointsB->begin())->timestamp;
		time_t t2 = (*std::prev(this->trk->trackpointsB->end()))->timestamp;

		VikCoord vc;
		/* Notional center of a track is simply an average of the bounding box extremities. */
		struct LatLon center = { (this->trk->bbox.north + this->trk->bbox.south) / 2, (this->trk->bbox.east + trk->bbox.west) / 2 };
		vik_coord_load_from_latlon(&vc, this->trw->get_coord_mode(), &center);
		this->tz = vu_get_tz_at_location(&vc);


		char * msg = vu_get_time_string(&t1, "%c", &vc, this->tz);
		this->w_time_start = ui_label_new_selectable(msg, this);
		free(msg);
		this->statistics_form->addRow(QString("Start:"), this->w_time_start);


		msg = vu_get_time_string(&t2, "%c", &vc, this->tz);
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
		this->w_time_start = ui_label_new_selectable(_("No Data"), this);
		this->statistics_form->addRow(QString("Start:"), this->w_time_start);

		this->w_time_end = ui_label_new_selectable(_("No Data"), this);
		this->statistics_form->addRow(QString("End:"), this->w_time_end);

		this->w_time_dur = ui_label_new_selectable(_("No Data"), this);
		this->statistics_form->addRow(QString("Duration:"), this->w_time_dur);
	}
}




/**
 * Update this property dialog
 * e.g. if the track has been renamed.
 */
void SlavGPS::track_properties_dialog_update(Track * trk)
{
	/* If not displayed do nothing. */
	if (!trk->property_dialog) {
		return;
	}

	/* Update title with current name. */
	if (trk->name) {
#ifdef K
		char * title = g_strdup_printf(_("%s - Track Properties"), trk->name);
		gtk_window_set_title(GTK_WINDOW(trk->property_dialog), title);
		free(title);
#endif
	}

}




void TrackPropertiesDialog::dialog_response_cb(int resp) /* Slot. */
{
#ifdef K

	/* FIXME: check and make sure the track still exists before doing anything to it. */

	switch (resp) {
	case GTK_RESPONSE_DELETE_EVENT: /* Received delete event (not from buttons). */
	case GTK_RESPONSE_REJECT:
		break;
	case GTK_RESPONSE_ACCEPT:
		trk->set_comment(this->w_comment->toUtf()->data());
		trk->set_description(this->w_description->toUtf()->data());
		trk->set_source(this->w_source->toUtf()->data())
		trk->set_type(this->w_type->toUtf()->data());
		gtk_color_button_get_color(GTK_COLOR_BUTTON(this->w_color), &(trk->color));
		trk->draw_name_mode = (TrackDrawnameType) gtk_combo_box_get_active(GTK_COMBO_BOX(this->w_namelabel));
		qDebug() << "II: Track Properties Dialog: selected item no" << trk->draw_name_mode;
		trk->max_number_dist_labels = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(this->w_number_distlabels));
		this->trw->update_treeview(this->trk);
		trw->emit_changed();
		break;
	case VIK_TRW_LAYER_PROPWIN_DEL_DUP:
		trk->remove_dup_points(); /* NB ignore the returned answer. */
		/* As we could have seen the nuber of dulplicates that would be deleted in the properties statistics tab,
		   choose not to inform the user unnecessarily. */

		/* Above operation could have deleted current_tp or last_tp. */
		trw->cancel_tps_of_track(trk);
		trw->emit_changed();
		break;
	default:
		fprintf(stderr, "DEBUG: unknown response\n");
		return;
	}


#endif
}
