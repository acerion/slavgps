/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#include <cstring>
#include <cstdlib>
#include <cstdio>

#include <glib.h>
#include <glib/gstdio.h>

#include "babel.h"
#include "babel_ui.h"
#include "layer_trw.h"
#include "layer_trw_export.h"
#include "gpx.h"
#include "fileutils.h"
#include "dialog.h"
#include "util.h"
#include "globals.h"
#include "window.h"




using namespace SlavGPS;




static char * last_folder_uri = NULL;




void SlavGPS::vik_trw_layer_export(LayerTRW * layer, const QString & title, const QString & default_name, Track * trk, VikFileType_t file_type)
{
#ifdef K
	GtkWidget * file_selector;
	char const * fn;
	bool failed = false;
	file_selector = gtk_file_chooser_dialog_new(title,
						    NULL,
						    GTK_FILE_CHOOSER_ACTION_SAVE,
						    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						    GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
						    NULL);
	if (last_folder_uri) {
		gtk_file_chooser_set_current_folder_uri(GTK_FILE_CHOOSER(file_selector), last_folder_uri);
	}

	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(file_selector), default_name);

	while (gtk_dialog_run(GTK_DIALOG(file_selector)) == GTK_RESPONSE_ACCEPT) {
		fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(file_selector));
		if (0 != access(fn, F_OK)
		    || Dialog::yes_or_no(tr("The file \"%1\" exists, do you wish to overwrite it?").arg(file_base_name(fn)), GTK_WINDOW(file_selector))) {

			free(last_folder_uri);
			last_folder_uri = gtk_file_chooser_get_current_folder_uri(GTK_FILE_CHOOSER(file_selector));

			gtk_widget_hide(file_selector);
			layer->get_window()->set_busy_cursor();
			/* Don't Export invisible items - unless requested on this specific track. */
			failed = ! a_file_export(layer, fn, file_type, trk, trk ? true : false);
			layer->get_window()->clear_busy_cursor();
			break;
		}
	}
	gtk_widget_destroy(file_selector);
	if (failed) {
		Dialog::error(tr("The filename you requested could not be opened for writing."), layer->get_window());
	}
#endif
}




/**
 * Convert the given TRW layer into a temporary GPX file and open it with the specified program.
 */
void SlavGPS::vik_trw_layer_export_external_gpx(LayerTRW * trw, char const * external_program)
{
	/* Don't Export invisible items. */
	static GpxWritingOptions options = { true, true, false, false };
	char *name_used = a_gpx_write_tmp_file(trw, &options);

	if (name_used) {
		GError *err = NULL;
		char *quoted_file = g_shell_quote (name_used);
		char *cmd = g_strdup_printf("%s %s", external_program, quoted_file);
		free(quoted_file);
		if (! g_spawn_command_line_async(cmd, &err)) {
			Dialog::error(QObject::tr("Could not launch %1.").arg(QString(external_program)), trw->get_window());
			g_error_free(err);
		}
		free(cmd);
		util_add_to_deletion_list(name_used);
		free(name_used);
	} else {
		Dialog::error(QObject::tr("Could not create temporary file for export."), trw->get_window());
	}
}




void SlavGPS::vik_trw_layer_export_gpsbabel(LayerTRW * trw, const QString & title, const QString & default_name)
{
	BabelMode mode = { 0, 0, 0, 0, 0, 0 };
	if (trw->get_routes().size()) {
		mode.routes_write = 1;
	}
	if (trw->get_tracks().size()) {
		mode.tracks_write = 1;
	}
	if (trw->get_waypoints().size()) {
		mode.waypoints_write = 1;
	}
#ifdef K
	GtkWidget * file_selector;
	const char * fn;
	bool failed = false;
	file_selector = gtk_file_chooser_dialog_new(title,
						    NULL,
						    GTK_FILE_CHOOSER_ACTION_SAVE,
						    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						    GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
						    NULL);
	char *cwd = g_get_current_dir();
	if (cwd) {
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(file_selector), cwd);
		free(cwd);
	}

	/* Build the extra part of the widget. */
	QComboBox * babel_selector = a_babel_ui_file_type_selector_new(mode);
	QLabel * label = new QLabel(QObject::tr("File format:"));
	GtkWidget * hbox = gtk_hbox_new(false, 0);
	hbox->addWidget(label);
	hbox->addWidget(babel_selector);
	gtk_widget_show(babel_selector);
	gtk_widget_show(label);
	gtk_widget_show_all(hbox);

	babel_selector->setToolTip(QObject::tr("Select the file format."));

	GtkWidget * babel_modes = a_babel_ui_modes_new(mode.tracksWrite, mode.routesWrite, mode.waypointsWrite);
	gtk_widget_show(babel_modes);

	babel_modes->setToolTip(QObject::tr("Select the information to process.\n"
					    "Warning: the behavior of these switches is highly dependent of the file format selected.\n"
					    "Please, refer to GPSbabel if unsure."));

	GtkWidget * vbox = gtk_vbox_new(false, 0);
	vbox->addWidget(hbox);
	vbox->addWidget(babel_modes);
	gtk_widget_show_all(vbox);

	gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(file_selector), vbox);

	/* Add some dynamic: only allow dialog's validation when format selection is done. */
	QObject::connect(babel_selector, SIGNAL("changed"), file_selector, SLOT (a_babel_ui_type_selector_dialog_sensitivity_cb));
	/* Manually call the callback to fix the state. */
	a_babel_ui_type_selector_dialog_sensitivity_cb(babel_selector, file_selector);

	/* Set possible name of the file. */
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(file_selector), default_name);

	while (gtk_dialog_run(GTK_DIALOG(file_selector)) == GTK_RESPONSE_ACCEPT) {
		fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_selector));
		if (0 != access(fn, F_OK)
		    || Dialog::yes_or_no(QObject::tr("The file \"%1\" exists, do you wish to overwrite it?").arg(file_base_name(fn)), GTK_WINDOW(file_selector))) {

			BabelFileType * active = a_babel_ui_file_type_selector_get(babel_selector);
			if (active == NULL) {
				Dialog::error(QObject::tr("You did not select a valid file format."), trw->get_window());
			} else {
				gtk_widget_hide(file_selector);
				trw->get_window()->set_busy_cursor();
				bool tracks, routes, waypoints;
				a_babel_ui_modes_get(babel_modes, &tracks, &routes, &waypoints);
				failed = ! a_file_export_babel(trw, fn, active->name, tracks, routes, waypoints);
				trw->get_window()->clear_busy_cursor();
				break;
			}
		}
	}
	//babel_ui_selector_destroy(babel_selector);
	gtk_widget_destroy(file_selector);
	if (failed) {
		Dialog::error(QObject::tr("The filename you requested could not be opened for writing."), trw->get_window());
	}
#endif
}
