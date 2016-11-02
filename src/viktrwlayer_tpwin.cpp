/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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
#include <ctime>

#include <QDebug>

#include "uibuilder_qt.h"
#include "viktrwlayer_tpwin.h"
#if 0
#include "viking.h"
#include "coords.h"
#include "coord.h"
#include "track.h"
#include "waypoint.h"
#include "vikutils.h"
#include "dialog.h"
#include "globals.h"
#include "vikdatetime_edit_dialog.h"
#include "ui_util.h"
#endif




using namespace SlavGPS;




/**
 *  Update the display for time fields.
 */
void PropertiesDialogTP::update_times(Trackpoint * tp)
{
#ifdef K
	if (tp->has_timestamp) {
		this->timestamp->setValue(tp->timestamp);
		char * msg = vu_get_time_string(&(tp->timestamp), "%c", &(tp->coord), NULL);
		gtk_button_set_label(GTK_BUTTON(tpwin->time), msg);
		free(msg);
	} else {
		this->timestamp->setValue(0);
		gtk_button_set_label(GTK_BUTTON(tpwin->time), "");
	}
#endif
}




void PropertiesDialogTP::sync_ll_to_tp_cb(void) /* Slot. */
{
	if (this->cur_tp && (!this->sync_to_tp_block)) {
		struct LatLon ll;
		VikCoord coord;
		ll.lat = this->lat->value();
		ll.lon = this->lon->value();
		vik_coord_load_from_latlon(&coord, this->cur_tp->coord.mode, &ll);

		/* Don't redraw unless we really have to. */
		if (vik_coord_diff(&this->cur_tp->coord, &coord) > 0.05) { /* May not be exact due to rounding. */
			this->cur_tp->coord = coord;
#ifdef K
			gtk_dialog_response(GTK_DIALOG(tpwin), VIK_TRW_LAYER_TPWIN_DATA_CHANGED);
#endif
		}
	}
}




void PropertiesDialogTP::sync_alt_to_tp_cb(void) /* Slot. */
{
	if (this->cur_tp && (!this->sync_to_tp_block)) {
		/* Always store internally in metres. */
		HeightUnit height_units = a_vik_get_units_height();
		switch (height_units) {
		case HeightUnit::METRES:
			this->cur_tp->altitude = this->alt->value();
			break;
		case HeightUnit::FEET:
			this->cur_tp->altitude = VIK_FEET_TO_METERS(this->alt->value());
			break;
		default:
			this->cur_tp->altitude = this->alt->value();
			qDebug() << "EE: TrackPoint Properties: invalid height unit in" << __FUNCTION__;
		}
	}
}




void PropertiesDialogTP::sync_timestamp_to_tp_cb(void) /* Slot. */
{
	if (this->cur_tp && (!this->sync_to_tp_block)) {
		this->cur_tp->timestamp = this->timestamp->value();
		this->update_times(this->cur_tp);
	}
}



#ifdef K



static time_t last_edit_time = 0;

void PropertiesDialogTP::sync_time_to_tp_cb(GtkWidget * widget, QMouseEvent * event)
{
	if (!this->cur_tp || this->sync_to_tp_block) {
		return;
	}

	if (event->button() == Qt::RightButton) {
		/* On right click and when a time is available, allow a method to copy the displayed time as text. */
		if (!gtk_button_get_image(GTK_BUTTON(widget))) {
			vu_copy_label_menu(widget, event->button());
		}
		return;
	} else if (event->button() == Qt::MiddleButton) {
		return;
	}

	if (!this->cur_tp || this->sync_to_tp_block) {
		return;
	}

	if (this->cur_tp->has_timestamp) {
		last_edit_time = this->cur_tp->timestamp;
	} else if (last_edit_time == 0) {
		time(&last_edit_time);
	}

	GTimeZone * gtz = g_time_zone_new_local();
	time_t mytime = vik_datetime_edit_dialog(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(&this->parent))),
						 _("Date/Time Edit"),
						 last_edit_time,
						 gtz);
	g_time_zone_unref(gtz);

	/* Was the dialog cancelled? */
	if (mytime == 0) {
		return;
	}

	/* Otherwise use the new value. */
	this->cur_tp->timestamp = mytime;
	this->cur_tp->has_timestamp = true;
	/* TODO: consider warning about unsorted times? */

	/* Clear the previous 'Add' image as now a time is set. */
	if (gtk_button_get_image (GTK_BUTTON(this->time))) {
		gtk_button_set_image(GTK_BUTTON(this->time), NULL);
	}

	this->update_times(this->cur_tp);
}


