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

/* Widgets in setup dialog specific to GPS. */
/* Widgets in progress dialog specific to GPS. */
/* Also counts needed for progress. */
class GPSData {
public:
	/* Setup dialog. */
	QLabel * proto_l = NULL;
	QComboBox * proto_combo = NULL;
	QLabel *ser_l = NULL;
	QComboBox * ser_combo = NULL;
	QLabel *off_request_l = NULL;
	QCheckBox *off_request_b = NULL;
	QLabel *get_tracks_l = NULL;
	QCheckBox *get_tracks_b = NULL;
	QLabel *get_routes_l = NULL;
	QCheckBox *get_routes_b = NULL;
	QLabel *get_waypoints_l = NULL;
	QCheckBox *get_waypoints_b = NULL;

	/* Progress dialog. */
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
} ;




#define VIK_SETTINGS_GPS_GET_TRACKS "gps_download_tracks"
#define VIK_SETTINGS_GPS_GET_ROUTES "gps_download_routes"
#define VIK_SETTINGS_GPS_GET_WAYPOINTS "gps_download_waypoints"
#define VIK_SETTINGS_GPS_PROTOCOL "gps_protocol"
#define VIK_SETTINGS_GPS_PORT "gps_port"
#define VIK_SETTINGS_GPS_POWER_OFF "gps_power_off"




static void * datasource_gps_init_func(acq_vik_t *avt)
{
	GPSData * gps_ud = new GPSData;
	gps_ud->direction = GPSDirection::DOWN;
	return gps_ud;
}




/**
 * Method to get the communication protocol of the GPS device from the widget structure.
 */
QString SlavGPS::datasource_gps_get_protocol(void * user_data)
{
	/* Uses the list of supported devices. */
	GPSData *w = (GPSData *)user_data;

	last_active = w->proto_combo->currentIndex();

	if (a_babel_device_list.size()) {
		const QString protocol = a_babel_device_list[last_active]->name;
		qDebug() << "II: Datasource GPS: get protocol: " << protocol;
		ApplicationState::set_string(VIK_SETTINGS_GPS_PROTOCOL, protocol);
		return protocol;
	}

	return "";
}




/**
 * Method to get the descriptor from the widget structure.
 * "Everything is a file".
 * Could actually be normal file or a serial port.
 */
QString SlavGPS::datasource_gps_get_descriptor(void * user_data)
{
	GPSData * data = (GPSData *) user_data;

	const QString descriptor = data->ser_combo->currentText();
	ApplicationState::set_string(VIK_SETTINGS_GPS_PORT, descriptor);
	return descriptor;
}




/**
 * Method to get the track handling behaviour from the widget structure.
 */
bool SlavGPS::datasource_gps_get_do_tracks(void * user_data)
{
	GPSData * data = (GPSData *) user_data;

	bool get_tracks = data->get_tracks_b->isChecked();
	if (data->direction == GPSDirection::DOWN) {
		ApplicationState::set_boolean(VIK_SETTINGS_GPS_GET_TRACKS, get_tracks);
	}
	return get_tracks;
}




/**
 * Method to get the route handling behaviour from the widget structure.
 */
bool SlavGPS::datasource_gps_get_do_routes(void * user_data)
{
	GPSData * data = (GPSData *) user_data;

	bool get_routes = data->get_routes_b->isChecked();
	if (data->direction == GPSDirection::DOWN) {
		ApplicationState::set_boolean(VIK_SETTINGS_GPS_GET_ROUTES, get_routes);
	}
	return get_routes;
}




/**
 * Method to get the waypoint handling behaviour from the widget structure.
 */
bool SlavGPS::datasource_gps_get_do_waypoints(void * user_data)
{
	GPSData * data = (GPSData *) user_data;

	bool get_waypoints = data->get_waypoints_b->isChecked();
	if (data->direction == GPSDirection::DOWN) {
		ApplicationState::set_boolean(VIK_SETTINGS_GPS_GET_WAYPOINTS, get_waypoints);
	}
	return get_waypoints;
}




