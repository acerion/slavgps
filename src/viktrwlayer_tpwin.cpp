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

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <time.h>

#include "viking.h"
#include "coords.h"
#include "coord.h"
#include "viktrack.h"
#include "viktrwlayer_tpwin.h"
#include "vikwaypoint.h"
#include "vikutils.h"
#include "dialog.h"
#include "globals.h"
#include "vikdatetime_edit_dialog.h"
#include "ui_util.h"




using namespace SlavGPS;




struct _VikTrwLayerTpwin {
	GtkDialog parent;
	GtkSpinButton *lat, *lon, *alt, *ts;
	GtkWidget *trkpt_name;
	GtkWidget *time;
	GtkLabel *course, *diff_dist, *diff_time, *diff_speed, *speed, *hdop, *vdop, *pdop, *sat;
	/* Previously these buttons were in a glist, however I think the ordering behaviour is implicit
	   thus control manually to ensure operating on the correct button. */
	GtkWidget * button_close;
	GtkWidget * button_delete;
	GtkWidget * button_insert;
	GtkWidget * button_split;
	GtkWidget * button_back;
	GtkWidget * button_forward;
	Trackpoint * cur_tp;
	bool sync_to_tp_block;
};




GType vik_trw_layer_tpwin_get_type(void)
{
	static GType tpwin_type = 0;

	if (!tpwin_type) {
		static const GTypeInfo tpwin_info = {
			sizeof (VikTrwLayerTpwinClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			NULL, /* class init */
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (VikTrwLayerTpwin),
			0,
			NULL /* instance init */
		};
		tpwin_type = g_type_register_static(GTK_TYPE_DIALOG, "VikTrwLayerTpwin", &tpwin_info, (GTypeFlags) 0);
	}

	return tpwin_type;
}




/**
 *  Just update the display for the time fields.
 */
static void tpwin_update_times(VikTrwLayerTpwin * tpwin, Trackpoint * tp)
{
	if (tp->has_timestamp) {
		gtk_spin_button_set_value(tpwin->ts, tp->timestamp);
		char * msg = vu_get_time_string(&(tp->timestamp), "%c", &(tp->coord), NULL);
		gtk_button_set_label(GTK_BUTTON(tpwin->time), msg);
		free(msg);
	} else {
		gtk_spin_button_set_value(tpwin->ts, 0);
		gtk_button_set_label(GTK_BUTTON(tpwin->time), "");
	}
}




static void tpwin_sync_ll_to_tp(VikTrwLayerTpwin * tpwin)
{
	if (tpwin->cur_tp && (!tpwin->sync_to_tp_block)) {
		struct LatLon ll;
		VikCoord coord;
		ll.lat = gtk_spin_button_get_value(tpwin->lat);
		ll.lon = gtk_spin_button_get_value(tpwin->lon);
		vik_coord_load_from_latlon(&coord, tpwin->cur_tp->coord.mode, &ll);

		/* Don't redraw unless we really have to. */
		if (vik_coord_diff(&(tpwin->cur_tp->coord), &coord) > 0.05) { /* May not be exact due to rounding. */
			tpwin->cur_tp->coord = coord;
			gtk_dialog_response(GTK_DIALOG(tpwin), VIK_TRW_LAYER_TPWIN_DATA_CHANGED);
		}
	}
}




static void tpwin_sync_alt_to_tp(VikTrwLayerTpwin * tpwin)
{
	if (tpwin->cur_tp && (!tpwin->sync_to_tp_block)) {
		/* Always store internally in metres. */
		HeightUnit height_units = a_vik_get_units_height();
		switch (height_units) {
		case HeightUnit::METRES:
			tpwin->cur_tp->altitude = gtk_spin_button_get_value(tpwin->alt);
			break;
		case HeightUnit::FEET:
			tpwin->cur_tp->altitude = VIK_FEET_TO_METERS(gtk_spin_button_get_value(tpwin->alt));
			break;
		default:
			tpwin->cur_tp->altitude = gtk_spin_button_get_value(tpwin->alt);
			fprintf(stderr, "CRITICAL: Houston, we've had a problem. height=%d\n", height_units);
		}
	}
}




static void tpwin_sync_ts_to_tp(VikTrwLayerTpwin * tpwin)
{
	if (tpwin->cur_tp && (!tpwin->sync_to_tp_block)) {
		tpwin->cur_tp->timestamp = gtk_spin_button_get_value_as_int(tpwin->ts);

		tpwin_update_times(tpwin, tpwin->cur_tp);
	}
}




static time_t last_edit_time = 0;

static void tpwin_sync_time_to_tp(GtkWidget * widget, GdkEventButton * event, VikTrwLayerTpwin * tpwin)
{
	if (!tpwin->cur_tp || tpwin->sync_to_tp_block) {
		return;
	}

	if (event->button == MouseButton::RIGHT) {
		/* On right click and when a time is available, allow a method to copy the displayed time as text. */
		if (!gtk_button_get_image(GTK_BUTTON(widget))) {
			vu_copy_label_menu(widget, event->button);
		}
		return;
	} else if (event->button == MouseButton::MIDDLE) {
		return;
	}

	if (!tpwin->cur_tp || tpwin->sync_to_tp_block) {
		return;
	}

	if (tpwin->cur_tp->has_timestamp) {
		last_edit_time = tpwin->cur_tp->timestamp;
	} else if (last_edit_time == 0) {
		time(&last_edit_time);
	}

	GTimeZone * gtz = g_time_zone_new_local();
	time_t mytime = vik_datetime_edit_dialog(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(&tpwin->parent))),
						 _("Date/Time Edit"),
						 last_edit_time,
						 gtz);
	g_time_zone_unref(gtz);

	/* Was the dialog cancelled? */
	if (mytime == 0) {
		return;
	}

	/* Otherwise use the new value. */
	tpwin->cur_tp->timestamp = mytime;
	tpwin->cur_tp->has_timestamp = true;
	/* TODO: consider warning about unsorted times? */

	/* Clear the previous 'Add' image as now a time is set. */
	if (gtk_button_get_image (GTK_BUTTON(tpwin->time))) {
		gtk_button_set_image(GTK_BUTTON(tpwin->time), NULL);
	}

	tpwin_update_times(tpwin, tpwin->cur_tp);
}




