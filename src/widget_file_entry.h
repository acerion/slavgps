/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_WIDGET_FILE_ENTRY_H_
#define _SG_WIDGET_FILE_ENTRY_H_




#include <QObject>
#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QLineEdit>

#include "file.h"




namespace SlavGPS {




	enum class SGFileTypeFilter;




	class SGFileEntry : public QWidget {
		Q_OBJECT
	public:
		SGFileEntry(enum QFileDialog::Option options, enum QFileDialog::FileMode mode, SGFileTypeFilter file_type_filter, const QString & title, QWidget * parent);
		~SGFileEntry();

		void preselect_file_full_path(const QString & file_path);
		QString get_selected_file_full_path(void) const;
		QStringList get_selected_files_full_paths(void) const;

		void set_directory_url(const QUrl & dir_url);
		QUrl get_directory_url(void) const;

		void set_name_filters(const QStringList & name_filters);
		void select_name_filter(const QString & name_filter);
		QString get_selected_name_filter(void) const;

		void set_accept_mode(QFileDialog::AcceptMode accept_mode);

	private slots:
		void open_browser_cb();

	private:
		void add_file_type_filters(SGFileTypeFilter file_type_filter);

		QFileDialog * file_dialog = NULL;
		QLineEdit * line = NULL;
		QPushButton * browse = NULL;
		QHBoxLayout * hbox = NULL;

		QString current_file_full_path;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WIDGET_FILE_ENTRY_H_ */
