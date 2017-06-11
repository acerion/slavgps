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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cassert>

#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include <time.h>

#include "layer_trw_analysis.h"
#include "ui_util.h"
#include "settings.h"
#include "globals.h"
#include "track_statistics.h"
#include "vikutils.h"




using namespace SlavGPS;




/* Could use GtkGrids but that is Gtk3+ */
static GtkWidget * create_table(int cnt, char * labels[], GtkWidget * contents[])
{
#ifdef K
	GtkTable * table = GTK_TABLE(gtk_table_new (cnt, 2, false));
	gtk_table_set_col_spacing(table, 0, 10);
	for (int i = 0; i < cnt; i++) {
		QLabel * label = new QLabel("");
		gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5); /* Position text centrally in vertical plane. */
		/* All text labels are set to be in bold. */
		char * markup = g_markup_printf_escaped("<b>%s:</b>", _(labels[i]));
		gtk_label_set_markup(GTK_LABEL(label), markup);
		free(markup);
		gtk_table_attach(table, label, 0, 1, i, i+1, GTK_FILL, GTK_EXPAND, 4, 2);
		if (GTK_IS_MISC(contents[i])) {
			gtk_misc_set_alignment(GTK_MISC (contents[i]), 0, 0.5);
		}
		gtk_table_attach_defaults(table, contents[i], 1, 2, i, i+1);
	}
	return GTK_WIDGET (table);
#endif
}




static char * label_texts[] = {
	(char *) N_("Number of Tracks"),
	(char *) N_("Date Range"),
	(char *) N_("Total Length"),
	(char *) N_("Average Length"),
	(char *) N_("Max Speed"),
	(char *) N_("Avg. Speed"),
	(char *) N_("Minimum Altitude"),
	(char *) N_("Maximum Altitude"),
	(char *) N_("Total Elevation Gain/Loss"),
	(char *) N_("Avg. Elevation Gain/Loss"),
	(char *) N_("Total Duration"),
	(char *) N_("Avg. Duration"),
};




/**
 * Returns a widget to hold the stats information in a table grid layout.
 */
static GtkWidget * create_layout(GtkWidget * content[])
{
#ifdef K
	int cnt = 0;
	for (cnt = 0; cnt < G_N_ELEMENTS(label_texts); cnt++) {
		content[cnt] = ui_label_new_selectable(NULL);
	}

	return create_table(cnt, label_texts, content);
#endif
}




/**
 * Update the given widgets table with the values from the track stats.
 */