#endif


bool PropertiesDialogTP::set_name_cb(void) /* Slot. */
{
	if (this->cur_tp && (!this->sync_to_tp_block)) {
		this->cur_tp->set_name(this->trkpt_name->text().toUtf8().constData());
	}
	return false;
}


#ifdef K


VikTrwLayerTpwin * vik_trw_layer_tpwin_new(GtkWindow * parent)
{
	tpwin->button_close = gtk_dialog_add_button (GTK_DIALOG(tpwin), GTK_STOCK_CLOSE, VIK_TRW_LAYER_TPWIN_CLOSE);
	tpwin->button_insert = gtk_dialog_add_button (GTK_DIALOG(tpwin), _("_Insert After"), VIK_TRW_LAYER_TPWIN_INSERT);
	tpwin->button_delete = gtk_dialog_add_button (GTK_DIALOG(tpwin), GTK_STOCK_DELETE, VIK_TRW_LAYER_TPWIN_DELETE);
	tpwin->button_split = gtk_dialog_add_button (GTK_DIALOG(tpwin), _("Split Here"), VIK_TRW_LAYER_TPWIN_SPLIT);
	tpwin->button_back = gtk_dialog_add_button (GTK_DIALOG(tpwin), GTK_STOCK_GO_BACK, VIK_TRW_LAYER_TPWIN_BACK);
	tpwin->button_forward = gtk_dialog_add_button (GTK_DIALOG(tpwin), GTK_STOCK_GO_FORWARD, VIK_TRW_LAYER_TPWIN_FORWARD);


	tpwin->time = gtk_button_new();


	GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
	response_w = gtk_dialog_get_widget_for_response(GTK_DIALOG(tpwin), VIK_TRW_LAYER_TPWIN_CLOSE);
#endif
	if (response_w) {
		gtk_widget_grab_focus(response_w);
	}

	return tpwin;
}


#endif


void PropertiesDialogTP::set_empty()
{
	/* TODO: shouldn't we set ->cur_tp to NULL? */
	this->trkpt_name->insert("");
	this->trkpt_name->setEnabled(false);

#ifdef K
	gtk_button_set_label(GTK_BUTTON(tpwin->time), "");
#endif

	this->course->setText(QString(""));

	this->lat->setEnabled(false);
	this->lon->setEnabled(false);
	this->alt->setEnabled(false);
	this->timestamp->setEnabled(false);
#ifdef K
	this->time->setEnabled(false);
#endif

	/* Only keep close button enabled. */
	this->button_insert_after->setEnabled(false);
	this->button_split_here->setEnabled(false);
	this->button_delete->setEnabled(false);
	this->button_back->setEnabled(false);
	this->button_forward->setEnabled(false);


	this->diff_dist->setText(QString(""));
	this->diff_time->setText(QString(""));
	this->diff_speed->setText(QString(""));
	this->speed->setText(QString(""));
	this->vdop->setText(QString(""));
	this->hdop->setText(QString(""));
	this->pdop->setText(QString(""));
	this->sat->setText(QString(""));

	this->setWindowTitle(QString("Trackpoint"));
}




/**
 * @tpwin:      The Trackpoint Edit Window
 * @track:      A Track
 * @iter:       Iterator to given Track
 * @track_name: The name of the track in which the trackpoint belongs
 * @is_route:   Is the track of the trackpoint actually a route?
 *
 * Sets the Trackpoint Edit Window to the values of the current trackpoint given in @tpl.
 */
