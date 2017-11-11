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
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vector>
#include <cstdlib>
#include <cstring>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>

#include <glib/gstdio.h>
#include <glib/gprintf.h>

#include "layer_gps.h"
#include "datasource_gps.h"
#include "babel.h"
#include "gpx.h"
#include "acquire.h"
#include "application_state.h"
#include "util.h"




using namespace SlavGPS;




std::vector<BabelDevice *> a_babel_device_list;

static bool gps_acquire_in_progress = false;

static int last_active = -1;

static void * datasource_gps_init_func(acq_vik_t *avt);
static ProcessOptions * datasource_gps_get_process_options(void * user_data, void * not_used, const char *not_used2, const char *not_used3);
static void datasource_gps_cleanup(void * user_data);
static void datasource_gps_progress(BabelProgressCode c, void * data, AcquireProcess * acquiring);
static void datasource_gps_add_setup_widgets(GtkWidget *dialog, Viewport * viewport, void * user_data);
static void datasource_gps_add_progress_widgets(GtkWidget *dialog, void * user_data);
static void datasource_gps_off(void * add_widgets_data_not_used, QString & babel_args, QString & file_path);



VikDataSourceInterface vik_datasource_gps_interface = {
	N_("Acquire from GPS"),
	N_("Acquired from GPS"),
	DatasourceMode::AUTO_LAYER_MANAGEMENT,
	DatasourceInputtype::NONE,
	true,
	true,
	true,

	(DataSourceInternalDialog)              NULL,
	(VikDataSourceInitFunc)		        datasource_gps_init_func,
	(VikDataSourceCheckExistenceFunc)	NULL,
	(VikDataSourceAddSetupWidgetsFunc)	datasource_gps_add_setup_widgets,
	(VikDataSourceGetProcessOptionsFunc)    datasource_gps_get_process_options,
	(VikDataSourceProcessFunc)              a_babel_convert_from,
	(VikDataSourceProgressFunc)		datasource_gps_progress,
	(VikDataSourceAddProgressWidgetsFunc)	datasource_gps_add_progress_widgets,
	(VikDataSourceCleanupFunc)		datasource_gps_cleanup,
	(VikDataSourceOffFunc)                  datasource_gps_off,

	NULL,
	0,
	NULL,
	NULL,
	0
};




/*********************************************************
 * Definitions and routines for acquiring data from GPS.
 *********************************************************/




#define VIK_SETTINGS_GPS_GET_TRACKS "gps_download_tracks"
#define VIK_SETTINGS_GPS_GET_ROUTES "gps_download_routes"
#define VIK_SETTINGS_GPS_GET_WAYPOINTS "gps_download_waypoints"
#define VIK_SETTINGS_GPS_PROTOCOL "gps_protocol"
#define VIK_SETTINGS_GPS_PORT "gps_port"
#define VIK_SETTINGS_GPS_POWER_OFF "gps_power_off"




static void * datasource_gps_init_func(acq_vik_t *avt)
{
	DatasourceGPSProgress * gps_dialog = new DatasourceGPSProgress(NULL);
	gps_dialog->direction = GPSDirection::DOWN;
	return gps_dialog;
}




DatasourceGPSSetup::~DatasourceGPSSetup()
{
	gps_acquire_in_progress = false;
}




/**
   Method to get the communication protocol of the GPS device from the widget structure.
 */
