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

#include <QDebug>
#include <QFileDialog>
#include <QDir>

#include "babel.h"
#include "babel_dialog.h"
#include "layer_trw.h"
#include "gpx.h"
#include "fileutils.h"
#include "dialog.h"
#include "util.h"
#include "globals.h"
#include "window.h"
#include "vikutils.h"
#include "widget_file_entry.h"




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
		const bool success = a_file_export(this, output_file_name, file_type, trk, trk ? true : false);
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




int LayerTRW::export_layer_with_gpsbabel(const QString & title, const QString & default_file_name)
{
	BabelMode mode = { 0, 0, 0, 0, 0, 0 };
	if (this->get_route_items().size()) {
		mode.routes_write = 1;
	}
	if (this->get_track_items().size()) {
		mode.tracks_write = 1;
	}
	if (this->get_waypoint_items().size()) {
		mode.waypoints_write = 1;
	}
	bool failed = false;

	BabelDialog * dialog = new BabelDialog(title);
	dialog->build_ui(&mode);

	const QString cwd = QDir::currentPath();
	if (!cwd.isEmpty()) {
#ifdef K
		gtk_file_chooser_set_current_folder(dialog, cwd);
#endif
	}

	/* Set possible name of the file. */
	dialog->file_entry->file_selector->selectFile(default_file_name);

	int rv = dialog->exec();
	if (rv == QDialog::Accepted) {
		const BabelFileType * file_type = dialog->get_file_type_selection();
		const QString output_file_path = dialog->file_entry->get_filename();

		qDebug() << "II: Layer TRW Export via gpsbabel: dialog result: accepted";
		qDebug() << "II: Layer TRW Export via gpsbabel: selected format type name:" << file_type->name;
		qDebug() << "II: Layer TRW Export via gpsbabel: selected format type label:" << file_type->label;
		qDebug() << "II: Layer TRW Export via gpsbabel: selected file path:" << output_file_path;


		this->get_window()->set_busy_cursor();
		dialog->get_write_mode(mode); /* We overwrite the old values of the struct, but that's ok. */

		if (file_type == NULL) {
			Dialog::error(QObject::tr("You did not select a valid file format."), this->get_window());
		} else {
			;
		}

		failed = !a_file_export_babel(this, QString(output_file_path), file_type->name, mode.tracks_write, mode.routes_write, mode.waypoints_write);

		this->get_window()->clear_busy_cursor();

	} else if (rv == QDialog::Rejected) {
		qDebug() << "II: Layer TRW Export via gpsbabel: dialog result: rejected";
	} else {
		qDebug() << "EE: Layer TRW Export via gpsbabel: dialog result: unknown:" << rv;
	}

	if (failed) {
		Dialog::error(QObject::tr("The filename you requested could not be opened for writing."), this->get_window());
	}

	return rv;
}
