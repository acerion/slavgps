/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2006, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2012-2015, Rob Norris <rw_norris@hotmail.com>
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




#include <vector>
#include <cstdlib>
#include <cstring>
#include <cassert>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif




#include <QCheckBox>
#include <QComboBox>
#include <QDebug>




#include "layer_gps.h"
#include "datasource_gps.h"
#include "babel.h"
#include "gpx.h"
#include "acquire.h"
#include "application_state.h"
#include "util.h"
#include "datasource.h"




using namespace SlavGPS;




#define SG_MODULE "DataSource GPS"




static bool gps_acquire_in_progress = false;

#define INVALID_ENTRY_INDEX -1

/* Index of the last device selected. */
static int g_last_device_index = INVALID_ENTRY_INDEX;

static DataSourceDialog * datasource_gps_setup_dialog_add_widgets(DatasourceGPSSetup * setup_dialog);
static int find_initial_device_index(void);




DataSourceGPS::DataSourceGPS()
{
	this->window_title = QObject::tr("Acquire from GPS");
	this->layer_title = QObject::tr("Acquired from GPS");
	this->mode = DataSourceMode::AutoLayerManagement;
	this->input_type = DataSourceInputType::None;
	this->autoview = true;
	this->keep_dialog_open = true; /* true = keep dialog open after success. */
}




/*********************************************************
 * Definitions and routines for acquiring data from GPS.
 *********************************************************/




#define VIK_SETTINGS_GPS_GET_TRACKS "gps_download_tracks"
#define VIK_SETTINGS_GPS_GET_ROUTES "gps_download_routes"
#define VIK_SETTINGS_GPS_GET_WAYPOINTS "gps_download_waypoints"
#define VIK_SETTINGS_GPS_PROTOCOL "gps_protocol"
#define VIK_SETTINGS_GPS_PORT "gps_port"
#define VIK_SETTINGS_GPS_POWER_OFF "gps_power_off"




DatasourceGPSSetup::~DatasourceGPSSetup()
{
	gps_acquire_in_progress = false;
}




/**
   Method to get the communication protocol of the GPS device from the widget structure.
 */
QString DatasourceGPSSetup::get_protocol(void)
{
	g_last_device_index = this->proto_combo->currentIndex();

	if (Babel::devices.size()) {
		const QString protocol = Babel::devices[g_last_device_index]->identifier;
		qDebug() << "II: Datasource GPS: get protocol: " << protocol;
		ApplicationState::set_string(VIK_SETTINGS_GPS_PROTOCOL, protocol);
		return protocol;
	}

	return "";
}




/**
   Method to get the descriptor from the widget structure.
   "Everything is a file".
   Could actually be normal file or a serial port.
*/
QString DatasourceGPSSetup::get_port(void)
{
	const QString descriptor = this->serial_port_combo->currentText();
	ApplicationState::set_string(VIK_SETTINGS_GPS_PORT, descriptor);
	return descriptor;
}




/**
   Method to get the track handling behaviour from the widget structure.
*/
bool DatasourceGPSSetup::get_do_tracks(void)
{
	bool get_tracks = this->get_tracks_b->isChecked();
	if (this->direction == GPSDirection::Down) {
		ApplicationState::set_boolean(VIK_SETTINGS_GPS_GET_TRACKS, get_tracks);
	}
	return get_tracks;
}




/**
   Method to get the route handling behaviour from the widget structure.
*/
bool DatasourceGPSSetup::get_do_routes(void)
{
	bool get_routes = this->get_routes_b->isChecked();
	if (this->direction == GPSDirection::Down) {
		ApplicationState::set_boolean(VIK_SETTINGS_GPS_GET_ROUTES, get_routes);
	}
	return get_routes;
}




/**
   Method to get the waypoint handling behaviour from the widget structure.
*/
bool DatasourceGPSSetup::get_do_waypoints(void)
{
	bool get_waypoints = this->get_waypoints_b->isChecked();
	if (this->direction == GPSDirection::Down) {
		ApplicationState::set_boolean(VIK_SETTINGS_GPS_GET_WAYPOINTS, get_waypoints);
	}
	return get_waypoints;
}




/**
   Method to get the off behaviour from the widget structure.
*/
bool DatasourceGPSSetup::get_do_turn_off(void)
{
	bool power_off = this->off_request_b->isChecked();
	ApplicationState::set_boolean(VIK_SETTINGS_GPS_POWER_OFF, power_off);
	return power_off;
}




