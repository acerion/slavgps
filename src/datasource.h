/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2012, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_DATASOURCE_H_
#define _SG_DATASOURCE_H_




#include "dialog.h"




namespace SlavGPS {




	class ProcessOptions;
	class DownloadOptions;




	enum class DatasourceInputtype {
		NONE = 0,
		TRWLAYER,
		TRACK,
		TRWLAYER_TRACK
	};




	enum class DataSourceMode {
		/* Generally Datasources shouldn't use these and let the HCI decide between the create or add to layer options. */
		CREATE_NEW_LAYER,
		ADD_TO_LAYER,
		AUTO_LAYER_MANAGEMENT,
		MANUAL_LAYER_MANAGEMENT,
	};
	/* TODO: replace track/layer? */




	class DataSourceDialog : public BasicDialog {
		Q_OBJECT
	public:
		virtual ProcessOptions * get_process_options(DownloadOptions & dl_options);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DATASOURCE_H_ */