static ProcessOptions * datasource_gps_get_process_options(void * user_data, void * not_used, const char *not_used2, const char *not_used3)
{
	ProcessOptions * po = new ProcessOptions();

	char *tracks = NULL;
	char *routes = NULL;
	char *waypoints = NULL;

	if (gps_acquire_in_progress) {
		po->babel_args = "";
		po->input_file_name = "";
	}

	gps_acquire_in_progress = true;

	const QString device = datasource_gps_get_protocol(user_data);

	if (datasource_gps_get_do_tracks (user_data)) {
		tracks = (char *) "-t";
	} else {
		tracks = (char *) "";
	}

	if (datasource_gps_get_do_routes (user_data)) {
		routes = (char *) "-r";
	} else {
		routes = (char *) "";
	}

	if (datasource_gps_get_do_waypoints (user_data)) {
		waypoints = (char *) "-w";
	} else {
		waypoints = (char *) "";
	}

	po->babel_args = QString("-D 9 %1 %2 %3 -i %4").arg(tracks).arg(routes).arg(waypoints).arg(device);
	/* Device points to static content => no free. */
	tracks = NULL;
	routes = NULL;
	waypoints = NULL;

	po->input_file_name = datasource_gps_get_descriptor(user_data);

	qDebug() << "DD: Datasource GPS: Get process options: using Babel args" << po->babel_args << "and input file" << po->input_file_name;

	return po;
}




/**
 * Method to get the off behaviour from the widget structure.
 */
bool SlavGPS::datasource_gps_get_off(void * user_data)
{
	GPSData * data = (GPSData *) user_data;

	bool power_off = data->off_request_b->isChecked();
	ApplicationState::set_boolean(VIK_SETTINGS_GPS_POWER_OFF, power_off);
	return power_off;
}




static void datasource_gps_off(void * user_data, QString & babel_args, QString & file_path)
{
	char *ser = NULL;
	GPSData *w = (GPSData *)user_data;

	if (gps_acquire_in_progress) {
		babel_args = "";
		file_path = "";
	}

	/* See if we should turn off the device. */
	if (!datasource_gps_get_off (user_data)){
		return;
	}

	if (a_babel_device_list.empty()) {
		return;
	}

	last_active = w->proto_combo->currentIndex();

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
#ifdef K
	ser = w->ser_combo->currentText();
#endif
	file_path = QString(ser);
}




static void datasource_gps_cleanup(void * user_data)
{
	delete ((GPSData *) user_data);
	gps_acquire_in_progress = false;
}




/**
 * External method to tidy up.
 */
void SlavGPS::datasource_gps_clean_up(void * user_data)
{
	datasource_gps_cleanup(user_data);
}