AcquireOptions * DatasourceGPSSetup::create_acquire_options(AcquireContext * acquire_context)
{
	gps_acquire_in_progress = true;

	AcquireOptions * babel_options = new AcquireOptions();

	babel_options->babel_process = new BabelProcess();
	babel_options->babel_process->set_options(QString("-D 9 %1").arg(BabelProcess::get_trw_string(this->get_do_tracks(), this->get_do_routes(), this->get_do_waypoints())));
	babel_options->babel_process->set_input(this->get_protocol(), this->get_port());

	return babel_options;
}




static void set_total_count(unsigned int cnt, AcquireWorker & getter)
{
#ifdef K_TODO
	if (acquiring->acquire_is_running) {

		DatasourceGPSProgress * gps_dialog = (DatasourceGPSProgress *) acquiring->parent_data_source_dialog;

		QString msg;

		switch (gps_dialog->progress_type) {
		case GPSTransferType::WPT:
			msg = QObject::tr("Downloading %n waypoints...", "", cnt);
			gps_dialog->total_count = cnt;
			break;
		case GPSTransferType::TRK:
			msg = QObject::tr("Downloading %n trackpoints...", "", cnt);
			gps_dialog->total_count = cnt;
			break;
		default: {
			/* Maybe a gpsbabel bug/feature (upto at least v1.4.3 or maybe my Garmin device) but the count always seems x2 too many for routepoints. */
			int mycnt = (cnt / 2) + 1;
			msg = QObject::tr("Downloading %n routepoints...", "", mycnt);
			gps_dialog->total_count = mycnt;
			break;
		}
		}
		gps_dialog->progress_label->setText(msg);
	}
#endif
}




/* Compare this function with GPSSession::set_current_count(int cnt) */
static void set_current_count(int cnt, AcquireWorker * getter)
{
#ifdef K_TODO
	if (acquiring->acquire_is_running) {
		DatasourceGPSProgress * gps_dialog = (DatasourceGPSProgress *) acquiring->parent_data_source_dialog;

		if (cnt < gps_dialog->total_count) {
			QString fmt;
			switch (gps_dialog->progress_type) {
			case GPSTransferType::WPT:
				fmt = QObject::tr("Downloaded %1 out of %2 waypoints...", "", gps_dialog->total_count);
				break;
			case GPSTransferType::TRK:
				fmt = QObject::tr("Downloaded %1 out of %2 trackpoints...", "", gps_dialog->total_count);
				break;
			default:
				fmt = QObject::tr("Downloaded %1 out of %2 routepoints...", "", gps_dialog->total_count);
				break;
			}
			gps_dialog->progress_label->setText(QString(fmt).arg(cnt).arg(gps_dialog->total_count));

		} else {
			QString s;
			switch (gps_dialog->progress_type) {
			case GPSTransferType::WPT:
				s = QObject::tr("Downloaded %1 waypoints.").arg(cnt);
				break;
			case GPSTransferType::TRK:
				s = QObject::tr("Downloaded %1 trackpoints.").arg(cnt);
				break;
			default:
				s = QObject::tr("Downloaded %1 routepoints.").arg(cnt);
				break;
			}
			gps_dialog->progress_label->setText(s);
		}
	}
#endif
}




static void set_gps_info(const char * info, AcquireWorker * getter)
{
#ifdef K_TODO
	if (acquiring->acquire_is_running) {
		((DatasourceGPSProgress *) acquiring->parent_data_source_dialog)->gps_label->setText(QObject::tr("GPS Device: %1").arg(info));
	}
#endif
}




/*
 * This routine relies on gpsbabel's diagnostic output to display the progress information.
 * These outputs differ when different GPS devices are used, so we will need to test
 * them on several and add the corresponding support.
 */
