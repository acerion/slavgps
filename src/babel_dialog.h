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




namespace SlavGPS {




	class SGFileEntry;




	class BabelDialog : public QDialog {
		Q_OBJECT
	public:
		BabelDialog(QString const & title, QWidget * parent = NULL);
		~BabelDialog();

		void build_ui(const BabelMode * mode = NULL);

		void get_write_mode(BabelMode & mode);

		QComboBox * build_file_type_selector(const BabelMode & mode);
		BabelFileType * get_file_type_selection(void);

		void add_file_type_filter(BabelFileType * babel_file_type);

	private slots:
		void file_type_changed_cb(int index);

	public:
		SlavGPS::SGFileEntry * file_entry = NULL;
		QComboBox * file_types_combo = NULL;

	private:
		QHBoxLayout * build_mode_selector(bool tracks, bool routes, bool waypoints);
		QVBoxLayout * vbox = NULL;
		QHBoxLayout * mode_box = NULL;
		QDialogButtonBox * button_box = NULL;

		bool do_import = true; /* The dialog may be used to perform export of TRW data, or import of TRW data. */
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_BABEL_DIALOG_H_ */
