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




#include <cstring>
#include <cstdlib>
#include <cstdio>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif




#include <QDebug>
#include <QFileDialog>
#include <QDir>




#include "babel.h"
#include "babel_dialog.h"
#include "layer_trw.h"
#include "gpx.h"
#include "file_utils.h"
#include "dialog.h"
#include "util.h"
#include "window.h"
#include "vikutils.h"
#include "widget_file_entry.h"




using namespace SlavGPS;




#define SG_MODULE "Layer TRW Export"




static QUrl last_folder_url;




void LayerTRW::export_layer(const QString & title, const QString & default_file_full_path, Track * trk, SGFileType file_type)
{
	QFileDialog file_selector(this->get_window(), title);
	file_selector.setFileMode(QFileDialog::AnyFile); /* Specify new or select existing file. */
	file_selector.setAcceptMode(QFileDialog::AcceptSave);

	if (!last_folder_url.toString().isEmpty()) {
		file_selector.setDirectoryUrl(last_folder_url);
	}

	file_selector.selectFile(default_file_full_path);

	if (QDialog::Accepted == file_selector.exec()) {
		const QString output_file_full_path = file_selector.selectedFiles().at(0);
		qDebug() << SG_PREFIX_I << "Will export file to" << output_file_full_path;

		last_folder_url = file_selector.directoryUrl();

		this->get_window()->set_busy_cursor();
		/* Don't Export invisible items - unless requested on this specific track. */
		const SaveStatus result = VikFile::export_trw(this, output_file_full_path, file_type, trk, trk ? true : false);
		this->get_window()->clear_busy_cursor();

		if (SaveStatus::Code::Success != result.code) {
			result.show_status_dialog(this->get_window());
		}
	}
}




/**
 * Convert the given TRW layer into a temporary GPX file and open it with the specified program.
 */
void LayerTRW::open_layer_with_external_program(const QString & external_program)
{
	/* Don't Export invisible items. */
	static GPXWriteOptions options(true, true, false, false);
	QString name_used;

	SaveStatus save_status = GPX::write_layer_to_tmp_file(name_used, this, &options);
	if (SaveStatus::Code::Success == save_status) {
		const QString quoted_file = Util::shell_quote(name_used);
		const QString command = QString("%1 %2").arg(external_program).arg(quoted_file);

		if (!QProcess::startDetached(command)) {
			Dialog::error(QObject::tr("Could not launch %1.").arg(external_program), this->get_window());
		}
		Util::add_to_deletion_list(name_used);
	} else {
		Dialog::error(QObject::tr("Could not create temporary file for export."), this->get_window());
	}
}




SaveStatus LayerTRW::export_layer_with_gpsbabel(const QString & title, const QString & default_file_full_path)
{
	BabelMode mode = { 0, 0, 0, 0, 0, 0 };
	if (this->get_tracks().size()) {
		mode.tracks_write = 1;
	}
	if (this->get_routes().size()) {
		mode.routes_write = 1;
	}
	if (this->get_waypoints().size()) {
		mode.waypoints_write = 1;
	}

	SaveStatus save_status = SaveStatus::Code::GenericError;

	BabelDialog dialog(title);
	dialog.build_ui(&mode);

	const QString cwd = QDir::currentPath();
	if (!cwd.isEmpty()) {
		dialog.file_selector->set_directory_url(QUrl(cwd));
	}

	/* Set possible name of the file. */
	dialog.file_selector->preselect_file_full_path(default_file_full_path);

	const int rv = dialog.exec();
	if (rv == QDialog::Accepted) {
		const BabelFileType * file_type = dialog.get_file_type_selection();
		const QString output_file_full_path = dialog.file_selector->get_selected_file_full_path();

		qDebug() << SG_PREFIX_I << "Dialog result: accepted";
		qDebug() << SG_PREFIX_I << "Selected format type identifier:" << file_type->identifier;
		qDebug() << SG_PREFIX_I << "Selected format type label:" << file_type->label;
		qDebug() << SG_PREFIX_I << "Selected file path:" << output_file_full_path;



		dialog.get_write_mode(mode); /* We overwrite the old values of the struct, but that's ok. */

		if (file_type == NULL) {
			Dialog::error(QObject::tr("You did not select a valid file format."), this->get_window());
		} else {
			this->get_window()->set_busy_cursor();
			save_status = VikFile::export_with_babel(this, output_file_full_path, file_type->identifier, mode.tracks_write, mode.routes_write, mode.waypoints_write);
			this->get_window()->clear_busy_cursor();
		}


	} else if (rv == QDialog::Rejected) {
		qDebug() << SG_PREFIX_I << "Dialog result: rejected";
	} else {
		qDebug() << SG_PREFIX_E << "Dialog result: unknown:" << rv;
	}

	if (SaveStatus::Code::Success != save_status) {
		save_status.show_status_dialog(this->get_window());
	}

	return save_status;
}
