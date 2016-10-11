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
 *
 */
#ifndef _SG_WIDGET_FILE_ENTRY_H_
#define _SG_WIDGET_FILE_ENTRY_H_




#include <QObject>
#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QLineEdit>




namespace SlavGPS {




	class SGFileEntry : public QWidget {
		Q_OBJECT
	public:
		SGFileEntry(enum QFileDialog::Option options, enum QFileDialog::FileMode mode, QString & title, QWidget * parent);
		~SGFileEntry();

		void set_filename(QString & filename);
		QString get_filename();


	private slots:

		void open_browser_cb();


	private:
		QLineEdit * line = NULL;
		QPushButton * browse = NULL;
		QHBoxLayout * hbox = NULL;

		QFileDialog * file_selector = NULL;

		QString selector_title;
		QString selector_filename;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WIDGET_FILE_ENTRY_H_ */