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

#ifndef _SG_DATASOURCE_GPS_H_
#define _SG_DATASOURCE_GPS_H_




#include <QString>
#include <QComboBox>
#include <QLabel>
#include <QCheckBox>




#include "layer_trw_import.h"
#include "babel.h"
#include "datasource.h"
#include "layer_gps.h"
#include "datasource_babel.h"




namespace SlavGPS {




	class DataSourceGPSDialog;




	class DataSourceGPS : public DataSourceBabel {
	public:
		DataSourceGPS();
		~DataSourceGPS() {};

		int run_config_dialog(AcquireContext & acquire_context) override;

		SGObjectTypeID get_source_id(void) const override;
		static SGObjectTypeID source_id(void);

		AcquireProgressDialog * create_progress_dialog(void * user_data);

		sg_ret on_complete(void);

		/* FIXME: this is most probably unused after changes in acquire. Make it used again. */
		void progress_func(AcquireProgressCode code, void * data, AcquireContext * acquire_context);

	private:
		QString device_path;
		bool do_turn_off = false; /* Turn off device after completing the task. */
	};




	class DataSourceGPSDialog : public DataSourceDialog {
		Q_OBJECT
	public:
		DataSourceGPSDialog(const QString & window_title, GPSTransferType xfer, bool xfer_all, QWidget * parent = NULL);
		~DataSourceGPSDialog();

		AcquireOptions * create_acquire_options(AcquireContext & acquire_context) override;

		void save_transfer_options(void);

		QString get_gps_protocol(void);
		QString get_serial_port(void);

		GPSTransfer transfer{GPSDirection::Upload};


		QComboBox * proto_combo = NULL;
		QComboBox * serial_port_combo = NULL;
		QCheckBox * off_request_b = NULL;

		QLabel * get_tracks_l = NULL;
		QCheckBox * get_tracks_b = NULL;
		QLabel * get_routes_l = NULL;
		QCheckBox * get_routes_b = NULL;
		QLabel * get_waypoints_l = NULL;
		QCheckBox * get_waypoints_b = NULL;

		/* State. */
		int total_count = 0;
		int count = 0;
	};



	class DatasourceGPSProgress : public DataSourceDialog {
		Q_OBJECT
	public:
		DatasourceGPSProgress(const QString & window_title, QWidget * parent = NULL);
		~DatasourceGPSProgress();

	private slots:

	public:
		QLabel *gps_label = NULL;
		QLabel *ver_label = NULL;
		QLabel *id_label = NULL;
		QLabel *wp_label = NULL;
		QLabel *trk_label = NULL;
		QLabel *rte_label = NULL;
		QLabel *progress_label = NULL;
		GPSTransferType progress_type;

		/* State. */
		int total_count = 0;
		int count = 0;
		/* Know which way xfer is so xfer setting types are only stored for download. */
		//GPSDirection direction;
	private:

	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DATASOURCE_GPS_H_ */
