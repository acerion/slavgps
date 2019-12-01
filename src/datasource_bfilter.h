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
#include "datasource_babel.h"




namespace SlavGPS {




	class BFilterSimplify : public DataSourceBabel {
	public:
		BFilterSimplify();

		int run_config_dialog(AcquireContext * acquire_context);

		SGObjectTypeID get_source_id(void) const override;
		static SGObjectTypeID source_id(void);
	};

	class BFilterSimplifyDialog : public DataSourceDialog {
	public:
		BFilterSimplifyDialog(const QString & window_title);
		AcquireOptions * create_acquire_options(AcquireContext * acquire_context);

		QSpinBox * spin = NULL;
	};




	class BFilterCompress : public DataSourceBabel {
	public:
		BFilterCompress();

		int run_config_dialog(AcquireContext * acquire_context);

		SGObjectTypeID get_source_id(void) const override;
		static SGObjectTypeID source_id(void);
	};

	class BFilterCompressDialog : public DataSourceDialog {
	public:
		BFilterCompressDialog(const QString & window_title);
		AcquireOptions * create_acquire_options(AcquireContext * acquire_context);

		QDoubleSpinBox * spin = NULL;
	};




	class BFilterDuplicates : public DataSourceBabel {
	public:
		BFilterDuplicates();

		int run_config_dialog(AcquireContext * acquire_context);

		SGObjectTypeID get_source_id(void) const override;
		static SGObjectTypeID source_id(void);
	};

	class BFilterDuplicatesDialog : public DataSourceDialog {
	public:
		BFilterDuplicatesDialog(const QString & window_title) : DataSourceDialog(window_title) {};
		AcquireOptions * create_acquire_options(AcquireContext * acquire_context);
	};




	class BFilterManual : public DataSourceBabel {
	public:
		BFilterManual();

		int run_config_dialog(AcquireContext * acquire_context);

		SGObjectTypeID get_source_id(void) const override;
		static SGObjectTypeID source_id(void);
	};

	class BFilterManualDialog : public DataSourceDialog {
	public:
		BFilterManualDialog(const QString & window_title);
		AcquireOptions * create_acquire_options(AcquireContext * acquire_context);

		QLineEdit * entry = NULL;
	};




	class BFilterPolygon : public DataSourceBabel {
	public:
		BFilterPolygon();
		int run_config_dialog(AcquireContext * acquire_context);

		SGObjectTypeID get_source_id(void) const override;
		static SGObjectTypeID source_id(void);
	};

	class BFilterPolygonDialog : public DataSourceDialog {
	public:
		BFilterPolygonDialog(const QString & window_title) : DataSourceDialog(window_title) {};
		AcquireOptions * create_acquire_options(AcquireContext * acquire_context);
	};




	class BFilterExcludePolygon : public DataSourceBabel {
	public:
		BFilterExcludePolygon();
		int run_config_dialog(AcquireContext * acquire_context);

		SGObjectTypeID get_source_id(void) const override;
		static SGObjectTypeID source_id(void);
	};

	class BFilterExcludePolygonDialog : public DataSourceDialog {
	public:
		BFilterExcludePolygonDialog(const QString & window_title) : DataSourceDialog(window_title) {};
		AcquireOptions * create_acquire_options(AcquireContext * acquire_context);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DATASOURCE_BFILTER_H_ */