void DataSourceGPS::progress_func(AcquireProgressCode code, void * data, AcquireContext * acquire_context)
{
	char *line;
#ifdef K_TODO
	DatasourceGPSProgress * gps_dialog = (DatasourceGPSProgress *) getter->parent_data_source_dialog;

	switch (code) {
	case AcquireProgressCode::DiagOutput:
		line = (char *)data;

		if (getter->acquire_is_running) {
			getter->acquire_getter_progress_dialog->set_headline(QObject::tr("Status: Working..."));
		}

		/* Tells us the type of items that will follow. */
		if (strstr(line, "Xfer Wpt")) {
			gps_dialog->progress_label = gps_dialog->wp_label;
			gps_dialog->progress_type = GPSTransferType::WPT;
		}
		if (strstr(line, "Xfer Trk")) {
			gps_dialog->progress_label = gps_dialog->trk_label;
			gps_dialog->progress_type = GPSTransferType::TRK;
		}
		if (strstr(line, "Xfer Rte")) {
			gps_dialog->progress_label = gps_dialog->rte_label;
			gps_dialog->progress_type = GPSTransferType::RTE;
		}

		if (strstr(line, "PRDDAT")) { /* TODO_2_LATER: there is a very similar code in process_line_for_gps_info() */
			char **tokens = g_strsplit(line, " ", 0);
			char info[128];
		        size_t ilen = 0;
			int i;
			int n_tokens = 0;

			while (tokens[n_tokens]) {
				n_tokens++;
			}

			if (n_tokens > 8) {
				for (i=8; tokens[i] && ilen < sizeof(info)-2 && strcmp(tokens[i], "00"); i++) {
					unsigned int ch;
					sscanf(tokens[i], "%x", &ch);
					info[ilen++] = ch;
				}
				info[ilen++] = 0;
				set_gps_info(info, acquiring);
			}
			g_strfreev(tokens);
		}
		/* e.g: "Unit:\teTrex Legend HCx Software Version 2.90\n" */
		if (strstr(line, "Unit:")) {
			char **tokens = g_strsplit(line, "\t", 0);
			int n_tokens = 0;
			while (tokens[n_tokens]) {
				n_tokens++;
			}

			if (n_tokens > 1) {
				set_gps_info(tokens[1], acquiring);
			}
			g_strfreev(tokens);
		}
		/* Tells us how many items there will be. */
		if (strstr(line, "RECORD")) {
			unsigned int lsb, msb, cnt;

			if (strlen(line) > 20) {
				sscanf(line+17, "%x", &lsb);
				sscanf(line+20, "%x", &msb);
				cnt = lsb + msb * 256;
				set_total_count(cnt, acquiring);
				gps_dialog->count = 0;
			}
		}
		if (strstr(line, "WPTDAT") || strstr(line, "TRKHDR") || strstr(line, "TRKDAT") || strstr(line, "RTEHDR") || strstr(line, "RTEWPT")) {
			gps_dialog->count++;
			set_current_count(gps_dialog->count, acquiring);
		}
		break;
	case AcquireProgressCode::Completed:
		break;
	default:
		break;
	}
#endif
}




int DataSourceGPS::run_config_dialog(AcquireContext * acquire_context)
{
	/* This function will be created for downloading data from
	   GPS, so build the dialog with all checkboxes available and
	   checked - hence second argument to constructor is
	   "true". */
	GPSTransferType xfer = GPSTransferType::WPT; /* This doesn't really matter much because second arg to constructor is 'true'. */
	DatasourceGPSSetup config_dialog(this->window_title, xfer, true, NULL);

	const int answer = config_dialog.exec();
	if (answer == QDialog::Accepted) {
		this->acquire_options = config_dialog.create_acquire_options(acquire_context);
		this->download_options = new DownloadOptions; /* With default values. */

		this->device_path = config_dialog.serial_port_combo->currentText();
		this->do_turn_off = config_dialog.get_do_turn_off();
		g_last_device_index = config_dialog.proto_combo->currentIndex();
	}

	return answer;

}




