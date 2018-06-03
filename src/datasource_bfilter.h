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
#include "babel.h"




namespace SlavGPS {




	class BFilterSimplify : public DataSourceBabel {
	public:
		BFilterSimplify();

		int run_config_dialog(AcquireProcess * acquire_context);
	};

	class BFilterSimplifyDialog : public DataSourceDialog {
	public:
		BFilterSimplifyDialog(const QString & window_title);

		BabelOptions * get_process_options_layer(const QString & input_layer_filename);
		QSpinBox * spin = NULL;
	};




	class BFilterCompress : public DataSourceBabel {
	public:
		BFilterCompress();

		int run_config_dialog(AcquireProcess * acquire_context);
	};

	class BFilterCompressDialog : public DataSourceDialog {
	public:
		BFilterCompressDialog(const QString & window_title);

		BabelOptions * get_process_options_layer(const QString & input_layer_filename);
		QDoubleSpinBox * spin = NULL;
	};




	class BFilterDuplicates : public DataSourceBabel {
	public:
		BFilterDuplicates();

		int run_config_dialog(AcquireProcess * acquire_context);
	};

	class BFilterDuplicatesDialog : public DataSourceDialog {
	public:
		BFilterDuplicatesDialog(const QString & window_title) : DataSourceDialog(window_title) {};

		BabelOptions * get_process_options_layer(const QString & input_layer_filename);
	};




	class BFilterManual : public DataSourceBabel {
	public:
		BFilterManual();

		int run_config_dialog(AcquireProcess * acquire_context);
	};

	class BFilterManualDialog : public DataSourceDialog {
	public:
		BFilterManualDialog(const QString & window_title);

		BabelOptions * get_process_options_layer(const QString & input_layer_filename);
		QLineEdit * entry = NULL;
	};




	class BFilterPolygon : public DataSourceBabel {
	public:
		BFilterPolygon();
		int run_config_dialog(AcquireProcess * acquire_context);
	};

	class BFilterPolygonDialog : public DataSourceDialog {
	public:
		BFilterPolygonDialog(const QString & window_title) : DataSourceDialog(window_title) {};

		BabelOptions * get_process_options_layer_track(const QString & layer_input_file_full_path, const QString & track_input_file_full_path);
	};




	class BFilterExcludePolygon : public DataSourceBabel {
	public:
		BFilterExcludePolygon();
		int run_config_dialog(AcquireProcess * acquire_context);
	};

	class BFilterExcludePolygonDialog : public DataSourceDialog {
	public:
		BFilterExcludePolygonDialog(const QString & window_title) : DataSourceDialog(window_title) {};

		BabelOptions * get_process_options_layer_track(const QString & layer_input_file_full_path, const QString & track_input_file_full_path);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DATASOURCE_BFILTER_H_ */