static void table_output(TrackStatistics& ts, GtkWidget * content[])
{
#ifdef K
	int cnt = 0;

	char tmp_buf[64];
	snprintf(tmp_buf, sizeof(tmp_buf), "%d", ts.count);
	content[cnt++]->setText(tmp_buf);

	if (ts.count == 0) {
		/* Blank all other fields. */
		snprintf(tmp_buf, sizeof(tmp_buf), "--");
		for (int cnt = 1; cnt < G_N_ELEMENTS(label_texts); cnt++) {
			content[cnt]->setText(tmp_buf);
		}
		return;
	}

	/* Check for potential date range. */
	/* Test if the same day by comparing the date string of the timestamp. */
	GDate* gdate_start = g_date_new();
	g_date_set_time_t (gdate_start, ts.start_time);
	char time_start[32];
	g_date_strftime(time_start, sizeof(time_start), "%x", gdate_start);
	g_date_free(gdate_start);

	GDate* gdate_end = g_date_new ();
	g_date_set_time_t (gdate_end, ts.end_time);
	char time_end[32];
	g_date_strftime(time_end, sizeof(time_end), "%x", gdate_end);
	g_date_free(gdate_end);


	if (ts.start_time == ts.end_time) {
		snprintf(tmp_buf, sizeof(tmp_buf), _("No Data"));
	} else if (strncmp(time_start, time_end, 32)) {
		snprintf(tmp_buf, sizeof(tmp_buf), "%s --> %s", time_start, time_end);
	} else {
		snprintf(tmp_buf, sizeof(tmp_buf), "%s", time_start);
	}
	content[cnt++]->setText(tmp_buf);

	get_distance_string(tmp_buf, sizeof (tmp_buf), Preferences::get_unit_distance(), ts.length);
	content[cnt++]->setText(tmp_buf);

	get_distance_string(tmp_buf, sizeof (tmp_buf), Preferences::get_unit_distance(), ts.length / ts.count);
	content[cnt++]->setText(tmp_buf);


	SpeedUnit speed_unit = Preferences::get_unit_speed();
	if (ts.max_speed > 0) {
		get_speed_string(tmp_buf, sizeof (tmp_buf), speed_unit, ts.max_speed);
	} else {
		snprintf(tmp_buf, sizeof (tmp_buf), "--");
	}
	content[cnt++]->setText(tmp_buf);


	if (ts.duration > 0) {
		get_speed_string(tmp_buf, sizeof (tmp_buf), speed_unit, ts.length / ts.duration);
	} else {
		snprintf(tmp_buf, sizeof (tmp_buf), "--");
	}
	content[cnt++]->setText(QObject::tr(tmp_buf);


	switch (Preferences::get_unit_height()) {
		/* Note always round off height value output since sub unit accuracy is overkill. */
	case HeightUnit::FEET:
		if (ts.min_alt != VIK_VAL_MIN_ALT) {
			snprintf(tmp_buf, sizeof(tmp_buf), _("%d feet"), (int)round(VIK_METERS_TO_FEET(ts.min_alt)));
		} else {
			snprintf(tmp_buf, sizeof(tmp_buf), "--");
		}
		content[cnt++]->setText(tmp_buf);
		fprintf(stderr, "%d: %s, cnt = %d\n", __LINE__, tmp_buf, cnt);

		if (ts.max_alt != VIK_VAL_MAX_ALT) {
			snprintf(tmp_buf, sizeof(tmp_buf), _("%d feet"), (int)round(VIK_METERS_TO_FEET(ts.max_alt)));
		} else {
			snprintf(tmp_buf, sizeof(tmp_buf), "--");
		}
		content[cnt++]->setText(tmp_buf);
		fprintf(stderr, "%d: %s, cnt = %d\n", __LINE__, tmp_buf, cnt);

		snprintf(tmp_buf, sizeof(tmp_buf), _("%d feet / %d feet"), (int)round(VIK_METERS_TO_FEET(ts.elev_gain)), (int)round(VIK_METERS_TO_FEET(ts.elev_loss)));
		content[cnt++]->setText(tmp_buf);
		fprintf(stderr, "%d: %s, cnt = %d\n", __LINE__, tmp_buf, cnt);
		snprintf(tmp_buf, sizeof(tmp_buf), _("%d feet / %d feet"), (int)round(VIK_METERS_TO_FEET(ts.elev_gain/ts.count)), (int)round(VIK_METERS_TO_FEET(ts.elev_loss/ts.count)));
		break;
	default:
		/* HeightUnit::METRES */
		if (ts.min_alt != VIK_VAL_MIN_ALT) {
			snprintf(tmp_buf, sizeof(tmp_buf), _("%d m"), (int)round(ts.min_alt));
		} else {
			snprintf(tmp_buf, sizeof(tmp_buf), "--");
		}
		content[cnt++]->setText(QObject::tr(tmp_buf);
		fprintf(stderr, "%d: %s, cnt = %d\n", __LINE__, tmp_buf, cnt);

		if (ts.max_alt != VIK_VAL_MAX_ALT) {
			snprintf(tmp_buf, sizeof(tmp_buf), _("%d m"), (int)round(ts.max_alt));
		} else {
			snprintf(tmp_buf, sizeof(tmp_buf), "--");
		}
		content[cnt++]->setText(tmp_buf);
		fprintf(stderr, "%d: %s, cnt = %d\n", __LINE__, tmp_buf, cnt);

		snprintf(tmp_buf, sizeof(tmp_buf), _("%d m / %d m"), (int)round(ts.elev_gain), (int)round(ts.elev_loss));
		content[cnt++]->setText(QObject::tr(tmp_buf);
		fprintf(stderr, "%d: %s, cnt = %d\n", __LINE__, tmp_buf, cnt);
		snprintf(tmp_buf, sizeof(tmp_buf), _("%d m / %d m"), (int)round(ts.elev_gain/ts.count), (int)round(ts.elev_loss/ts.count));
		break;
	}
	content[cnt++]->setText(tmp_buf);
	fprintf(stderr, "%d: %s, cnt = %d\n", __LINE__, tmp_buf, cnt);

	/* Total Duration. */
	int days    = (int) (ts.duration / (60*60*24));
	int hours   = (int) floor((ts.duration - (days * 60 * 60 * 24)) / (60 * 60));
	int minutes = (int) ((ts.duration - (days * 60 * 60 * 24) - (hours * 60 * 60)) / 60);
	snprintf(tmp_buf, sizeof(tmp_buf), _("%d:%02d:%02d days:hrs:mins"), days, hours, minutes);
	content[cnt++]->setText(tmp_buf);
	fprintf(stderr, "%d: %s, cnt = %d\n", __LINE__, tmp_buf, cnt);

	/* Average Duration. */
	int avg_dur = ts.duration / ts.count;
	hours   = (int) floor(avg_dur / (60*60));
	minutes = (int) ((avg_dur - (hours * 60 * 60)) / 60);
	snprintf(tmp_buf, sizeof(tmp_buf), _("%d:%02d hrs:mins"), hours, minutes);
	content[cnt++]->setText(tmp_buf);
	fprintf(stderr, "%d: %s, cnt = %d\n", __LINE__, tmp_buf, cnt);
#endif
}




/**
 * @widgets:           The widget layout
 * @tracks_and_layers: A list of #track_layer_t
 * @include_invisible: Whether to include invisible layers and tracks
 *
 * Analyse each item in the @tracks_and_layers list.
 */
void val_analyse(GtkWidget * widgets[], std::list<track_layer_t *> * tracks_and_layers, bool include_invisible)
{
	TrackStatistics stats;

	for (auto iter = tracks_and_layers->begin(); iter != tracks_and_layers->end(); iter++) {
		LayerTRW * trw = (*iter)->trw;
		assert (trw->type == LayerType::TRW);
		stats.add_track_maybe((*iter)->trk, trw->visible, trw->get_tracks_visibility(), trw->get_routes_visibility(), include_invisible);
	}

	table_output(stats, widgets);
}




typedef struct {
	GtkWidget ** widgets;
	GtkWidget * layout;
	GtkWidget * check_button;
	std::list<track_layer_t *> * tracks_and_layers;
	Layer * layer;
	SublayerType user_data;
	VikTrwlayerAnalyseCloseFunc on_close_cb;
} analyse_cb_t;

static void include_invisible_toggled_cb(GtkToggleButton * togglebutton, analyse_cb_t * acb)
{
#ifdef K
	bool value = false;
	if (gtk_toggle_button_get_active(togglebutton)) {
		value = true;
	}

	/* Delete old list of items. */
	if (acb->tracks_and_layers) {
		/* kamilTODO: delete acb->tracks_and_layers. */
	}

	/* Get the latest list of items to analyse. */
	/* kamilTODO: why do we need to get the latest list on checkbox toggle? */
	Layer * layer = acb->layer;
	if (acb->layer->type == LayerType::TRW) {
		acb->tracks_and_layers = ((LayerTRW *) layer)->create_tracks_and_layers_list(acb->user_data);
	} else if (layer->type == LayerType::AGGREGATE) {
		acb->tracks_and_layers = ((LayerAggregate *) layer)->create_tracks_and_layers_list(acb->user_data);
	} else {
		assert (0);
	}

	val_analyse(acb->widgets, acb->tracks_and_layers, value);
	gtk_widget_show_all(acb->layout);
#endif
}




#define VIK_SETTINGS_ANALYSIS_DO_INVISIBLE "track_analysis_do_invisible"

/**
 * Multi stage closure - as we need to clear allocations made here
 * before passing on to the callee so they know then the dialog is closed too.
 */
static void analyse_close(GtkWidget * dialog, int resp, analyse_cb_t * data)
{
#ifdef K
	/* Save current invisible value for next time. */
	bool do_invisible = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->check_button));
	a_settings_set_boolean(VIK_SETTINGS_ANALYSIS_DO_INVISIBLE, do_invisible);

	//free(data->layout);
	free(data->widgets);

	/* kamilTODO: delete data->tracks_and_layers here? */

	if (data->on_close_cb) {
		data->on_close_cb(dialog, resp, data->layer);
	}

	free(data);
#endif
}




/**
 * @window:                   A window from which the dialog will be derived
 * @name:                     The name to be shown
 * @layer:                    The #Layer passed on into get_tracks_and_layers_cb()
 * @user_data:                Data passed on into get_tracks_and_layers_cb()
 * @get_tracks_and_layers_cb: The function to call to construct items to be analysed
 *
 * Display a dialog with stats across many tracks.
 *
 * Returns: The dialog that is created to display the analyse information.
 */
GtkWidget * SlavGPS::vik_trw_layer_analyse_this(Window * window,
				       const char * name,
				       Layer * layer,
				       SublayerType sublayer_type,
				       VikTrwlayerAnalyseCloseFunc on_close_cb)
{
	//VikWindow *vw = VIK_WINDOW(window);
#ifdef K
	GtkWidget * dialog;
	dialog = gtk_dialog_new_with_buttons(_("Statistics"),
					     window,
					     GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_CLOSE,     GTK_RESPONSE_CANCEL,
					     NULL);

	QLabel * name_l = new QLabel("");
	char *myname = g_markup_printf_escaped("<b>%s</b>", name);
	gtk_label_set_markup(GTK_LABEL(name_l), myname);
	free(myname);

	GtkWidget * content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_box_pack_start (GTK_BOX(content), name_l, false, false, 10);

	/* Get previous value (if any) from the settings. */
	bool include_invisible;
	if (!a_settings_get_boolean (VIK_SETTINGS_ANALYSIS_DO_INVISIBLE, &include_invisible)) {
		include_invisible = true;
	}

	analyse_cb_t *acb = (analyse_cb_t *) malloc(sizeof(analyse_cb_t));
	acb->layer = layer;
	acb->user_data = sublayer_type;
	acb->on_close_cb = on_close_cb;
	if (layer->type == LayerType::TRW) {
		acb->tracks_and_layers = ((LayerTRW *) layer)->create_tracks_and_layers_list(acb->user_data);
	} else if (layer->type == LayerType::AGGREGATE) {
		acb->tracks_and_layers = ((LayerAggregate *) layer)->create_tracks_and_layers_list(acb->user_data);
	} else {
		assert (0);
	}
	acb->widgets = (GtkWidget **) malloc(sizeof(GtkWidget*) * G_N_ELEMENTS(label_texts));
	acb->layout = create_layout(acb->widgets);

	gtk_box_pack_start(GTK_BOX(content), acb->layout, false, false, 0);

	/* Analysis seems reasonably quick
	   unless you have really large numbers of tracks (i.e. many many thousands or a really slow computer).
	   One day might store stats in the track itself... */
	val_analyse(acb->widgets, acb->tracks_and_layers, include_invisible);

	GtkWidget * cb = gtk_check_button_new_with_label(_("Include Invisible Items"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb), include_invisible);
	gtk_box_pack_start (GTK_BOX(content), cb, false, false, 10);
	acb->check_button = cb;

	gtk_widget_show_all(dialog);

	QObject::connect(cb, SIGNAL("toggled"), acb, SLOT (include_invisible_toggled_cb));
	QObject::connect(dialog, SIGNAL("response"), acb, SLOT (analyse_close));

	return dialog;
#endif
}