static bool tpwin_set_name(VikTrwLayerTpwin * tpwin)
{
	if (tpwin->cur_tp && (!tpwin->sync_to_tp_block)) {
		tpwin->cur_tp->set_name(gtk_entry_get_text(GTK_ENTRY(tpwin->trkpt_name)));
	}
	return false;
}




VikTrwLayerTpwin * vik_trw_layer_tpwin_new(GtkWindow * parent)
{
	static char * left_label_texts[] = { (char *) N_("<b>Name:</b>"),
					     (char *) N_("<b>Latitude:</b>"),
					     (char *) N_("<b>Longitude:</b>"),
					     (char *) N_("<b>Altitude:</b>"),
					     (char *) N_("<b>Course:</b>"),
					     (char *) N_("<b>Timestamp:</b>"),
					     (char *) N_("<b>Time:</b>") };
	static char * right_label_texts[] = { (char *) N_("<b>Distance Difference:</b>"),
					      (char *) N_("<b>Time Difference:</b>"),
					      (char *) N_("<b>\"Speed\" Between:</b>"),
					      (char *) N_("<b>Speed:</b>"),
					      (char *) N_("<b>VDOP:</b>"),
					      (char *) N_("<b>HDOP:</b>"),
					      (char *) N_("<b>PDOP:</b>"),
					      (char *) N_("<b>SAT/FIX:</b>") };

	VikTrwLayerTpwin *tpwin = VIK_TRW_LAYER_TPWIN (g_object_new (VIK_TRW_LAYER_TPWIN_TYPE, NULL));
	GtkWidget *main_hbox, *left_vbox, *right_vbox;
	GtkWidget *diff_left_vbox, *diff_right_vbox;

	gtk_window_set_transient_for (GTK_WINDOW(tpwin), parent);
	gtk_window_set_title (GTK_WINDOW(tpwin), _("Trackpoint"));

	tpwin->button_close = gtk_dialog_add_button (GTK_DIALOG(tpwin), GTK_STOCK_CLOSE, VIK_TRW_LAYER_TPWIN_CLOSE);
	tpwin->button_insert = gtk_dialog_add_button (GTK_DIALOG(tpwin), _("_Insert After"), VIK_TRW_LAYER_TPWIN_INSERT);
	tpwin->button_delete = gtk_dialog_add_button (GTK_DIALOG(tpwin), GTK_STOCK_DELETE, VIK_TRW_LAYER_TPWIN_DELETE);
	tpwin->button_split = gtk_dialog_add_button (GTK_DIALOG(tpwin), _("Split Here"), VIK_TRW_LAYER_TPWIN_SPLIT);
	tpwin->button_back = gtk_dialog_add_button (GTK_DIALOG(tpwin), GTK_STOCK_GO_BACK, VIK_TRW_LAYER_TPWIN_BACK);
	tpwin->button_forward = gtk_dialog_add_button (GTK_DIALOG(tpwin), GTK_STOCK_GO_FORWARD, VIK_TRW_LAYER_TPWIN_FORWARD);

	/*
	  gtk_dialog_add_buttons (GTK_DIALOG(tpwin),
	  GTK_STOCK_CLOSE, VIK_TRW_LAYER_TPWIN_CLOSE,
	  _("_Insert After"), VIK_TRW_LAYER_TPWIN_INSERT,
	  GTK_STOCK_DELETE, VIK_TRW_LAYER_TPWIN_DELETE,
	  _("Split Here"), VIK_TRW_LAYER_TPWIN_SPLIT,
	  GTK_STOCK_GO_BACK, VIK_TRW_LAYER_TPWIN_BACK,
	  GTK_STOCK_GO_FORWARD, VIK_TRW_LAYER_TPWIN_FORWARD,
	  NULL);
	  tpwin->buttons = gtk_container_get_children(GTK_CONTAINER(GTK_DIALOG(tpwin)->action_area));
	*/

	/* Main track info. */
	left_vbox = a_dialog_create_label_vbox (left_label_texts, G_N_ELEMENTS(left_label_texts), 1, 3);

	tpwin->trkpt_name = gtk_entry_new();
	g_signal_connect_swapped (G_OBJECT(tpwin->trkpt_name), "focus-out-event", G_CALLBACK(tpwin_set_name), tpwin);

	tpwin->course = GTK_LABEL(ui_label_new_selectable(NULL));
	tpwin->time = gtk_button_new();
	gtk_button_set_relief (GTK_BUTTON(tpwin->time), GTK_RELIEF_NONE);
	g_signal_connect (G_OBJECT(tpwin->time), "button-release-event", G_CALLBACK(tpwin_sync_time_to_tp), tpwin);

	tpwin->lat = GTK_SPIN_BUTTON(gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, -90, 90, 0.00005, 0.01, 0)), 0.00005, 6));
	tpwin->lon = GTK_SPIN_BUTTON(gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, -180, 180, 0.00005, 0.01, 0)), 0.00005, 6));

	g_signal_connect_swapped (G_OBJECT(tpwin->lat), "value-changed", G_CALLBACK(tpwin_sync_ll_to_tp), tpwin);
	g_signal_connect_swapped (G_OBJECT(tpwin->lon), "value-changed", G_CALLBACK(tpwin_sync_ll_to_tp), tpwin);

	tpwin->alt = GTK_SPIN_BUTTON(gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new (0, (int) VIK_VAL_MAX_ALT, (int) VIK_VAL_MIN_ALT, 10, 100, 0)), 10, 2));

	g_signal_connect_swapped (G_OBJECT(tpwin->alt), "value-changed", G_CALLBACK(tpwin_sync_alt_to_tp), tpwin);

	tpwin->ts = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0,2147483647,1)); /* pow(2,31)-1 limit input to ~2038 for now. */
	g_signal_connect_swapped (G_OBJECT(tpwin->ts), "value-changed", G_CALLBACK(tpwin_sync_ts_to_tp), tpwin);
	gtk_spin_button_set_digits (tpwin->ts, 0);

	right_vbox = gtk_vbox_new (true, 1);
	gtk_box_pack_start (GTK_BOX(right_vbox), GTK_WIDGET(tpwin->trkpt_name), false, false, 3);
	gtk_box_pack_start (GTK_BOX(right_vbox), GTK_WIDGET(tpwin->lat), false, false, 3);
	gtk_box_pack_start (GTK_BOX(right_vbox), GTK_WIDGET(tpwin->lon), false, false, 3);
	gtk_box_pack_start (GTK_BOX(right_vbox), GTK_WIDGET(tpwin->alt), false, false, 3);
	gtk_box_pack_start (GTK_BOX(right_vbox), GTK_WIDGET(tpwin->course), false, false, 3);
	gtk_box_pack_start (GTK_BOX(right_vbox), GTK_WIDGET(tpwin->ts), false, false, 3);
	gtk_box_pack_start (GTK_BOX(right_vbox), GTK_WIDGET(tpwin->time), false, false, 3);

	/* Diff info. */
	diff_left_vbox = a_dialog_create_label_vbox (right_label_texts, G_N_ELEMENTS(right_label_texts), 1, 3);

	tpwin->diff_dist = GTK_LABEL(ui_label_new_selectable(NULL));
	tpwin->diff_time = GTK_LABEL(ui_label_new_selectable(NULL));
	tpwin->diff_speed = GTK_LABEL(ui_label_new_selectable(NULL));
	tpwin->speed = GTK_LABEL(ui_label_new_selectable(NULL));

	tpwin->vdop = GTK_LABEL(ui_label_new_selectable(NULL));
	tpwin->hdop = GTK_LABEL(ui_label_new_selectable(NULL));
	tpwin->pdop = GTK_LABEL(ui_label_new_selectable(NULL));
	tpwin->sat = GTK_LABEL(ui_label_new_selectable(NULL));

	diff_right_vbox = gtk_vbox_new (true, 1);
	gtk_box_pack_start (GTK_BOX(diff_right_vbox), GTK_WIDGET(tpwin->diff_dist), false, false, 3);
	gtk_box_pack_start (GTK_BOX(diff_right_vbox), GTK_WIDGET(tpwin->diff_time), false, false, 3);
	gtk_box_pack_start (GTK_BOX(diff_right_vbox), GTK_WIDGET(tpwin->diff_speed), false, false, 3);
	gtk_box_pack_start (GTK_BOX(diff_right_vbox), GTK_WIDGET(tpwin->speed), false, false, 3);

	gtk_box_pack_start (GTK_BOX(diff_right_vbox), GTK_WIDGET(tpwin->vdop), false, false, 3);
	gtk_box_pack_start (GTK_BOX(diff_right_vbox), GTK_WIDGET(tpwin->hdop), false, false, 3);
	gtk_box_pack_start (GTK_BOX(diff_right_vbox), GTK_WIDGET(tpwin->pdop), false, false, 3);
	gtk_box_pack_start (GTK_BOX(diff_right_vbox), GTK_WIDGET(tpwin->sat), false, false, 3);

	main_hbox = gtk_hbox_new(false, 0);
	gtk_box_pack_start (GTK_BOX(main_hbox), left_vbox, true, true, 3);
	gtk_box_pack_start (GTK_BOX(main_hbox), right_vbox, true, true, 0);
	gtk_box_pack_start (GTK_BOX(main_hbox), diff_left_vbox, true, true, 0);
	gtk_box_pack_start (GTK_BOX(main_hbox), diff_right_vbox, true, true, 0);

	gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(tpwin))), main_hbox, false, false, 0);

	tpwin->cur_tp = NULL;

	GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
	response_w = gtk_dialog_get_widget_for_response(GTK_DIALOG(tpwin), VIK_TRW_LAYER_TPWIN_CLOSE);
