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




	enum class DataSourceInputType {
		None = 0,
		TRWLayer,
		Track,
		TRWLayerTrack
	};




	enum class DataSourceMode {
		/* Generally Datasources shouldn't use these and let the HCI decide between the create or add to layer options. */
		CreateNewLayer,
		AddToLayer,
		AutoLayerManagement,
		ManualLayerManagement,
	};
	/* TODO: replace track/layer? */




	class DataSourceDialog : public BasicDialog {
		Q_OBJECT
	public:
		DataSourceDialog() {};

		virtual ProcessOptions * get_process_options(void) { return NULL; };
		virtual ProcessOptions * get_process_options(const QString & input_filename, const QString & input_track_filename) { return NULL; };
		virtual void adjust_download_options(DownloadOptions & dl_options) { return; };
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DATASOURCE_H_ */
