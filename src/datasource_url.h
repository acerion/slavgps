/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_DATASOURCE_URL_H_
#define _SG_DATASOURCE_URL_H_




#include <QComboBox>
#include <QLineEdit>




#include "datasource.h"
#include "acquire.h"




namespace SlavGPS {




	class DataSourceURL : public DataSourceBabel {
	public:
		DataSourceURL();
		~DataSourceURL();

		DataSourceDialog * create_setup_dialog(Viewport * viewport, void * user_data);
	};




	class DataSourceURLDialog : public DataSourceDialog {
		Q_OBJECT
	public:
		DataSourceURLDialog();
		~DataSourceURLDialog();

		ProcessOptions * get_process_options(void);
		void adjust_download_options(DownloadOptions & dl_options) const;

	private:
		QLineEdit url_input;
		QComboBox file_type_combo;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DATASOURCE_URL_H_ */
