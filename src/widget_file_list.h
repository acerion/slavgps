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

#ifndef _SG_WIDGET_FILE_LIST_H_
#define _SG_WIDGET_FILE_LIST_H_




#include <QObject>
#include <QWidget>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QStandardItemModel>
#include <QTableView>
#include <QFileDialog>




#include "widget_file_entry.h"




namespace SlavGPS {




	class FileListWidget : public QWidget {
		Q_OBJECT
	public:
		FileListWidget(const QString & title, const QStringList & fl, QWidget * parent);
		~FileListWidget();

		QStringList get_list(void);

		void set_file_type_filter(FileSelectorWidget::FileTypeFilter file_type_filter);

	signals:

	private slots:
		void add_file();
		void del_file();


	private:
		QStringList file_list;
		QDialogButtonBox * button_box = NULL;
		QPushButton * add = NULL;
		QPushButton * del = NULL;
		QVBoxLayout * vbox = NULL;

		QStandardItemModel * model = NULL;
		QTableView * view = NULL;

		QFileDialog * file_selector = NULL;

		FileSelectorWidget::FileTypeFilter file_type_filter = FileSelectorWidget::FileTypeFilter::Any;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WIDGET_FILE_LIST_H_ */
