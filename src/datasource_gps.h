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




#include "acquire.h"
#include "babel.h"
#include "datasource.h"
#include "layer_gps.h"




namespace SlavGPS {




	class DataSourceGPS : public DataSourceBabel {
	public:
		DataSourceGPS();
		~DataSourceGPS() {};

		int run_config_dialog(AcquireProcess * acquire_context);

		/* FIXME: these are most probably unused after changes in acquire. Make them used again. */
		void off(void * user_data, QString & babel_args, QString & file_path);
		void progress_func(AcquireProgressCode code, void * data, AcquireProcess * acquiring);
		DataSourceDialog * create_progress_dialog(void * user_data);
	};





	class DatasourceGPSSetup : public DataSourceDialog {
		Q_OBJECT
	public:
		DatasourceGPSSetup(const QString & window_title, GPSTransferType xfer, bool xfer_all, QWidget * parent = NULL);
		~DatasourceGPSSetup();

		BabelOptions * get_acquire_options_none(void);

		QString get_protocol(void);
		QString get_port(void);
		bool get_do_tracks(void);
		bool get_do_routes(void);
		bool get_do_waypoints(void);
		bool get_do_turn_off(void);

	private slots:


	public:
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
		/* Know which way xfer is so xfer setting types are only stored for download. */
		GPSDirection direction;


	private:

	};



	class DatasourceGPSProgress : public DataSourceDialog {
		Q_OBJECT
	public:
		DatasourceGPSProgress(const QString & window_title, QWidget * parent = NULL);
		~DatasourceGPSProgress();

		BabelOptions * get_acquire_options_none(void) { return NULL; };

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
		GPSDirection direction;
	private:

	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_DATASOURCE_GPS_H_ */