void PropertiesDialogTP::set_tp(Track * track, TrackPoints::iterator * iter, const char * track_name, bool is_route)
{
	static char tmp_str[64];
	static struct LatLon ll;
	Trackpoint * tp = **iter;

	this->trkpt_name->setEnabled(true);
	if (tp->name) {
		this->trkpt_name->insert(QString(tp->name));
	} else {
		this->trkpt_name->insert(QString(""));
	}

	/* User can insert only if not at the end of track (otherwise use extend track). */
	this->button_insert_after->setEnabled(std::next(*iter) != track->end());
	this->button_delete->setEnabled(true);

	/* We can only split up a track if it's not an endpoint. */
	this->button_split_here->setEnabled(std::next(*iter) != track->end() && *iter != track->begin());

	this->button_forward->setEnabled(std::next(*iter) != track->end());
	this->button_back->setEnabled(*iter != track->begin());


	this->lat->setEnabled(true);
	this->lon->setEnabled(true);
	this->alt->setEnabled(true);
	this->timestamp->setEnabled(tp->has_timestamp);
#ifdef K
	this->time->setEnabled(tp->has_timestamp);
	/* Enable adding timestamps - but not on routepoints. */
	if (!tp->has_timestamp && !is_route) {
		this->time->setEnabled(true);
		GtkWidget *img = gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU);
		gtk_button_set_image(GTK_BUTTON(tpwin->time), img);
	} else {
		this->set_track_name(track_name);
	}
#endif

	this->sync_to_tp_block = true; /* Don't update while setting data. */

	vik_coord_to_latlon(&tp->coord, &ll);
	this->lat->setValue(ll.lat);
	this->lon->setValue(ll.lon);


	HeightUnit height_units = a_vik_get_units_height();
	switch (height_units) {
	case HeightUnit::METRES:
		this->alt->setValue(tp->altitude);
		break;
	case HeightUnit::FEET:
		this->alt->setValue(VIK_METERS_TO_FEET(tp->altitude));
		break;
	default:
		this->alt->setValue(tp->altitude);
		fprintf(stderr, "CRITICAL: Houston, we've had a problem. height=%d\n", height_units);
	}

	this->update_times(tp);

	this->sync_to_tp_block = false; /* Don't update while setting data. */


	SpeedUnit speed_units = a_vik_get_units_speed();
	DistanceUnit distance_unit = a_vik_get_units_distance();
	if (this->cur_tp) {
		switch (distance_unit) {
		case DistanceUnit::KILOMETRES:
			snprintf(tmp_str, sizeof (tmp_str), "%.2f m", vik_coord_diff(&(tp->coord), &(this->cur_tp->coord)));
			break;
		case DistanceUnit::MILES:
		case DistanceUnit::NAUTICAL_MILES:
			snprintf(tmp_str, sizeof (tmp_str), "%.2f yards", vik_coord_diff(&(tp->coord), &(this->cur_tp->coord))*1.0936133);
			break;
		default:
			fprintf(stderr, "CRITICAL: invalid distance unit %d\n", distance_unit);
		}

		this->diff_dist->setText(QString(tmp_str));
		if (tp->has_timestamp && this->cur_tp->has_timestamp) {
			snprintf(tmp_str, sizeof (tmp_str), "%ld s", tp->timestamp - this->cur_tp->timestamp);
			this->diff_time->setText(QString(tmp_str));
			if (tp->timestamp == this->cur_tp->timestamp) {
				this->diff_speed->setText(QString("--"));
			} else {
				double tmp_speed = vik_coord_diff(&tp->coord, &this->cur_tp->coord) / (ABS(tp->timestamp - this->cur_tp->timestamp));
#ifdef K
				get_speed_string(tmp_str, sizeof (tmp_str), speed_units, tmp_speed);
#endif
				this->diff_speed->setText(QString(tmp_str));
			}
		} else {
			this->diff_time->setText(QString(""));
			this->diff_speed->setText(QString(""));
		}
	}

	if (isnan(tp->course)) {
		snprintf(tmp_str, sizeof (tmp_str), "--");
	} else {
		snprintf(tmp_str, sizeof (tmp_str), "%05.1f\302\260", tp->course);
	}
	this->course->setText(QString(tmp_str));

	if (isnan(tp->speed)) {
		snprintf(tmp_str, sizeof (tmp_str), "--");
	} else {
#ifdef K
		get_speed_string(tmp_str, sizeof (tmp_str), speed_units, tp->speed);
#endif
	}
	this->speed->setText(QString(tmp_str));

	switch (distance_unit) {
	case DistanceUnit::KILOMETRES:
		snprintf(tmp_str, sizeof (tmp_str), "%.5f m", tp->hdop);
		this->hdop->setText(QString(tmp_str));
		snprintf(tmp_str, sizeof (tmp_str), "%.5f m", tp->pdop);
		this->pdop->setText(QString(tmp_str));
		break;
	case DistanceUnit::MILES:
		snprintf(tmp_str, sizeof (tmp_str), "%.5f yards", tp->hdop*1.0936133);
		this->hdop->setText(QString(tmp_str));
		snprintf(tmp_str, sizeof (tmp_str), "%.5f yards", tp->pdop*1.0936133);
		this->pdop->setText(QString(tmp_str));
		break;
	default: /* kamilTODO: where NM are handled? */
		fprintf(stderr, "CRITICAL: invalid distance unit %d\n", distance_unit);
	}

	switch (height_units) {
	case HeightUnit::METRES:
		snprintf(tmp_str, sizeof (tmp_str), "%.5f m", tp->vdop);
		break;
	case HeightUnit::FEET:
		snprintf(tmp_str, sizeof (tmp_str), "%.5f feet", VIK_METERS_TO_FEET(tp->vdop));
		break;
	default:
		snprintf(tmp_str, sizeof (tmp_str), "--");
		fprintf(stderr, "CRITICAL: Houston, we've had a problem. height=%d\n", height_units);
	}
	this->vdop->setText(QString(tmp_str));



	snprintf(tmp_str, sizeof (tmp_str), "%d / %d", tp->nsats, tp->fix_mode);
	this->sat->setText(QString(tmp_str));

	this->cur_tp = tp;
}