static DataSourceDialog * datasource_gps_setup_dialog_add_widgets(DatasourceGPSSetup * setup_dialog)
{
	{
		setup_dialog->proto_combo = new QComboBox();
		for (auto iter = Babel::devices.begin(); iter != Babel::devices.end(); iter++) {
			setup_dialog->proto_combo->addItem((*iter)->label);
		}

		if (g_last_device_index == INVALID_ENTRY_INDEX) {
			g_last_device_index = find_initial_device_index();
		}
		/* After this the index is valid. */

		setup_dialog->proto_combo->setCurrentIndex(g_last_device_index);
#ifdef K_FIXME_RESTORE
		g_object_ref(setup_dialog->proto_combo);
#endif

		setup_dialog->grid->addWidget(new QLabel(QObject::tr("GPS Protocol:")), 0, 0);
		setup_dialog->grid->addWidget(setup_dialog->proto_combo, 0, 1);
	}

	{


		setup_dialog->serial_port_combo = new QComboBox();


		/* Value from the settings is promoted to the top. */
		QString preferred_gps_port;
		if (ApplicationState::get_string(VIK_SETTINGS_GPS_PORT, preferred_gps_port)) {
			/* Use setting if available. */
			if (!preferred_gps_port.isEmpty()) {
#ifndef WINDOWS
				if (preferred_gps_port.left(6) == "/dev/tty") {
					if (access(preferred_gps_port.toUtf8().constData(), R_OK) == 0) {
						setup_dialog->serial_port_combo->addItem(preferred_gps_port);
					}
				} else
#endif
					setup_dialog->serial_port_combo->addItem(preferred_gps_port);
			}
		}

		/* Note avoid appending the port selected from the settings. */
		/* Here just try to see if the device is available which gets passed onto gpsbabel.
		   List USB devices first as these will generally only be present if autogenerated by udev or similar.
		   User is still able to set their own free text entry. */
#ifdef WINDOWS
		const QStringList gps_ports = { "com1", "usb:" };
#else
		const QStringList gps_ports = { "/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyS0", "/dev/ttyS0", "/dev/ttyS1" };
#endif
		for (int i = 0; i < gps_ports.size(); i++) {
			const QString port = gps_ports.at(i);

			if (!preferred_gps_port.isEmpty() && port == preferred_gps_port) {
				/* This port has already been added as preferred port. */
				continue;
			}

			if (access(port.toUtf8().constData(), R_OK) == 0) {
				setup_dialog->serial_port_combo->addItem(port);
			}
		}


		setup_dialog->serial_port_combo->setCurrentIndex(0);
#ifdef K_FIXME_RESTORE
		g_object_ref(setup_dialog->serial_port_combo);
#endif
		setup_dialog->grid->addWidget(new QLabel(QObject::tr("Serial Port:")), 1, 0);
		setup_dialog->grid->addWidget(setup_dialog->serial_port_combo, 1, 1);
	}


	{
		setup_dialog->off_request_b = new QCheckBox();
		bool power_off;
		if (!ApplicationState::get_boolean(VIK_SETTINGS_GPS_POWER_OFF, &power_off)) {
			power_off = false;
		}
		setup_dialog->off_request_b->setChecked(power_off);

		setup_dialog->grid->addWidget(new QLabel(QObject::tr("Turn Off After Transfer\n(Garmin/NAViLink Only)")), 2, 0);
		setup_dialog->grid->addWidget(setup_dialog->off_request_b, 2, 1);
	}


	{
		setup_dialog->get_tracks_l = new QLabel(QObject::tr("Tracks:"));
		setup_dialog->get_tracks_b = new QCheckBox();
		bool get_tracks;
		if (!ApplicationState::get_boolean(VIK_SETTINGS_GPS_GET_TRACKS, &get_tracks)) {
			get_tracks = true;
		}
		setup_dialog->get_tracks_b->setChecked(get_tracks);

		setup_dialog->grid->addWidget(setup_dialog->get_tracks_l, 3, 0);
		setup_dialog->grid->addWidget(setup_dialog->get_tracks_b, 3, 1);
	}

	{
		setup_dialog->get_routes_l = new QLabel(QObject::tr("Routes:"));
		setup_dialog->get_routes_b = new QCheckBox();
		bool get_routes;
		if (!ApplicationState::get_boolean(VIK_SETTINGS_GPS_GET_ROUTES, &get_routes)) {
			get_routes = false;
		}
		setup_dialog->get_routes_b->setChecked(get_routes);

		setup_dialog->grid->addWidget(setup_dialog->get_routes_l, 4, 0);
		setup_dialog->grid->addWidget(setup_dialog->get_routes_b, 4, 1);
	}

	{
		setup_dialog->get_waypoints_l = new QLabel(QObject::tr("Waypoints:"));
		setup_dialog->get_waypoints_b = new QCheckBox();
		bool get_waypoints;
		if (!ApplicationState::get_boolean(VIK_SETTINGS_GPS_GET_WAYPOINTS, &get_waypoints)) {
			get_waypoints = true;
		}
		setup_dialog->get_waypoints_b->setChecked(get_waypoints);

		setup_dialog->grid->addWidget(setup_dialog->get_waypoints_l, 5, 0);
		setup_dialog->grid->addWidget(setup_dialog->get_waypoints_b, 5, 1);
	}

	return setup_dialog;
}




