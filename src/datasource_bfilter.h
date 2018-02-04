/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2014-2015, Rob Norris <rw_norris@hotmail.com>
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
 * See: http://www.gpsbabel.org/htmldoc-development/Data_Filters.html
 */

#ifndef _SG_DATASOURCE_BFILTER_H_
#define _SG_DATASOURCE_BFILTER_H_




#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>




#include "datasource.h"
#include "acquire.h"




namespace SlavGPS {




	class BFilterSimplify : public DataSourceBabel {
	public:
		BFilterSimplify();

		DataSourceDialog * create_setup_dialog(Viewport * viewport, void * user_data);
	};

	class BFilterSimplifyDialog : public DataSourceDialog {
	public:
		BFilterSimplifyDialog();

		ProcessOptions * get_process_options(const QString & input_filename, const QString & not_used);
		QSpinBox * spin = NULL;
	};




	class BFilterCompress : public DataSourceBabel {
	public:
		BFilterCompress();

		DataSourceDialog * create_setup_dialog(Viewport * viewport, void * user_data);
	};

	class BFilterCompressDialog : public DataSourceDialog {
	public:
		BFilterCompressDialog();

		ProcessOptions * get_process_options(const QString & input_filename, const QString & not_used);
		QDoubleSpinBox * spin = NULL;
	};




	class BFilterDuplicates : public DataSourceBabel {
	public:
		BFilterDuplicates();

		DataSourceDialog * create_setup_dialog(Viewport * viewport, void * user_data);
	};

	class BFilterDuplicatesDialog : public DataSourceDialog {
	public:
		BFilterDuplicatesDialog() {};

		ProcessOptions * get_process_options(const QString & input_filename, const QString & not_used);
	};




	class BFilterManual : public DataSourceBabel {
	public:
		BFilterManual();

		DataSourceDialog * create_setup_dialog(Viewport * viewport, void * user_data);
	};

	class BFilterManualDialog : public DataSourceDialog {
	public:
		BFilterManualDialog();

		ProcessOptions * get_process_options(const QString & input_filename, const QString & not_used);
		QLineEdit * entry = NULL;
	};




	class BFilterPolygon : public DataSourceBabel {
	public:
		BFilterPolygon();
	};

	class BFilterPolygonDialog : public DataSourceDialog {
	public:
		BFilterPolygonDialog();

		ProcessOptions * get_process_options(const QString & input_filename, const QString & input_track_filename);
	};




	class BFilterExcludePolygon : public DataSourceBabel {
	public:
		BFilterExcludePolygon();
	};

	class BFilterExcludePolygonDialog : public DataSourceDialog {
	public:
		BFilterExcludePolygonDialog();

		ProcessOptions * get_process_options(const QString & input_filename, const QString & input_track_filename);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DATASOURCE_BFILTER_H_ */
