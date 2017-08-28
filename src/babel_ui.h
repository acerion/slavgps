/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013 Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifndef _SG_BABEL_DIALOG_H_
#define _SG_BABEL_DIALOG_H_




#include <QString>
#include <QWidget>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QComboBox>

#include "babel.h"
#include "widget_file_entry.h"




namespace SlavGPS {




	class BabelDialog : public QDialog {
		Q_OBJECT
	public:
		BabelDialog(QString const & title, QWidget * parent = NULL);
		~BabelDialog();

		void set_write_mode(const BabelMode & mode);
		void get_write_mode(BabelMode & mode);
		void build_ui(void);
		QHBoxLayout * build_mode_selector(const BabelMode & mode);
		QComboBox * build_file_type_selector(const BabelMode & mode);
		BabelFileType * get_file_type_selection(void);

	private slots:
		void file_type_changed_cb(int index);

	public:
		QDialogButtonBox * button_box = NULL;
		QVBoxLayout * vbox = NULL;
		QComboBox * file_types_combo = NULL;

		SlavGPS::SGFileEntry * file_entry = NULL;

		void add_file_type_filter(BabelFileType * babel_file_type);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_BABEL_DIALOG_H_ */