void PropertiesDialogTP::set_track_name(char const * track_name)
{
	QString new_name = QString("%1: %2").arg(QString(track_name)).arg(QString("Trackpoint"));
	this->setWindowTitle(new_name);
	//this->track_name->setText(QString(track_name));
}




PropertiesDialogTP::PropertiesDialogTP()
{
}




PropertiesDialogTP::PropertiesDialogTP(QWidget * parent) : QDialog(parent)
{
	this->setWindowTitle(QString("Trackpoint"));

	this->button_box = new QDialogButtonBox();

	this->button_close = this->button_box->addButton("&Close", QDialogButtonBox::ActionRole);
	connect(this->button_close, SIGNAL (released()), this, SLOT (reject()));
	this->button_insert_after = this->button_box->addButton("&Insert After", QDialogButtonBox::ActionRole);
	this->button_insert_after->setIcon(QIcon::fromTheme("list-add"));
	this->button_delete = this->button_box->addButton("&Delete", QDialogButtonBox::ActionRole);
	this->button_delete->setIcon(QIcon::fromTheme("list-delete"));
	this->button_split_here = this->button_box->addButton("Split Here", QDialogButtonBox::ActionRole);
	this->button_back = this->button_box->addButton("&Back", QDialogButtonBox::ActionRole);
	this->button_back->setIcon(QIcon::fromTheme("go-previous"));
	this->button_forward = this->button_box->addButton("&Forward", QDialogButtonBox::ActionRole);
	this->button_forward->setIcon(QIcon::fromTheme("go-next"));

	this->vbox = new QVBoxLayout; /* Main track info. */
	this->hbox = new QHBoxLayout; /* Diff info. */

	QFormLayout * left_form = NULL;
	QFormLayout * right_form = NULL;
	QLayout * old_layout = NULL;

	left_form = new QFormLayout();
	this->left_area = new QWidget();
	old_layout = this->left_area->layout();
	delete old_layout;
	this->left_area->setLayout(left_form);

	right_form = new QFormLayout();
	this->right_area = new QWidget();
	old_layout = this->right_area->layout();
	delete old_layout;
	this->right_area->setLayout(right_form);


	this->hbox->addWidget(this->left_area);
	this->hbox->addWidget(this->right_area);
	this->vbox->addLayout(this->hbox);
	this->vbox->addWidget(this->button_box);


	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);


	this->trkpt_name = new QLineEdit("", this);
	left_form->addRow(QString("Name:"), this->trkpt_name);
	// connect(this->trkpt_name, "focus-out-event", this, SLOT (set_name_cb(void)));



	this->lat = new QDoubleSpinBox(this);
	this->lat->setDecimals(6);
	this->lat->setMinimum(-90);
	this->lat->setMaximum(90);
	this->lat->setSingleStep(0.00005);
	this->lat->setValue(0);
	left_form->addRow(QString("Latitude:"), this->lat);
	connect(this->lat, SIGNAL (valueChanged(double)), this, SLOT (sync_ll_to_tp_cb(void)));


	this->lon = new QDoubleSpinBox(this);
	this->lon->setDecimals(0);
	this->lon->setMinimum(-180);
	this->lon->setMaximum(180);
	this->lon->setSingleStep(0.00005);
	this->lon->setValue(0);
	left_form->addRow(QString("Latitude:"), this->lon);
	connect(this->lon, SIGNAL (valueChanged(double)), this, SLOT (sync_ll_to_tp_cb(void)));


	this->alt = new QDoubleSpinBox(this);
	this->alt->setDecimals(2);
	this->alt->setMinimum(VIK_VAL_MIN_ALT);
	this->alt->setMaximum(VIK_VAL_MAX_ALT);
	this->alt->setSingleStep(10);
	this->alt->setValue(0);
	left_form->addRow(QString("Altitude:"), this->alt);
	connect(this->alt, SIGNAL (valueChanged(double)), this, SLOT (sync_alt_to_tp_cb(void)));


	this->course = new QLabel("", this);
	this->course->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	left_form->addRow(QString("Course:"), this->course);


	this->timestamp = new QSpinBox(this);
	this->timestamp->setMinimum(0);
	this->timestamp->setMaximum(2147483647); /* pow(2,31)-1 limit input to ~2038 for now. */ /* TODO: improve this initialization. */
	this->timestamp->setSingleStep(1);
	left_form->addRow(QString("Timestamp:"), this->timestamp);
	connect(this->timestamp, SIGNAL (valueChanged(int)), this, SLOT (sync_timestamp_to_tp_cb(void)));


	left_form->addRow(QString("Time:"), (QWidget *) NULL);
	//gtk_button_set_relief (GTK_BUTTON(tpwin->time), GTK_RELIEF_NONE);
	//connect(this->time, "button-release-event", this, SLOT (sync_time_to_tp_cb(void)));



	this->diff_dist = new QLabel("", this);
	this->diff_dist->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	right_form->addRow(QString("Distance Difference:"), this->diff_dist);


	this->diff_time = new QLabel("", this);
	this->diff_time->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	right_form->addRow(QString("Time Difference:"), this->diff_time);


	this->diff_speed = new QLabel("", this);
	this->diff_speed->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	right_form->addRow(QString("\"Speed\" Between:"), this->diff_speed);


	this->speed = new QLabel("", this);
	this->speed->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	right_form->addRow(QString("Speed:"), this->speed);


	this->vdop = new QLabel("", this);
	this->vdop->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	right_form->addRow(QString("VDOP:"), this->vdop);


	this->hdop = new QLabel("", this);
	this->hdop->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	right_form->addRow(QString("HDOP:"), this->hdop);


	this->pdop = new QLabel("", this);
	this->pdop->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	right_form->addRow(QString("PDOP:"), this->pdop);


	this->sat = new QLabel("", this);
	this->sat->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	right_form->addRow(QString("SAT/FIX:"), this->sat);
}



PropertiesDialogTP::~PropertiesDialogTP()
{
}