#endif
	if (response_w) {
		gtk_widget_grab_focus(response_w);
	}

	return tpwin;
}




void vik_trw_layer_tpwin_set_empty(VikTrwLayerTpwin * tpwin)
{
	gtk_editable_delete_text(GTK_EDITABLE(tpwin->trkpt_name), 0, -1);
	gtk_widget_set_sensitive(tpwin->trkpt_name, false);

	gtk_button_set_label(GTK_BUTTON(tpwin->time), "");
	gtk_label_set_text(tpwin->course, NULL);

	gtk_widget_set_sensitive(GTK_WIDGET(tpwin->lat), false);
	gtk_widget_set_sensitive(GTK_WIDGET(tpwin->lon), false);
	gtk_widget_set_sensitive(GTK_WIDGET(tpwin->alt), false);
	gtk_widget_set_sensitive(GTK_WIDGET(tpwin->ts), false);
	gtk_widget_set_sensitive(GTK_WIDGET(tpwin->time), false);

	/* Only keep close button enabled. */
	gtk_widget_set_sensitive(tpwin->button_insert, false);
	gtk_widget_set_sensitive(tpwin->button_split, false);
	gtk_widget_set_sensitive(tpwin->button_delete, false);
	gtk_widget_set_sensitive(tpwin->button_back, false);
	gtk_widget_set_sensitive(tpwin->button_forward, false);

	gtk_label_set_text(tpwin->diff_dist, NULL);
	gtk_label_set_text(tpwin->diff_time, NULL);
	gtk_label_set_text(tpwin->diff_speed, NULL);
	gtk_label_set_text(tpwin->speed, NULL);
	gtk_label_set_text(tpwin->vdop, NULL);
	gtk_label_set_text(tpwin->hdop, NULL);
	gtk_label_set_text(tpwin->pdop, NULL);
	gtk_label_set_text(tpwin->sat, NULL);

	gtk_window_set_title(GTK_WINDOW(tpwin), _("Trackpoint"));
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
void vik_trw_layer_tpwin_set_tp(VikTrwLayerTpwin * tpwin, Track * track, TrackPoints::iterator * iter, const char * track_name, bool is_route)
{
	static char tmp_str[64];
	static struct LatLon ll;
	Trackpoint * tp = **iter;

	if (tp->name) {
		gtk_entry_set_text(GTK_ENTRY(tpwin->trkpt_name), tp->name);
	} else {
		gtk_editable_delete_text(GTK_EDITABLE(tpwin->trkpt_name), 0, -1);
	}
	gtk_widget_set_sensitive(tpwin->trkpt_name, true);

	/* Only can insert if not at the end (otherwise use extend track). */
	gtk_widget_set_sensitive(tpwin->button_insert, std::next(*iter) != track->end());
	gtk_widget_set_sensitive(tpwin->button_delete, true);

	/* We can only split up a track if it's not an endpoint. Makes sense to me. */
	gtk_widget_set_sensitive(tpwin->button_split, std::next(*iter) != track->end() && *iter != track->begin());

	gtk_widget_set_sensitive(tpwin->button_forward, std::next(*iter) != track->end());
	gtk_widget_set_sensitive(tpwin->button_back, *iter != track->begin());

	gtk_widget_set_sensitive(GTK_WIDGET(tpwin->lat), true);
	gtk_widget_set_sensitive(GTK_WIDGET(tpwin->lon), true);
	gtk_widget_set_sensitive(GTK_WIDGET(tpwin->alt), true);
	gtk_widget_set_sensitive(GTK_WIDGET(tpwin->ts), tp->has_timestamp);
	gtk_widget_set_sensitive(GTK_WIDGET(tpwin->time), tp->has_timestamp);
	/* Enable adding timestamps - but not on routepoints. */
	if (!tp->has_timestamp && !is_route) {
		gtk_widget_set_sensitive(GTK_WIDGET(tpwin->time), true);
		GtkWidget *img = gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU);
		gtk_button_set_image(GTK_BUTTON(tpwin->time), img);
	} else {
		vik_trw_layer_tpwin_set_track_name(tpwin, track_name);
	}

	tpwin->sync_to_tp_block = true; /* Don't update while setting data. */

	vik_coord_to_latlon(&(tp->coord), &ll);
	gtk_spin_button_set_value(tpwin->lat, ll.lat);
	gtk_spin_button_set_value(tpwin->lon, ll.lon);
	HeightUnit height_units = a_vik_get_units_height();
	switch (height_units) {
	case HeightUnit::METRES:
		gtk_spin_button_set_value(tpwin->alt, tp->altitude);
		break;
	case HeightUnit::FEET:
		gtk_spin_button_set_value(tpwin->alt, VIK_METERS_TO_FEET(tp->altitude));
		break;
	default:
		gtk_spin_button_set_value(tpwin->alt, tp->altitude);
		fprintf(stderr, "CRITICAL: Houston, we've had a problem. height=%d\n", height_units);
	}

	tpwin_update_times(tpwin, tp);

	tpwin->sync_to_tp_block = false; /* Don't update while setting data. */

	SpeedUnit speed_units = a_vik_get_units_speed();
	DistanceUnit distance_unit = a_vik_get_units_distance();
	if (tpwin->cur_tp) {
		switch (distance_unit) {
		case DistanceUnit::KILOMETRES:
			snprintf(tmp_str, sizeof(tmp_str), "%.2f m", vik_coord_diff(&(tp->coord), &(tpwin->cur_tp->coord)));
			break;
		case DistanceUnit::MILES:
		case DistanceUnit::NAUTICAL_MILES:
			snprintf(tmp_str, sizeof(tmp_str), "%.2f yards", vik_coord_diff(&(tp->coord), &(tpwin->cur_tp->coord))*1.0936133);
			break;
		default:
			fprintf(stderr, "CRITICAL: invalid distance unit %d\n", distance_unit);
		}

		gtk_label_set_text (tpwin->diff_dist, tmp_str);
		if (tp->has_timestamp && tpwin->cur_tp->has_timestamp) {
			snprintf(tmp_str, sizeof(tmp_str), "%ld s", tp->timestamp - tpwin->cur_tp->timestamp);
			gtk_label_set_text (tpwin->diff_time, tmp_str);
			if (tp->timestamp == tpwin->cur_tp->timestamp) {
				gtk_label_set_text (tpwin->diff_speed, "--");
			} else {
				double tmp_speed = vik_coord_diff(&tp->coord, &tpwin->cur_tp->coord) / (ABS(tp->timestamp - tpwin->cur_tp->timestamp));
				get_speed_string(tmp_str, sizeof (tmp_str), speed_units, tmp_speed);
				gtk_label_set_text(tpwin->diff_speed, tmp_str);
			}
		} else {
			gtk_label_set_text(tpwin->diff_time, NULL);
			gtk_label_set_text(tpwin->diff_speed, NULL);
		}
	}

	if (isnan(tp->course)) {
		snprintf(tmp_str, sizeof(tmp_str), "--");
	} else {
		snprintf(tmp_str, sizeof(tmp_str), "%05.1f\302\260", tp->course);
	}
	gtk_label_set_text(tpwin->course, tmp_str);

	if (isnan(tp->speed)) {
		snprintf(tmp_str, sizeof(tmp_str), "--");
	} else {
		get_speed_string(tmp_str, sizeof (tmp_str), speed_units, tp->speed);
	}
	gtk_label_set_text(tpwin->speed, tmp_str);

	switch (distance_unit) {
	case DistanceUnit::KILOMETRES:
		snprintf(tmp_str, sizeof(tmp_str), "%.5f m", tp->hdop);
		gtk_label_set_text(tpwin->hdop, tmp_str);
		snprintf(tmp_str, sizeof(tmp_str), "%.5f m", tp->pdop);
		gtk_label_set_text(tpwin->pdop, tmp_str);
		break;
	case DistanceUnit::MILES:
		snprintf(tmp_str, sizeof(tmp_str), "%.5f yards", tp->hdop*1.0936133);
		gtk_label_set_text(tpwin->hdop, tmp_str);
		snprintf(tmp_str, sizeof(tmp_str), "%.5f yards", tp->pdop*1.0936133);
		gtk_label_set_text(tpwin->pdop, tmp_str);
		break;
	default: /* kamilTODO: where NM are handled? */
		fprintf(stderr, "CRITICAL: invalid distance unit %d\n", distance_unit);
	}

	switch (height_units) {
	case HeightUnit::METRES:
		snprintf(tmp_str, sizeof(tmp_str), "%.5f m", tp->vdop);
		break;
	case HeightUnit::FEET:
		snprintf(tmp_str, sizeof(tmp_str), "%.5f feet", VIK_METERS_TO_FEET(tp->vdop));
		break;
	default:
		snprintf(tmp_str, sizeof(tmp_str), "--");
		fprintf(stderr, "CRITICAL: Houston, we've had a problem. height=%d\n", height_units);
	}
	gtk_label_set_text(tpwin->vdop, tmp_str);

	snprintf(tmp_str, sizeof(tmp_str), "%d / %d", tp->nsats, tp->fix_mode);
	gtk_label_set_text(tpwin->sat, tmp_str);

	tpwin->cur_tp = tp;
}

void vik_trw_layer_tpwin_set_track_name(VikTrwLayerTpwin * tpwin, const char * track_name)
{
	char * tmp_name = g_strdup_printf("%s: %s", track_name, _("Trackpoint"));
	gtk_window_set_title(GTK_WINDOW(tpwin), tmp_name);
	free(tmp_name);
	//gtk_label_set_text(tpwin->track_name, track_name);
}