/**
   @xfer: The default type of items enabled for transfer, others disabled
   @xfer_all: When specified all items are enabled for transfer
*/
DatasourceGPSSetup::DatasourceGPSSetup(const QString & window_title, GPSTransferType xfer, bool xfer_all, QWidget * parent) : DataSourceDialog(window_title)
{
	this->direction = GPSDirection::Up;
	this->setWindowTitle(QObject::tr("GPS Upload"));

	datasource_gps_setup_dialog_add_widgets(this);

	bool do_waypoints = xfer_all;
	bool do_tracks = xfer_all;
	bool do_routes = xfer_all;

	/* Selectively turn bits on. */
	if (!xfer_all) {
		switch (xfer) {
		case GPSTransferType::WPT:
			do_waypoints = true;
			break;
		case GPSTransferType::RTE:
			do_routes = true;
			break;
		default:
			do_tracks = true;
			break;
		}
	}

	/* Apply. */
	this->get_tracks_b->setChecked(do_tracks);
	this->get_tracks_l->setEnabled(do_tracks);
	this->get_tracks_b->setEnabled(do_tracks);

	this->get_routes_b->setChecked(do_routes);
	this->get_routes_l->setEnabled(do_routes);
	this->get_routes_b->setEnabled(do_routes);

	this->get_waypoints_b->setChecked(do_waypoints);
	this->get_waypoints_l->setEnabled(do_waypoints);
	this->get_waypoints_b->setEnabled(do_waypoints);
}




AcquireProgressDialog * DataSourceGPS::create_progress_dialog(void * user_data)
{
	AcquireProgressDialog * progress_dialog = NULL;
	DatasourceGPSProgress * gps_dialog = (DatasourceGPSProgress *) user_data;

	QLabel * gpslabel = new QLabel(QObject::tr("GPS device: N/A"));
	QLabel * verlabel = new QLabel("");
	QLabel * idlabel = new QLabel("");
	QLabel * wplabel = new QLabel("");
	QLabel * trklabel = new QLabel("");
	QLabel * rtelabel = new QLabel("");

#ifdef K_FIXME_RESTORE
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), gpslabel, false, false, 5);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), wplabel, false, false, 5);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), trklabel, false, false, 5);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), rtelabel, false, false, 5);

	gtk_widget_show_all(dialog);

	gps_dialog->gps_label = gpslabel;
	gps_dialog->id_label = idlabel;
	gps_dialog->ver_label = verlabel;
	gps_dialog->progress_label = w_gps->wp_label = wplabel;
	gps_dialog->trk_label = trklabel;
	gps_dialog->rte_label = rtelabel;
	gps_dialog->total_count = -1;
#endif

	return progress_dialog;
}



DatasourceGPSProgress::DatasourceGPSProgress(const QString & window_title, QWidget * parent) : DataSourceDialog(window_title)
{
}

DatasourceGPSProgress::~DatasourceGPSProgress()
{
}




int find_initial_device_index(void)
{
	QString protocol;
	if (ApplicationState::get_string (VIK_SETTINGS_GPS_PROTOCOL, protocol)) {
		/* Use setting. */
	} else {
		/* Attempt to maintain default to Garmin devices (assumed most popular/numerous device). */
		protocol = "garmin";
	}

	int entry_index = INVALID_ENTRY_INDEX;
	bool found = false;
	if (!protocol.isEmpty()) {
		for (auto iter = Babel::devices.begin(); iter != Babel::devices.end(); iter++) {
			entry_index++;
			if ((*iter)->identifier == protocol) {
				found = true;
				break;
			}
		}
	}

	if (!found) {
		/* First entry in Babel::devices. */
		entry_index = 0;
	}

	return entry_index;
}




sg_ret DataSourceGPS::on_complete(void)
{
	if (!this->do_turn_off) {
		qDebug() << SG_PREFIX_I << "Not turning off device, 'turn off' option not selected";
		return sg_ret::ok;
	}

	if (gps_acquire_in_progress) {
		qDebug() << SG_PREFIX_W << "Not turning off device, acquire in progress";
		return sg_ret::err;
	}

	if (Babel::devices.empty()) {
		qDebug() << SG_PREFIX_W << "Not turning off device, no supported devices";
		return sg_ret::err;
	}


	const QString command = "power_off";
	const QString device = Babel::devices[g_last_device_index]->identifier;
	if (device != "garmin" && device != "navilink") {
		qDebug() << SG_PREFIX_W << "Unrecognized last active device" << device;
		sg_ret::err;
	}
	qDebug() << SG_PREFIX_I << "Last active device:" << device;


	const QString babel_args = QString("-i %1,%2").arg(device).arg(command);
	BabelTurnOffDevice turn_off(this->device_path, babel_args);
	turn_off.run_process();

	return sg_ret::ok;
}