static void set_total_count(unsigned int cnt, AcquireProcess * acquiring)
{
	char *s = NULL;
#ifdef K
	gdk_threads_enter();
#endif
	if (acquiring->running) {
		GPSData *gps_data = (GPSData *) acquiring->user_data;
		const char *tmp_str;
#ifdef K
		switch (gps_data->progress_type) {
		case GPSTransferType::WPT:
			tmp_str = ngettext("Downloading %d waypoint...", "Downloading %d waypoints...", cnt);
			gps_data->total_count = cnt;
			break;
		case GPSTransferType::TRK:
			tmp_str = ngettext("Downloading %d trackpoint...", "Downloading %d trackpoints...", cnt);
			gps_data->total_count = cnt;
			break;
		default: {
			/* Maybe a gpsbabel bug/feature (upto at least v1.4.3 or maybe my Garmin device) but the count always seems x2 too many for routepoints. */
			int mycnt = (cnt / 2) + 1;
			tmp_str = ngettext("Downloading %d routepoint...", "Downloading %d routepoints...", mycnt);
			gps_data->total_count = mycnt;
			break;
		}
		}
		s = g_strdup_printf(tmp_str, cnt);
		gps_data->progress_label->setText(QObject::tr(s));
		gtk_widget_show(gps_data->progress_label);
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
		GPSData *gps_data = (GPSData *) acquiring->user_data;

		if (cnt < gps_data->total_count) {
			switch (gps_data->progress_type) {
			case GPSTransferType::WPT:
				s = g_strdup_printf(_("Downloaded %d out of %d %s..."), cnt, gps_data->total_count, "waypoints");
				break;
			case GPSTransferType::TRK:
				s = g_strdup_printf(_("Downloaded %d out of %d %s..."), cnt, gps_data->total_count, "trackpoints");
				break;
			default:
				s = g_strdup_printf(_("Downloaded %d out of %d %s..."), cnt, gps_data->total_count, "routepoints");
				break;
			}
		} else {
			switch (gps_data->progress_type) {
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
		gps_data->progress_label->setText(s);
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
		((GPSData *) acquiring->user_data)->gps_label->setText(QObject::tr("GPS Device: %s").arg(info));
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
	GPSData *gps_data = (GPSData *) acquiring->user_data;

	switch(c) {
	case BABEL_DIAG_OUTPUT:
		line = (char *)data;
#ifdef K
		gdk_threads_enter();
		if (acquiring->running) {
			acquiring->status->setText(QObject::tr("Status: Working..."));
		}
		gdk_threads_leave();
#endif

		/* Tells us the type of items that will follow. */
		if (strstr(line, "Xfer Wpt")) {
			gps_data->progress_label = gps_data->wp_label;
			gps_data->progress_type = GPSTransferType::WPT;
		}
		if (strstr(line, "Xfer Trk")) {
			gps_data->progress_label = gps_data->trk_label;
			gps_data->progress_type = GPSTransferType::TRK;
		}
		if (strstr(line, "Xfer Rte")) {
			gps_data->progress_label = gps_data->rte_label;
			gps_data->progress_type = GPSTransferType::RTE;
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
				gps_data->count = 0;
			}
		}
		if (strstr(line, "WPTDAT") || strstr(line, "TRKHDR") || strstr(line, "TRKDAT") || strstr(line, "RTEHDR") || strstr(line, "RTEWPT")) {
			gps_data->count++;
			set_current_count(gps_data->count, acquiring);
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
	GPSData * data = (GPSData *) user_data;

	data->proto_l = new QLabel(QObject::tr("GPS Protocol:"));
	data->proto_combo = new QComboBox();
	for (auto iter = a_babel_device_list.begin(); iter != a_babel_device_list.end(); iter++) {
		data->proto_combo->addItem((*iter)->label);
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

	data->proto_combo->setCurrentIndex(last_active);
#ifdef K
	g_object_ref(data->proto_combo);
#endif
	data->ser_l = new QLabel(QObject::tr("Serial Port:"));
	data->ser_combo = new QComboBox();


	/* Value from the settings is promoted to the top. */
	QString preferred_gps_port;
	if (ApplicationState::get_string(VIK_SETTINGS_GPS_PORT, preferred_gps_port)) {
		/* Use setting if available. */
		if (!preferred_gps_port.isEmpty()) {
#ifndef WINDOWS
			if (preferred_gps_port.left(6) == "/dev/tty") {
				if (access(preferred_gps_port.toUtf8().constData(), R_OK) == 0) {
					data->ser_combo->addItem(preferred_gps_port);
				}
			} else
#endif
				data->ser_combo->addItem(preferred_gps_port);
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
			data->ser_combo->addItem(port);
		}
	}


	data->ser_combo->setCurrentIndex(0);
#ifdef K
	g_object_ref(data->ser_combo);
#endif

	data->off_request_l = new QLabel(QObject::tr("Turn Off After Transfer\n(Garmin/NAViLink Only)"));
	data->off_request_b = new QCheckBox();
	bool power_off;
	if (!ApplicationState::get_boolean(VIK_SETTINGS_GPS_POWER_OFF, &power_off)) {
		power_off = false;
	}
	data->off_request_b->setChecked(power_off);

	data->get_tracks_l = new QLabel(QObject::tr("Tracks:"));
	data->get_tracks_b = new QCheckBox();
	bool get_tracks;
	if (!ApplicationState::get_boolean(VIK_SETTINGS_GPS_GET_TRACKS, &get_tracks)) {
		get_tracks = true;
	}
	data->get_tracks_b->setChecked(get_tracks);

	data->get_routes_l = new QLabel(QObject::tr("Routes:"));
	data->get_routes_b = new QCheckBox();
	bool get_routes;
	if (!ApplicationState::get_boolean(VIK_SETTINGS_GPS_GET_ROUTES, &get_routes)) {
		get_routes = false;
	}
	data->get_routes_b->setChecked(get_routes);

	data->get_waypoints_l = new QLabel(QObject::tr("Waypoints:"));
	data->get_waypoints_b = new QCheckBox();
	bool get_waypoints;
	if (!ApplicationState::get_boolean(VIK_SETTINGS_GPS_GET_WAYPOINTS, &get_waypoints)) {
		get_waypoints = true;
	}
	data->get_waypoints_b->setChecked(get_waypoints);

#ifdef K
	GtkTable * box = GTK_TABLE(gtk_table_new(2, 4, false));
	GtkTable * data_type_box = GTK_TABLE(gtk_table_new(4, 1, false));

	gtk_table_attach_defaults(box, GTK_WIDGET(data->proto_l), 0, 1, 0, 1);
	gtk_table_attach_defaults(box, GTK_WIDGET(data->proto_combo), 1, 2, 0, 1);
	gtk_table_attach_defaults(box, GTK_WIDGET(data->ser_l), 0, 1, 1, 2);
	gtk_table_attach_defaults(box, GTK_WIDGET(data->ser_combo), 1, 2, 1, 2);
	gtk_table_attach_defaults(data_type_box, GTK_WIDGET(data->get_tracks_l), 0, 1, 0, 1);
	gtk_table_attach_defaults(data_type_box, GTK_WIDGET(data->get_tracks_b), 1, 2, 0, 1);
	gtk_table_attach_defaults(data_type_box, GTK_WIDGET(data->get_routes_l), 2, 3, 0, 1);
	gtk_table_attach_defaults(data_type_box, GTK_WIDGET(data->get_routes_b), 3, 4, 0, 1);
	gtk_table_attach_defaults(data_type_box, GTK_WIDGET(data->get_waypoints_l), 4, 5, 0, 1);
	gtk_table_attach_defaults(data_type_box, GTK_WIDGET(data->get_waypoints_b), 5, 6, 0, 1);
	gtk_table_attach_defaults(box, GTK_WIDGET(data_type_box), 0, 2, 2, 3);
	gtk_table_attach_defaults(box, GTK_WIDGET(data->off_request_l), 0, 1, 3, 4);
	gtk_table_attach_defaults(box, GTK_WIDGET(data->off_request_b), 1, 3, 3, 4);
	gtk_box_pack_start (GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), GTK_WIDGET(box), false, false, 5);

	gtk_widget_show_all (dialog);
#endif
}




/**
 * @dialog: The GTK dialog. The caller is responsible for managing the dialog creation/deletion.
 * @xfer: The default type of items enabled for transfer, others disabled.
 * @xfer_all: When specified all items are enabled for transfer.
 *
 * Returns: A void * to the private structure for GPS progress/information widgets.
 *          Pass this pointer back into the other exposed datasource_gps_X functions.
 */
void * SlavGPS::datasource_gps_setup(GtkWidget *dialog, GPSTransferType xfer, bool xfer_all)
{
	GPSData *w_gps = (GPSData *)datasource_gps_init_func(NULL);
	w_gps->direction = GPSDirection::UP;
	datasource_gps_add_setup_widgets(dialog, NULL, w_gps);

	bool way = xfer_all;
	bool trk = xfer_all;
	bool rte = xfer_all;

	/* Selectively turn bits on. */
	if (!xfer_all) {
		switch (xfer) {
		case GPSTransferType::WPT:
			way = true;
			break;
		case GPSTransferType::RTE:
			rte = true;
			break;
		default:
			trk = true;
			break;
		}
	}
#ifdef K
	/* Apply. */
	w_gps->get_tracks_b->setChecked(trk);
	gtk_widget_set_sensitive(GTK_WIDGET(w_gps->get_tracks_l), trk);
	gtk_widget_set_sensitive(GTK_WIDGET(w_gps->get_tracks_b), trk);

	w_gps->get_routes_b->setChecked(rte);
	gtk_widget_set_sensitive(GTK_WIDGET(w_gps->get_routes_l), rte);
	gtk_widget_set_sensitive(GTK_WIDGET(w_gps->get_routes_b), rte);

	w_gps->get_waypoints_b->setChecked(way);
	gtk_widget_set_sensitive(GTK_WIDGET(w_gps->get_waypoints_l), way);
	gtk_widget_set_sensitive(GTK_WIDGET(w_gps->get_waypoints_b), way);
#endif

	return (void *)w_gps;
}




void datasource_gps_add_progress_widgets(GtkWidget *dialog, void * user_data)
{
	GPSData * w_gps = (GPSData *) user_data;

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

	w_gps->gps_label = gpslabel;
	w_gps->id_label = idlabel;
	w_gps->ver_label = verlabel;
	w_gps->progress_label = w_gps->wp_label = wplabel;
	w_gps->trk_label = trklabel;
	w_gps->rte_label = rtelabel;
	w_gps->total_count = -1;
#endif
}