QString DatasourceGPSSetup::get_protocol(void)
{
	last_active = this->proto_combo->currentIndex();

	if (a_babel_device_list.size()) {
		const QString protocol = a_babel_device_list[last_active]->name;
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
	if (this->direction == GPSDirection::DOWN) {
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
	if (this->direction == GPSDirection::DOWN) {
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
	if (this->direction == GPSDirection::DOWN) {
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




static ProcessOptions * datasource_gps_get_process_options(void * user_data, void * not_used, const char *not_used2, const char *not_used3)
{
	ProcessOptions * po = new ProcessOptions();
	DatasourceGPSSetup * gps_dialog = (DatasourceGPSSetup *) user_data;

	if (gps_acquire_in_progress) {
		po->babel_args = "";
		po->input_file_name = "";
	}

	gps_acquire_in_progress = true;

	const QString device = gps_dialog->get_protocol();
	const char * tracks = gps_dialog->get_do_tracks() ? "-t" : "";
	const char * routes = gps_dialog->get_do_routes() ? "-r" : "";
	const char * waypoints = gps_dialog->get_do_waypoints() ? "-w" : "";

	po->babel_args = QString("-D 9 %1 %2 %3 -i %4").arg(tracks).arg(routes).arg(waypoints).arg(device);
	po->input_file_name = gps_dialog->get_port();

	qDebug() << "DD: Datasource GPS: Get process options: using Babel args" << po->babel_args << "and input file" << po->input_file_name;

	return po;
}




static void datasource_gps_off(void * user_data, QString & babel_args, QString & file_path)
{
	DatasourceGPSSetup * gps_dialog = (DatasourceGPSSetup *) user_data;

	if (gps_acquire_in_progress) {
		babel_args = "";
		file_path = "";
	}

	/* See if we should turn off the device. */
	if (!gps_dialog->get_do_turn_off()){
		return;
	}

	if (a_babel_device_list.empty()) {
		return;
	}

	last_active = gps_dialog->proto_combo->currentIndex();

	QString device = a_babel_device_list[last_active]->name;
	qDebug() << "II: Datasource GPS: GPS off: last active device:" << device;

	if (device == "garmin") {
		device = "garmin,power_off";
	} else if (device == "navilink") {
		device = "navilink,power_off";
	} else {
		return;
	}

	babel_args = QString("-i %1").arg(device);
	file_path = QString(gps_dialog->serial_port_combo->currentText());
}




static void datasource_gps_cleanup(void * user_data)
{
	delete ((DatasourceGPSSetup *) user_data);
}




static void set_total_count(unsigned int cnt, AcquireProcess * acquiring)
{
	char *s = NULL;
#ifdef K
	gdk_threads_enter();
#endif
	if (acquiring->running) {
		DatasourceGPSProgress * gps_dialog = (DatasourceGPSProgress *) acquiring->user_data;
		const char *tmp_str;
#ifdef K
		switch (gps_dialog->progress_type) {
		case GPSTransferType::WPT:
			tmp_str = ngettext("Downloading %d waypoint...", "Downloading %d waypoints...", cnt);
			gps_dialog->total_count = cnt;
			break;
		case GPSTransferType::TRK:
			tmp_str = ngettext("Downloading %d trackpoint...", "Downloading %d trackpoints...", cnt);
			gps_dialog->total_count = cnt;
			break;
		default: {
			/* Maybe a gpsbabel bug/feature (upto at least v1.4.3 or maybe my Garmin device) but the count always seems x2 too many for routepoints. */
			int mycnt = (cnt / 2) + 1;
			tmp_str = ngettext("Downloading %d routepoint...", "Downloading %d routepoints...", mycnt);
			gps_dialog->total_count = mycnt;
			break;
		}
		}
		s = g_strdup_printf(tmp_str, cnt);
		gps_dialog->progress_label->setText(QObject::tr(s));
		gtk_widget_show(gps_dialog->progress_label);
#endif
	}
	free(s);
	s = NULL;
#ifdef K
	gdk_threads_leave();
#endif
}




static void set_current_count(int cnt, AcquireProcess * acquiring)
{
	char *s = NULL;
#ifdef K
	gdk_threads_enter();
#endif
	if (acquiring->running) {
		DatasourceGPSProgress * gps_dialog = (DatasourceGPSProgress *) acquiring->user_data;

		if (cnt < gps_dialog->total_count) {
			switch (gps_dialog->progress_type) {
			case GPSTransferType::WPT:
				s = g_strdup_printf(_("Downloaded %d out of %d %s..."), cnt, gps_dialog->total_count, "waypoints");
				break;
			case GPSTransferType::TRK:
				s = g_strdup_printf(_("Downloaded %d out of %d %s..."), cnt, gps_dialog->total_count, "trackpoints");
				break;
			default:
				s = g_strdup_printf(_("Downloaded %d out of %d %s..."), cnt, gps_dialog->total_count, "routepoints");
				break;
			}
		} else {
			switch (gps_dialog->progress_type) {
			case GPSTransferType::WPT:
				s = g_strdup_printf(_("Downloaded %d %s."), cnt, "waypoints");
				break;
			case GPSTransferType::TRK:
				s = g_strdup_printf(_("Downloaded %d %s."), cnt, "trackpoints");
				break;
			default:
				s = g_strdup_printf(_("Downloaded %d %s."), cnt, "routepoints");
				break;
			}
		}
		gps_dialog->progress_label->setText(s);
	}
	free(s);
	s = NULL;
#ifdef K
	gdk_threads_leave();
#endif
}




static void set_gps_info(const char * info, AcquireProcess * acquiring)
{
#ifdef K
	gdk_threads_enter();
#endif
	if (acquiring->running) {
		((DatasourceGPSProgress *) acquiring->user_data)->gps_label->setText(QObject::tr("GPS Device: %s").arg(info));
	}
#ifdef K
	gdk_threads_leave();
#endif
}




/*
 * This routine relies on gpsbabel's diagnostic output to display the progress information.
 * These outputs differ when different GPS devices are used, so we will need to test
 * them on several and add the corresponding support.
 */
static void datasource_gps_progress(BabelProgressCode c, void * data, AcquireProcess * acquiring)
{
	char *line;
	DatasourceGPSProgress * gps_dialog = (DatasourceGPSProgress *) acquiring->user_data;

	switch(c) {
	case BABEL_DIAG_OUTPUT:
		line = (char *)data;
#ifdef K
		gdk_threads_enter();
#endif
		if (acquiring->running) {
			acquiring->status->setText(QObject::tr("Status: Working..."));
		}
#ifdef K
		gdk_threads_leave();
#endif

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

		if (strstr(line, "PRDDAT")) { /* kamilTODO: there is a very similar code in process_line_for_gps_info() */
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
	case BABEL_DONE:
		break;
	default:
		break;
	}
}




static int find_entry = -1;
static int wanted_entry = -1;

static void find_protocol(BabelDevice * device, const QString & protocol)
{
	find_entry++;

	if (device->name == protocol) {
		wanted_entry = find_entry;
	}
}




static void datasource_gps_add_setup_widgets(GtkWidget * dialog, Viewport * viewport, void * user_data)
{
	DatasourceGPSSetup * gps_dialog = (DatasourceGPSSetup *) user_data;

	{
		gps_dialog->proto_label = new QLabel(QObject::tr("GPS Protocol:"));
		gps_dialog->proto_combo = new QComboBox();
		for (auto iter = a_babel_device_list.begin(); iter != a_babel_device_list.end(); iter++) {
			gps_dialog->proto_combo->addItem((*iter)->label);
		}

		if (last_active < 0) {
			find_entry = -1;
			wanted_entry = -1;
			QString protocol;
			if (ApplicationState::get_string (VIK_SETTINGS_GPS_PROTOCOL, protocol)) {
				/* Use setting. */
				if (!protocol.isEmpty()) {
					for (auto iter = a_babel_device_list.begin(); iter != a_babel_device_list.end(); iter++) {
						find_protocol(*iter, protocol);
					}
				}
			} else {
				/* Attempt to maintain default to Garmin devices (assumed most popular/numerous device). */
				for (auto iter = a_babel_device_list.begin(); iter != a_babel_device_list.end(); iter++) {
					find_protocol(*iter, "garmin");
				}
			}
			/* If not found set it to the first entry, otherwise use the entry. */
			last_active = (wanted_entry < 0) ? 0 : wanted_entry;
		}

		gps_dialog->proto_combo->setCurrentIndex(last_active);
#ifdef K
		g_object_ref(gps_dialog->proto_combo);
#endif

		gps_dialog->grid->addWidget(gps_dialog->proto_label, 0, 0);
		gps_dialog->grid->addWidget(gps_dialog->proto_combo, 0, 1);
	}

	{


		gps_dialog->serial_port_label = new QLabel(QObject::tr("Serial Port:"));
		gps_dialog->serial_port_combo = new QComboBox();


		/* Value from the settings is promoted to the top. */
		QString preferred_gps_port;
		if (ApplicationState::get_string(VIK_SETTINGS_GPS_PORT, preferred_gps_port)) {
			/* Use setting if available. */
			if (!preferred_gps_port.isEmpty()) {
#ifndef WINDOWS
				if (preferred_gps_port.left(6) == "/dev/tty") {
					if (access(preferred_gps_port.toUtf8().constData(), R_OK) == 0) {
						gps_dialog->serial_port_combo->addItem(preferred_gps_port);
					}
				} else
#endif
					gps_dialog->serial_port_combo->addItem(preferred_gps_port);
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
				gps_dialog->serial_port_combo->addItem(port);
			}
		}


		gps_dialog->serial_port_combo->setCurrentIndex(0);
#ifdef K
		g_object_ref(gps_dialog->serial_port_combo);
#endif
		gps_dialog->grid->addWidget(gps_dialog->serial_port_label, 1, 0);
		gps_dialog->grid->addWidget(gps_dialog->serial_port_combo, 1, 1);
	}


	{
		gps_dialog->off_request_l = new QLabel(QObject::tr("Turn Off After Transfer\n(Garmin/NAViLink Only)"));
		gps_dialog->off_request_b = new QCheckBox();
		bool power_off;
		if (!ApplicationState::get_boolean(VIK_SETTINGS_GPS_POWER_OFF, &power_off)) {
			power_off = false;
		}
		gps_dialog->off_request_b->setChecked(power_off);

		gps_dialog->grid->addWidget(gps_dialog->off_request_l, 2, 0);
		gps_dialog->grid->addWidget(gps_dialog->off_request_b, 2, 1);
	}


	{
		gps_dialog->get_tracks_l = new QLabel(QObject::tr("Tracks:"));
		gps_dialog->get_tracks_b = new QCheckBox();
		bool get_tracks;
		if (!ApplicationState::get_boolean(VIK_SETTINGS_GPS_GET_TRACKS, &get_tracks)) {
			get_tracks = true;
		}
		gps_dialog->get_tracks_b->setChecked(get_tracks);

		gps_dialog->grid->addWidget(gps_dialog->get_tracks_l, 3, 0);
		gps_dialog->grid->addWidget(gps_dialog->get_tracks_b, 3, 1);
	}

	{
		gps_dialog->get_routes_l = new QLabel(QObject::tr("Routes:"));
		gps_dialog->get_routes_b = new QCheckBox();
		bool get_routes;
		if (!ApplicationState::get_boolean(VIK_SETTINGS_GPS_GET_ROUTES, &get_routes)) {
			get_routes = false;
		}
		gps_dialog->get_routes_b->setChecked(get_routes);

		gps_dialog->grid->addWidget(gps_dialog->get_routes_l, 4, 0);
		gps_dialog->grid->addWidget(gps_dialog->get_routes_b, 4, 1);
	}

	{
		gps_dialog->get_waypoints_l = new QLabel(QObject::tr("Waypoints:"));
		gps_dialog->get_waypoints_b = new QCheckBox();
		bool get_waypoints;
		if (!ApplicationState::get_boolean(VIK_SETTINGS_GPS_GET_WAYPOINTS, &get_waypoints)) {
			get_waypoints = true;
		}
		gps_dialog->get_waypoints_b->setChecked(get_waypoints);

		gps_dialog->grid->addWidget(gps_dialog->get_waypoints_l, 5, 0);
		gps_dialog->grid->addWidget(gps_dialog->get_waypoints_b, 5, 1);
	}
}




/**
   @xfer: The default type of items enabled for transfer, others disabled
   @xfer_all: When specified all items are enabled for transfer
*/
DatasourceGPSSetup::DatasourceGPSSetup(GPSTransferType xfer, bool xfer_all, QWidget * parent)
{
	this->direction = GPSDirection::UP;
	this->setWindowTitle(QObject::tr("GPS Upload"));

	this->vbox = new QVBoxLayout;
	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);


	this->grid = new QGridLayout();
	this->vbox->addLayout(this->grid);


	this->button_box = new QDialogButtonBox();
	this->button_box->addButton(QDialogButtonBox::Ok);
	this->button_box->addButton(QDialogButtonBox::Cancel);
	QObject::connect(this->button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
	QObject::connect(this->button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
	this->vbox->addWidget(this->button_box);

	datasource_gps_add_setup_widgets(NULL, NULL, this);


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




void datasource_gps_add_progress_widgets(GtkWidget *dialog, void * user_data)
{
	DatasourceGPSProgress * gps_dialog = (DatasourceGPSProgress *) user_data;

	QLabel * gpslabel = new QLabel(QObject::tr("GPS device: N/A"));
	QLabel * verlabel = new QLabel("");
	QLabel * idlabel = new QLabel("");
	QLabel * wplabel = new QLabel("");
	QLabel * trklabel = new QLabel("");
	QLabel * rtelabel = new QLabel("");

#ifdef K
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
}



DatasourceGPSProgress::DatasourceGPSProgress(QWidget * parent)
{
}

DatasourceGPSProgress::~DatasourceGPSProgress()
{
}
