/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_DATASOURCE_ROUTING_H_
#define _SG_DATASOURCE_ROUTING_H_




#include <QComboBox>
#include <QLineEdit>




#include "acquire.h"
#include "datasource.h"




namespace SlavGPS {




	class DataSourceRouting : public DataSourceBabel {
	public:
		DataSourceRouting();
		~DataSourceRouting();

		DataSourceDialog * create_setup_dialog(Viewport * viewport, void * user_data);
	};




	class DataSourceRoutingDialog : public DataSourceDialog {
		Q_OBJECT
	public:
		DataSourceRoutingDialog();
		~DataSourceRoutingDialog();

		ProcessOptions * get_process_options(DownloadOptions & dl_options);

		QComboBox * engines_combo;
		QLineEdit from_entry;
		QLineEdit to_entry;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DATASOURCE_ROUTING_H_ */
