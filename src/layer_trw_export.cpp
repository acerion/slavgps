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
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "babel.h"
#include "babel_ui.h"
#include "layer_trw.h"
#include "gpx.h"
#include "fileutils.h"
#include "dialog.h"
#include "util.h"
#include "globals.h"
#include "window.h"
#include "vikutils.h"




using namespace SlavGPS;




static QUrl last_folder_url;




void LayerTRW::export_layer(const QString & title, const QString & default_file_name, Track * trk, SGFileType file_type)
{
	QFileDialog file_selector(this->get_window(), title);
	file_selector.setFileMode(QFileDialog::AnyFile); /* Specify new or select existing file. */
	file_selector.setAcceptMode(QFileDialog::AcceptSave);

	if (!last_folder_url.toString().isEmpty()) {
		file_selector.setDirectoryUrl(last_folder_url);
	}

	file_selector.selectFile(default_file_name);

	if (QDialog::Accepted == file_selector.exec()) {
		const QString output_file_name = file_selector.selectedFiles().at(0);

		last_folder_url = file_selector.directoryUrl();

		this->get_window()->set_busy_cursor();
		/* Don't Export invisible items - unless requested on this specific track. */
		const bool success = a_file_export(this, output_file_name.toUtf8().constData(), file_type, trk, trk ? true : false);
		this->get_window()->clear_busy_cursor();

		if (!success) {
			Dialog::error(QObject::tr("The filename you requested could not be opened for writing."), this->get_window());
		}
	}
}




/**
 * Convert the given TRW layer into a temporary GPX file and open it with the specified program.
 */
void LayerTRW::open_layer_with_external_program(const QString & external_program)
{
	/* Don't Export invisible items. */
	static GpxWritingOptions options = { true, true, false, false };
	char * name_used = a_gpx_write_tmp_file(this, &options);

	if (name_used) {
		char * quoted_file = g_shell_quote(name_used);
		const QString command = QString("%1 %2").arg(external_program).arg(quoted_file);
		free(quoted_file);
		if (!QProcess::startDetached(command)) {
			Dialog::error(QObject::QObject::tr("Could not launch %1.").arg(external_program), this->get_window());
		}
		util_add_to_deletion_list(name_used);
		free(name_used);
	} else {
		Dialog::error(QObject::QObject::tr("Could not create temporary file for export."), this->get_window());
	}
}




void LayerTRW::export_layer_with_gpsbabel(const QString & title, const QString & default_name)
{
	BabelMode mode = { 0, 0, 0, 0, 0, 0 };
	if (this->get_routes().size()) {
		mode.routes_write = 1;
	}
	if (this->get_tracks().size()) {
		mode.tracks_write = 1;
	}
	if (this->get_waypoints().size()) {
		mode.waypoints_write = 1;
	}
	bool failed = false;
#ifdef K
	GtkWidget * file_selector;
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
#endif
	QComboBox * babel_selector = a_babel_ui_file_type_selector_new(mode);
#ifdef K
	QLabel * label = new QLabel(QObject::tr("File format:"));
	GtkWidget * hbox = gtk_hbox_new(false, 0);
	hbox->addWidget(label);
	hbox->addWidget(babel_selector);
	gtk_widget_show(babel_selector);
	gtk_widget_show(label);
	gtk_widget_show_all(hbox);

	babel_selector->setToolTip(QObject::tr("Select the file format."));
#endif
	QHBoxLayout * babel_modes = a_babel_ui_modes_new(mode.tracks_write, mode.routes_write, mode.waypoints_write);
#ifdef K
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
#endif
		char * output_file_path = NULL;
		// output_file_name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_selector));
		if (0 != access(output_file_path, F_OK)
		    || Dialog::yes_or_no(QObject::tr("The file \"%1\" exists, do you wish to overwrite it?").arg(file_base_name(output_file_path)), this->get_window())) {

			const BabelFileType * active_type = a_babel_ui_file_type_selector_get(babel_selector);
			if (active_type == NULL) {
				Dialog::error(QObject::tr("You did not select a valid file format."), this->get_window());
			} else {
				//gtk_widget_hide(file_selector);
				this->get_window()->set_busy_cursor();
				bool do_tracks, do_routes, do_waypoints;
				a_babel_ui_modes_get(babel_modes, &do_tracks, &do_routes, &do_waypoints);
				failed = !a_file_export_babel(this, QString(output_file_path), active_type->name, do_tracks, do_routes, do_waypoints);
				this->get_window()->clear_busy_cursor();
				//break;
			}
		}
#ifdef K
	}
	//babel_ui_selector_destroy(babel_selector);
	gtk_widget_destroy(file_selector);
#endif
	if (failed) {
		Dialog::error(QObject::tr("The filename you requested could not be opened for writing."), this->get_window());
	}
}
