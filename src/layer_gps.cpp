/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2006-2008, Quy Tonthat <qtonthat@gmail.com>
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
#include <list>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <cstring>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif




#include <QDebug>
#include <QPushButton>




#ifdef VIK_CONFIG_REALTIME_GPS_TRACKING
#include "vikutils.h"
#endif

#include "viewport_internal.h"
#include "layers_panel.h"
#include "window.h"
#include "layer_gps.h"
#include "layer_trw.h"
#include "layer_trw_track_internal.h"
#include "tree_view_internal.h"
#include "application_state.h"
#include "babel.h"
#include "dialog.h"
#include "util.h"
#include "statusbar.h"




using namespace SlavGPS;




#define SG_MODULE "GPS Layer"

#define VIK_SETTINGS_GPS_STATUSBAR_FORMAT "gps_statusbar_format"




static WidgetStringEnumerationData gps_protocols_enum = {
	{
		"garmin",
		"magellan",
		"delbin",
		"navilink",
	},
	"garmin",
};





static std::vector<SGLabelID> trw_names = {
	SGLabelID(QObject::tr("GPS Download"),          GPS_CHILD_LAYER_TRW_DOWNLOAD),
	SGLabelID(QObject::tr("GPS Upload"),            GPS_CHILD_LAYER_TRW_UPLOAD),
#if REALTIME_GPS_TRACKING_ENABLED
	SGLabelID(QObject::tr("GPS Realtime Tracking"), GPS_CHILD_LAYER_TRW_REALTIME)
#endif
};




/* TODO_HARD: this list should be generated and updated automatically. */
static WidgetStringEnumerationData ports_enum = {
	{
		"/dev/ttyS0",
		"/dev/ttyS1",
		"/dev/ttyUSB0",
		"/dev/ttyUSB1",
		"usb:",
	},
	"/dev/ttyUSB0"
};




enum {
	PARAMETER_GROUP_DATA_MODE,
#if REALTIME_GPS_TRACKING_ENABLED
	PARAMETER_GROUP_REALTIME_MODE
#endif
};




#if REALTIME_GPS_TRACKING_ENABLED

static WidgetIntEnumerationData vehicle_position_enum = {
	{
		SGLabelID(QObject::tr("Keep vehicle at center"), (int) VehiclePosition::Centered),
		SGLabelID(QObject::tr("Keep vehicle on screen"), (int) VehiclePosition::OnScreen),
		SGLabelID(QObject::tr("Disable"),                (int) VehiclePosition::None)
	},
	(int) VehiclePosition::OnScreen,
};

static SGVariant gpsd_host_default(void) { return SGVariant("localhost"); }

/* If user enters 0 then retry is disabled. */
static MeasurementScale<Duration, Duration_ll, DurationUnit> gpsd_retry_interval_scale(0, 10000, 10, 10, DurationUnit::Seconds, 0);

/* If user enters 0 then default port number will be used. */
static ParameterScale<int> gpsd_port_scale(0, 65535, SGVariant((int32_t) SG_GPSD_PORT, SGVariantType::Int), 1, 0);

#endif




enum {
	PARAM_PROTOCOL = 0,
	PARAM_PORT,
	PARAM_DOWNLOAD_TRACKS,
	PARAM_UPLOAD_TRACKS,
	PARAM_DOWNLOAD_ROUTES,
	PARAM_UPLOAD_ROUTES,
	PARAM_DOWNLOAD_WAYPOINTS,
	PARAM_UPLOAD_WAYPOINTS,
#if REALTIME_GPS_TRACKING_ENABLED
	PARAM_REALTIME_REC,
	PARAM_REALTIME_CENTER_START,
	PARAM_VEHICLE_POSITION,
	PARAM_REALTIME_UPDATE_STATUSBAR,
	PARAM_GPSD_HOST,
	PARAM_GPSD_PORT,
	PARAM_GPSD_RETRY_INTERVAL,
#endif /* REALTIME_GPS_TRACKING_ENABLED */
	NUM_PARAMS
};




static ParameterSpecification gps_layer_param_specs[] = {
	/* LayerGPS::init() is performed after registration of
	   gps_layer_param_specs thus to give the protocols list some
	   initial values use a list with all items:
	   gps_protocols_enum. */
	{ PARAM_PROTOCOL,                   "gps_protocol",              SGVariantType::String,       PARAMETER_GROUP_DATA_MODE,     QObject::tr("GPS Protocol:"),                     WidgetType::StringEnumeration,   &gps_protocols_enum,        NULL,                        "" },
	{ PARAM_PORT,                       "gps_port",                  SGVariantType::String,       PARAMETER_GROUP_DATA_MODE,     QObject::tr("Serial Port:"),                      WidgetType::StringEnumeration,   &ports_enum,                NULL,                        "" },
	{ PARAM_DOWNLOAD_TRACKS,            "gps_download_tracks",       SGVariantType::Boolean,      PARAMETER_GROUP_DATA_MODE,     QObject::tr("Download Tracks:"),                  WidgetType::CheckButton,   NULL,                       sg_variant_true,             "" },
	{ PARAM_UPLOAD_TRACKS,              "gps_upload_tracks",         SGVariantType::Boolean,      PARAMETER_GROUP_DATA_MODE,     QObject::tr("Upload Tracks:"),                    WidgetType::CheckButton,   NULL,                       sg_variant_true,             "" },
	{ PARAM_DOWNLOAD_ROUTES,            "gps_download_routes",       SGVariantType::Boolean,      PARAMETER_GROUP_DATA_MODE,     QObject::tr("Download Routes:"),                  WidgetType::CheckButton,   NULL,                       sg_variant_true,             "" },
	{ PARAM_UPLOAD_ROUTES,              "gps_upload_routes",         SGVariantType::Boolean,      PARAMETER_GROUP_DATA_MODE,     QObject::tr("Upload Routes:"),                    WidgetType::CheckButton,   NULL,                       sg_variant_true,             "" },
	{ PARAM_DOWNLOAD_WAYPOINTS,         "gps_download_waypoints",    SGVariantType::Boolean,      PARAMETER_GROUP_DATA_MODE,     QObject::tr("Download Waypoints:"),               WidgetType::CheckButton,   NULL,                       sg_variant_true,             "" },
	{ PARAM_UPLOAD_WAYPOINTS,           "gps_upload_waypoints",      SGVariantType::Boolean,      PARAMETER_GROUP_DATA_MODE,     QObject::tr("Upload Waypoints:"),                 WidgetType::CheckButton,   NULL,                       sg_variant_true,             "" },
#if REALTIME_GPS_TRACKING_ENABLED
	{ PARAM_REALTIME_REC,               "record_tracking",           SGVariantType::Boolean,      PARAMETER_GROUP_REALTIME_MODE, QObject::tr("Recording tracks"),                  WidgetType::CheckButton,   NULL,                       sg_variant_true,             "" },
	{ PARAM_REALTIME_CENTER_START,      "center_start_tracking",     SGVariantType::Boolean,      PARAMETER_GROUP_REALTIME_MODE, QObject::tr("Jump to current position on start"), WidgetType::CheckButton,   NULL,                       sg_variant_false,            "" },
	{ PARAM_VEHICLE_POSITION,           "moving_map_method",         SGVariantType::Enumeration,  PARAMETER_GROUP_REALTIME_MODE, QObject::tr("Moving Map Method:"),                WidgetType::IntEnumeration,   &vehicle_position_enum,     NULL,                        "" },
	{ PARAM_REALTIME_UPDATE_STATUSBAR,  "realtime_update_statusbar", SGVariantType::Boolean,      PARAMETER_GROUP_REALTIME_MODE, QObject::tr("Update Statusbar:"),                 WidgetType::CheckButton,   NULL,                       sg_variant_true,             QObject::tr("Display information in the statusbar on GPS updates") },
	{ PARAM_GPSD_HOST,                  "gpsd_host",                 SGVariantType::String,       PARAMETER_GROUP_REALTIME_MODE, QObject::tr("Gpsd Host:"),                        WidgetType::Entry,         NULL,                       gpsd_host_default,           "" },
	{ PARAM_GPSD_PORT,                  "gpsd_port",                 SGVariantType::Int,          PARAMETER_GROUP_REALTIME_MODE, QObject::tr("Gpsd Port:"),                        WidgetType::SpinBoxInt,    &gpsd_port_scale,           NULL,                        "" },
	{ PARAM_GPSD_RETRY_INTERVAL,        "gpsd_retry_interval",       SGVariantType::DurationType, PARAMETER_GROUP_REALTIME_MODE, QObject::tr("Gpsd Retry Interval:"),              WidgetType::DurationType,  &gpsd_retry_interval_scale, NULL,                        "" }, // KKAMIL
#endif /* REALTIME_GPS_TRACKING_ENABLED */

	{ NUM_PARAMS,                       "",                          SGVariantType::Empty,        PARAMETER_GROUP_GENERIC,       "",                                               WidgetType::None,          NULL,                       NULL,                        "" }, /* Guard. */
};




GPSSession::GPSSession(GPSTransfer & new_transfer, LayerTRW * new_trw, Track * new_trk, GisViewport * new_gisview, bool new_in_progress)
{
	qDebug() << SG_PREFIX_D << "";

	this->transfer = new_transfer;

	this->window_title = (this->transfer.direction == GPSDirection::Download) ? QObject::tr("GPS Download") : QObject::tr("GPS Upload");
	this->trw = new_trw;
	this->trk = new_trk;
	this->gisview = new_gisview;
	this->in_progress = new_in_progress;
}





LayerGPSInterface vik_gps_layer_interface;




LayerGPSInterface::LayerGPSInterface()
{
	this->parameters_c = gps_layer_param_specs;

	this->parameter_groups.emplace_back(SGLabelID(QObject::tr("Data Mode"), PARAMETER_GROUP_DATA_MODE));
#if REALTIME_GPS_TRACKING_ENABLED
	this->parameter_groups.emplace_back(SGLabelID(QObject::tr("Realtime Tracking Mode"), PARAMETER_GROUP_REALTIME_MODE));
#endif

	this->fixed_layer_kind_string = "GPS"; /* Non-translatable. */

	// this->action_accelerator = ...; /* Empty accelerator. */
	// this->action_icon = ...; /* Set elsewhere. */

	this->ui_labels.new_layer = QObject::tr("New GPS Layer");
	this->ui_labels.translated_layer_kind = QObject::tr("GPS");
	this->ui_labels.layer_defaults = QObject::tr("Default Settings of GPS Layer");
}




/**
   Overwrite the static setup with dynamically generated GPS Babel device list.
*/
void LayerGPS::init(void)
{
	gps_protocols_enum.values.clear();

	int i = 0;
	for (auto iter = Babel::devices.begin(); iter != Babel::devices.end(); iter++) {
		/* Should be using 'label' property but use
		   'identifier' for now thus don't need to mess around
		   converting 'label' to 'identifier' later on. */
		gps_protocols_enum.values.push_back((*iter)->identifier);
		qDebug() << SG_PREFIX_I << "New protocol:" << (*iter)->identifier;
	}
}




QString LayerGPS::get_tooltip(void) const
{
	return QObject::tr("Protocol: %1").arg(this->upload.gps_protocol);
}




/* "Copy". */
void LayerGPS::marshall(Pickle & pickle)
{
	this->marshall_params(pickle);

	for (int i = 0; i < GPS_CHILD_LAYER_MAX; i++) {
		Pickle helper_pickle;
		this->trw_children[i]->marshall(helper_pickle);
		if (helper_pickle.data_size() > 0) {
			pickle.put_pickle(helper_pickle);
		}
	}
}




/* "Paste". */
Layer * LayerGPSInterface::unmarshall(Pickle & pickle, GisViewport * gisview)
{
	LayerGPS * layer = new LayerGPS();
	layer->set_coord_mode(gisview->get_coord_mode());

	layer->unmarshall_params(pickle);

	int i = 0;
	while (pickle.data_size() > 0 && i < GPS_CHILD_LAYER_MAX) {
		Layer * child_layer = Layer::unmarshall(pickle, gisview);
		if (child_layer) {
			layer->trw_children[i++] = (LayerTRW *) child_layer;
			/* NB no need to attach signal update handler here
			   as this will always be performed later on in LayerGPS::attach_children_to_tree(). */
		}
	}

	// qDebug() << SG_PREFIX_I << "Unmarshalling ended with" << pickle.data_szie;
	assert (pickle.data_size() == 0);
	return layer;
}




bool LayerGPS::set_param_value(param_id_t param_id, const SGVariant & data, bool is_file_operation)
{
	qDebug() << SG_PREFIX_D << "";

	switch (param_id) {
	case PARAM_PROTOCOL:
		assert (data.type_id == SGVariantType::String); /* One of very few enums in which the string is a primary data type. */
		this->upload.gps_protocol = data.val_string;
		this->download.gps_protocol = data.val_string;
		qDebug() << SG_PREFIX_D << "Selected GPS Protocol is" << data.val_string;
		break;
	case PARAM_PORT:
		assert (data.type_id == SGVariantType::String); /* One of very few enums in which the string is a primary data type. */
		this->upload.serial_port = data.val_string;
		this->download.serial_port = data.val_string;
		qDebug() << SG_PREFIX_D << "Selected Serial Port is" << data.val_string;
		break;
	case PARAM_DOWNLOAD_TRACKS:
		this->download.do_tracks = data.u.val_bool;
		break;
	case PARAM_UPLOAD_TRACKS:
		this->upload.do_tracks = data.u.val_bool;
		break;
	case PARAM_DOWNLOAD_ROUTES:
		this->download.do_routes = data.u.val_bool;
		break;
	case PARAM_UPLOAD_ROUTES:
		this->upload.do_routes = data.u.val_bool;
		break;
	case PARAM_DOWNLOAD_WAYPOINTS:
		this->download.do_waypoints = data.u.val_bool;
		break;
	case PARAM_UPLOAD_WAYPOINTS:
		this->upload.do_waypoints = data.u.val_bool;
		break;
#if REALTIME_GPS_TRACKING_ENABLED
	case PARAM_GPSD_HOST:
		if (!data.val_string.isEmpty()) {
			this->gpsd_host = data.val_string;
		}
		break;
	case PARAM_GPSD_PORT:
		this->gpsd_port = data.u.val_int == 0 ? SG_GPSD_PORT : data.u.val_int;
		break;
	case PARAM_GPSD_RETRY_INTERVAL:
		this->gpsd_retry_interval = data.get_duration();
		break;
	case PARAM_REALTIME_REC:
		this->realtime_record = data.u.val_bool;
		break;
	case PARAM_REALTIME_CENTER_START:
		this->realtime_jump_to_start = data.u.val_bool;
		break;
	case PARAM_VEHICLE_POSITION:
		this->vehicle_position = (VehiclePosition) data.u.val_enumeration;
		break;
	case PARAM_REALTIME_UPDATE_STATUSBAR:
		this->realtime_update_statusbar = data.u.val_bool;
		break;
#endif /* REALTIME_GPS_TRACKING_ENABLED */
	default:
		qDebug() << SG_PREFIX_E << "unknown parameter id" << param_id;
	}

	return true;
}




SGVariant LayerGPS::get_param_value(param_id_t param_id, bool is_file_operation) const
{
	qDebug() << SG_PREFIX_D << "";

	SGVariant rv;
	switch (param_id) {
	case PARAM_PROTOCOL:
		rv = SGVariant(this->upload.gps_protocol, gps_layer_param_specs[PARAM_PROTOCOL].type_id);
		break;
	case PARAM_PORT:
		rv = SGVariant(this->upload.serial_port, gps_layer_param_specs[PARAM_PORT].type_id);
		break;
	case PARAM_DOWNLOAD_TRACKS:
		rv = SGVariant(this->download.do_tracks);
		break;
	case PARAM_UPLOAD_TRACKS:
		rv = SGVariant(this->upload.do_tracks);
		break;
	case PARAM_DOWNLOAD_ROUTES:
		rv = SGVariant(this->download.do_routes);
		break;
	case PARAM_UPLOAD_ROUTES:
		rv = SGVariant(this->upload.do_routes);
		break;
	case PARAM_DOWNLOAD_WAYPOINTS:
		rv = SGVariant(this->download.do_waypoints);
		break;
	case PARAM_UPLOAD_WAYPOINTS:
		rv = SGVariant(this->upload.do_waypoints);
		break;
#if REALTIME_GPS_TRACKING_ENABLED
	case PARAM_GPSD_HOST:
		rv = SGVariant(this->gpsd_host);
		break;
	case PARAM_GPSD_PORT:
		rv = SGVariant(this->gpsd_port == 0 ? SG_GPSD_PORT : this->gpsd_port, gps_layer_param_specs[PARAM_GPSD_PORT].type_id);
		break;
	case PARAM_GPSD_RETRY_INTERVAL:
		rv = SGVariant(this->gpsd_retry_interval, gps_layer_param_specs[PARAM_GPSD_RETRY_INTERVAL].type_id);
		break;
	case PARAM_REALTIME_REC:
		rv = SGVariant(this->realtime_record);
		break;
	case PARAM_REALTIME_CENTER_START:
		rv = SGVariant(this->realtime_jump_to_start);
		break;
	case PARAM_VEHICLE_POSITION:
		rv = SGVariant((int) this->vehicle_position, gps_layer_param_specs[PARAM_VEHICLE_POSITION].type_id);
		break;
	case PARAM_REALTIME_UPDATE_STATUSBAR:
		rv = SGVariant(this->realtime_update_statusbar); /* kamilkamil: in viking code there is a mismatch of data types. */
		break;
#endif /* REALTIME_GPS_TRACKING_ENABLED */
	default:
		qDebug() << SG_PREFIX_E << "unknown parameter id" << param_id;
	}

	return rv;
}




void LayerGPS::draw_tree_item(GisViewport * gisview, bool highlight_selected, bool parent_is_selected)
{
	qDebug() << SG_PREFIX_D << "-- realtime tracking";

	Layer * trigger = gisview->get_trigger();

	for (int i = 0; i < GPS_CHILD_LAYER_MAX; i++) {
		LayerTRW * trw = this->trw_children[i];
		if (TreeItem::the_same_object(trw, trigger)) {
			if (gisview->get_half_drawn()) {
				gisview->set_half_drawn(false);
				gisview->snapshot_restore();
			} else {
				gisview->snapshot_save();
			}
		}
		if (!gisview->get_half_drawn()) {
			trw->draw_tree_item(gisview, false, false);
		}
	}
#if REALTIME_GPS_TRACKING_ENABLED
	if (this->realtime_tracking_in_progress) {
		if (TreeItem::the_same_object(this, trigger)) {
			if (gisview->get_half_drawn()) {
				gisview->set_half_drawn(false);
				gisview->snapshot_restore();
			} else {
				gisview->snapshot_save();
			}
		}
		if (!gisview->get_half_drawn()) {
			this->rt_tracking_draw(gisview, this->current_rt_data);
		}
	}
#endif /* REALTIME_GPS_TRACKING_ENABLED */
}




void LayerGPS::change_coord_mode(CoordMode mode)
{
	qDebug() << SG_PREFIX_D << "";

	for (int i = 0; i < GPS_CHILD_LAYER_MAX; i++) {
		this->trw_children[i]->change_coord_mode(mode);
	}
}




void LayerGPS::add_menu_items(QMenu & menu)
{
	QAction * action = NULL;


	action = new QAction(QObject::tr("&Upload to GPS"), this);
	action->setIcon(QIcon::fromTheme("go-up"));
	QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (gps_upload_cb(void)));
	menu.addAction(action);


	action = new QAction(QObject::tr("Download from &GPS"), this);
	action->setIcon(QIcon::fromTheme("go-down"));
	QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (gps_download_cb(void)));
	menu.addAction(action);


#if REALTIME_GPS_TRACKING_ENABLED
	action = new QAction(this->realtime_tracking_in_progress ? QObject::tr("&Stop Realtime Tracking") : QObject::tr("&Start Realtime Tracking"), this);
	action->setIcon(this->realtime_tracking_in_progress ? QIcon::fromTheme("media-playback-stop") : QIcon::fromTheme("media-playback-start"));
	QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (rt_start_stop_tracking_cb()));
	menu.addAction(action);


	action = new QAction(QObject::tr("Empty &Realtime"), this);
	action->setIcon(QIcon::fromTheme("edit-clear"));
	QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (rt_empty_realtime_cb(void)));
	menu.addAction(action);
#endif /* REALTIME_GPS_TRACKING_ENABLED */


	action = new QAction(QObject::tr("E&mpty Upload"), this);
	action->setIcon(QIcon::fromTheme("edit-clear"));
	QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (gps_empty_upload_cb(void)));
	menu.addAction(action);


	action = new QAction(QObject::tr("&Empty Download"), this);
	action->setIcon(QIcon::fromTheme("edit-clear"));
	QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (gps_empty_download_cb(void)));
	menu.addAction(action);


	action = new QAction(QObject::tr("Empty &All"), this);
	action->setIcon(QIcon::fromTheme("edit-clear"));
	QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (gps_empty_all_cb()));
	menu.addAction(action);
}




LayerGPS::~LayerGPS()
{
	for (int i = 0; i < GPS_CHILD_LAYER_MAX; i++) {
		if (this->tree_view) {
			//this->disconnect_layer_signal(this->trw_children[i]);
		}
		this->trw_children[i]->unref_layer();
	}

#if REALTIME_GPS_TRACKING_ENABLED
	this->rt_stop_tracking();
#endif
}




sg_ret LayerGPS::attach_children_to_tree(void)
{
	if (!this->is_in_tree()) {
		qDebug() << SG_PREFIX_E << "GPS Layer" << this->name << "is not connected to tree";
		return sg_ret::err;
	}

	for (int i = 0; i < GPS_CHILD_LAYER_MAX; i++) {
		LayerTRW * trw = this->trw_children[i];
		qDebug() << SG_PREFIX_I << "Attaching to tree item" << trw->name << "under" << this->name;
		this->tree_view->attach_to_tree(this, trw);
		QObject::connect(trw, SIGNAL (tree_item_changed(const QString &)), this, SLOT (child_tree_item_changed_cb(const QString &)));
	}

	return sg_ret::ok;
}




std::list<Layer const * > LayerGPS::get_child_layers(void) const
{
	qDebug() << SG_PREFIX_D << "";

	std::list<Layer const * > result;
	for (int i = GPS_CHILD_LAYER_MAX - 1; i >= 0; i--) {
		result.push_front((Layer const *) this->trw_children[i]);
	}
	return result;
}




LayerTRW * LayerGPS::get_a_child()
{
	qDebug() << SG_PREFIX_D << "";

	assert ((this->cur_read_child >= 0) && (this->cur_read_child < GPS_CHILD_LAYER_MAX));

	LayerTRW * trw = this->trw_children[this->cur_read_child];
	if (++(this->cur_read_child) >= GPS_CHILD_LAYER_MAX) {
		this->cur_read_child = 0;
	}
	return trw;
}




/**
   @reviewed-on: 2019-03-23
*/
int LayerGPS::get_child_layers_count(void) const
{
	if (this->trw_children[0]) {
		/* First child layer not created, so the second/third is not created either. */
		return 0;
	} else {
		/* First child has been created, so the second/third has been created as well. */
		return GPS_CHILD_LAYER_MAX;
	}
}




void GPSSession::set_total_count(int cnt)
{
	qDebug() << SG_PREFIX_D << "";

	this->mutex.lock();
	if (this->in_progress) {
		QString msg;
		if (this->transfer.direction == GPSDirection::Download) {
			switch (this->progress_type) {
			case GPSTransferType::WPT:
				msg = QObject::tr("Downloading %n waypoints...", "", cnt);
				this->total_count = cnt;
				break;
			case GPSTransferType::TRK:
				msg = QObject::tr("Downloading %n trackpoints...", "", cnt);
				this->total_count = cnt;
				break;
			default: {
				/* Maybe a gpsbabel bug/feature (upto at least v1.4.3 or maybe my Garmin device) but the count always seems x2 too many for routepoints. */
				int mycnt = (cnt / 2) + 1;
				msg = QObject::tr("Downloading %n routepoints...", "", mycnt);
				this->total_count = mycnt;
				break;
			}
			}
		} else {
			switch (this->progress_type) {
			case GPSTransferType::WPT:
				msg = QObject::tr("Uploading %n waypoints...", "", cnt);
				break;
			case GPSTransferType::TRK:
				msg = QObject::tr("Uploading %n trackpoints...", "", cnt);
				break;
			default:
				msg = QObject::tr("Uploading %n routepoints...", "", cnt);
				break;
			}
		}

		this->progress_label->setText(msg);
		this->total_count = cnt;
	}
	this->mutex.unlock();
}




/* Compare this function with set_current_count(int cnt, AcquireProcess * acquiring) */
void GPSSession::set_current_count(int cnt)
{
	qDebug() << SG_PREFIX_D << "";

	this->mutex.lock();
	if (!this->in_progress) {
		this->mutex.unlock();
		return;
	}

	QString msg;

	if (cnt < this->total_count) {
		QString fmt;
		if (this->transfer.direction == GPSDirection::Download) {
			switch (this->progress_type) {
			case GPSTransferType::WPT:
				fmt = QObject::tr("Downloaded %1 out of %2 waypoints...", "", this->total_count);
				break;
			case GPSTransferType::TRK:
				fmt = QObject::tr("Downloaded %1 out of %2 trackpoints...", "", this->total_count);
				break;
			default:
				fmt = QObject::tr("Downloaded %1 out of %2 routepoints...", "", this->total_count);
				break;
			}
		} else {
			switch (this->progress_type) {
			case GPSTransferType::WPT:
				fmt = QObject::tr("Uploaded %1 out of %2 waypoints...", "", this->total_count);
					break;
			case GPSTransferType::TRK:
				fmt = QObject::tr("Uploaded %1 out of %2 trackpoints...", "", this->total_count);
				break;
			default:
				fmt = QObject::tr("Uploaded %1 out of %2 routepoints...", "", this->total_count);
				break;
			}
		}
		msg = QString(fmt).arg(cnt).arg(this->total_count);
	} else {
		if (this->transfer.direction == GPSDirection::Download) {
			switch (this->progress_type) {
			case GPSTransferType::WPT:
				msg = QObject::tr("Downloaded %n waypoints", "", cnt);
				break;
			case GPSTransferType::TRK:
				msg = QObject::tr("Downloaded %n trackpoints", "", cnt);
				break;
			default:
				msg = QObject::tr("Downloaded %n routepoints", "", cnt);
				break;
			}
		} else {
			switch (this->progress_type) {
				case GPSTransferType::WPT:
					msg = QObject::tr("Uploaded %n waypoints", "", cnt);
					break;
				case GPSTransferType::TRK:
					msg = QObject::tr("Uploaded %n trackpoints", "", cnt);
					break;
				default:
					msg = QObject::tr("Uploaded %n routepoints", "", cnt);
					break;
			}
		}
	}
	this->progress_label->setText(msg);

	this->mutex.unlock();
}




void GPSSession::set_gps_device_info(const QString & info)
{
	qDebug() << SG_PREFIX_D << "";

	this->mutex.lock();
	if (this->in_progress) {
		this->gps_device_label->setText(QObject::tr("GPS Device: %1").arg(info));
	}
	this->mutex.unlock();
}




/*
  Common processing for GPS Device information.
  It doesn't matter whether we're uploading or downloading.
*/
void GPSSession::process_line_for_gps_info(const char * line)
{
	qDebug() << SG_PREFIX_D << "";

	if (strstr(line, "PRDDAT")) {
		char info[128] = { 0 };
		if (SlavGPS::get_prddat_gps_info(info, sizeof (info), line)) {
			this->set_gps_device_info(info);
		}
	}

	/* eg: "Unit:\teTrex Legend HCx Software Version 2.90\n" */
	if (strstr(line, "Unit:")) {
		char info[128] = { 0 };
		if (SlavGPS::get_unit_gps_info(info, sizeof (info), line)) {
			this->set_gps_device_info(info);
		}
	}
}




bool SlavGPS::get_prddat_gps_info(char * info, size_t info_size, const char * str)
{
	const QString line(str);
	const QStringList tokens = line.split(" ");

	size_t ilen = 0;
	const int n_tokens = tokens.size();

	if (n_tokens <= 8) {
		return false;
	}

	/* I'm not entirely clear what information this is trying to get...
	   Obviously trying to decipher some kind of text/naming scheme.
	   Anyway this will be superceded if there is 'Unit:' information. */
	for (int i = 8; i < n_tokens; i++) {

		if (!(ilen < info_size - 2)) {
			break;
		}
		if (tokens[i] == "00") {
			break;
		}

		unsigned int ch;
		sscanf(tokens[i].toUtf8().constData(), "%x", &ch);
		info[ilen++] = ch;
	}
	info[ilen++] = 0;

	return true;
}




bool SlavGPS::get_unit_gps_info(char * info, size_t info_size, const char * str)
{
	const QString line(str);
	const QStringList tokens = line.split("\t");
	const int n_tokens = tokens.size();

	if (n_tokens <= 1) {
		return false;
	}

	snprintf(info, info_size, "%s", tokens[1].toUtf8().constData());
	return true;
}




void GPSSession::import_progress_cb(AcquireProgressCode code, void * data)
{
	qDebug() << SG_PREFIX_D << "";

	char * line = NULL;

	this->mutex.lock();
	if (!this->in_progress) {
		this->mutex.unlock();
#ifdef K_FIXME_RESTORE
		delete this;
#endif
	}
	this->mutex.unlock();

	switch (code) {
	case AcquireProgressCode::DiagOutput:
		line = (char *) data;

		this->mutex.lock();
		if (this->in_progress) {
			this->status_label->setText(QObject::tr("Status: Working..."));
		}
		this->mutex.unlock();

		/* Tells us the type of items that will follow. */
		if (strstr(line, "Xfer Wpt")) {
			this->progress_label = this->wp_label;
			this->progress_type = GPSTransferType::WPT;
		}
		if (strstr(line, "Xfer Trk")) {
			this->progress_label = this->trk_label;
			this->progress_type = GPSTransferType::TRK;
		}
		if (strstr(line, "Xfer Rte")) {
			this->progress_label = this->rte_label;
			this->progress_type = GPSTransferType::RTE;
		}

		this->process_line_for_gps_info(line);

		if (strstr(line, "RECORD")) {
			if (strlen(line) > 20) {
				unsigned int lsb, msb;
				sscanf(line + 17, "%x", &lsb);
				sscanf(line + 20, "%x", &msb);
				unsigned int cnt = lsb + msb * 256;
				this->set_total_count(cnt);
				this->count = 0;
			}
		}
		if (strstr(line, "WPTDAT") || strstr(line, "TRKHDR") || strstr(line, "TRKDAT") || strstr(line, "RTEHDR") || strstr(line, "RTEWPT")) {
			this->count++;
			this->set_current_count(this->count);
		}
		break;
	case AcquireProgressCode::Completed:
		break;
	default:
		break;
	}
}




void GPSSession::export_progress_cb(AcquireProgressCode code, void * data)
{
	qDebug() << SG_PREFIX_D << "";

	char * line = NULL;
	static unsigned int cnt = 0;

	this->mutex.lock();
	if (!this->in_progress) {
		this->mutex.unlock();
#ifdef K_FIXME_RESTORE
		delete sess;
#endif
	}
	this->mutex.unlock();

	switch (code) {
	case AcquireProgressCode::DiagOutput:
		line = (char *) data;

		this->mutex.lock();
		if (this->in_progress) {
			this->status_label->setText(QObject::tr("Status: Working..."));
		}
		this->mutex.unlock();

		this->process_line_for_gps_info(line);

		if (strstr(line, "RECORD")) {


			if (strlen(line) > 20) {
				unsigned int lsb, msb;
				sscanf(line + 17, "%x", &lsb);
				sscanf(line + 20, "%x", &msb);
				cnt = lsb + msb * 256;
				/* this->set_total_count(cnt); */
				this->count = 0;
			}
		}
		if (strstr(line, "WPTDAT")) {
			if (this->count == 0) {
				this->progress_label = this->wp_label;
				this->progress_type = GPSTransferType::WPT;
				this->set_total_count(cnt);
			}
			this->count++;
			this->set_current_count(this->count);
		}
		if (strstr(line, "RTEHDR") || strstr(line, "RTEWPT")) {
			if (this->count == 0) {
				this->progress_label = this->rte_label;
				this->progress_type = GPSTransferType::RTE;
				/* Maybe a gpsbabel bug/feature (upto at least v1.4.3 or maybe my Garmin device) but the count always seems x2 too many for routepoints.
				   Anyway since we're uploading - we should know how many points we're going to put! */
				cnt = (cnt / 2) + 1;
				this->set_total_count(cnt);
			}
			this->count++;
			this->set_current_count(this->count);
		}
		if (strstr(line, "TRKHDR") || strstr(line, "TRKDAT")) {
			if (this->count == 0) {
				this->progress_label = this->trk_label;
				this->progress_type = GPSTransferType::TRK;
				this->set_total_count(cnt);
			}
			this->count++;
			this->set_current_count(this->count);
		}
		break;
	case AcquireProgressCode::Completed:
		break;
	default:
		break;
	}
}




/* Re-implementation of QRunnable::run() */
void GPSSession::run(void)
{
	qDebug() << SG_PREFIX_D << "";

	SaveStatus save_status;

	AcquireContext acquire_context;

	if (this->transfer.direction == GPSDirection::Download) {
		BabelProcess * importer = new BabelProcess();
		importer->set_options(this->babel_opts);
		importer->set_input("", this->transfer.serial_port); /* TODO_LATER: type of input? */

		acquire_context.target_trw = this->trw;
		importer->set_acquire_context(&acquire_context);
		importer->set_progress_dialog(NULL /* TODO_LATER: progr_dialog */);

		save_status = (sg_ret::ok == importer->run_process()) ? SaveStatus::Code::Success : SaveStatus::Code::Error;
	} else {
		BabelProcess * exporter = new BabelProcess();
		exporter->set_options(this->babel_opts);
		exporter->set_output("", this->transfer.serial_port); /* TODO_LATER: type of output? */

		acquire_context.target_trw = this->trw;
		acquire_context.target_trk = this->trk;
		exporter->set_acquire_context(&acquire_context);
		exporter->set_progress_dialog(NULL /* TODO_LATER: progr_dialog */);

		save_status = exporter->export_through_gpx(this->trw, this->trk);
	}

	if (SaveStatus::Code::Success != save_status) {
		this->status_label->setText(QObject::tr("Error: failed to export with gpsbabel."));
	} else {
		this->mutex.lock();
		if (this->in_progress) {
			this->status_label->setText(QObject::tr("Done."));
			this->dialog->button_box->button(QDialogButtonBox::Ok)->setEnabled(true);
			this->dialog->button_box->button(QDialogButtonBox::Cancel)->setEnabled(false);

			/* Do not change the view if we are following the current GPS position. */
#if REALTIME_GPS_TRACKING_ENABLED
			if (!this->realtime_tracking_in_progress)
#endif
			{
				if (this->gisview && this->transfer.direction == GPSDirection::Download) {
					this->trw->post_read(this->gisview, true);
					/* View the data available. */
					this->trw->move_viewport_to_show_all(this->gisview) ;
					this->trw->emit_tree_item_changed("GPS Session - post read"); /* NB update from background thread. */
				}
			}
		} else {
			/* Cancelled. */
		}
		this->mutex.unlock();
	}

	this->mutex.lock();
	if (this->in_progress) {
		this->in_progress = false;
		this->mutex.unlock();
	} else {
		this->mutex.unlock();
		delete this; /* TODO_LATER: is is valid? Can we delete ourselves here? */
	}
}





/**
   @brief Talk to a GPS Device using a thread which updates a dialog with the progress

   @layer: The TrackWaypoint layer to operate on
   @track: Operate on a particular track when specified
   @viewport: A viewport is required as the display may get updated
   @tracking: If tracking then viewport display update will be skipped
*/
int GPSTransfer::run_transfer(LayerTRW * layer, Track * trk, GisViewport * gisview, bool tracking)
{
	qDebug() << SG_PREFIX_D << "";

	GPSSession * sess = new GPSSession(*this, layer, trk, gisview, true);
	sess->setAutoDelete(false);

	/* This must be done inside the main thread as the uniquify causes screen updates
	   (originally performed this nearer the point of upload in the thread). */
	if (this->direction == GPSDirection::Upload) {
		/* Enforce unique names in the layer upload to the GPS device.
		   NB this may only be a Garmin device restriction (and may be not every Garmin device either...).
		   Thus this maintains the older code in built restriction. */
		if (!sess->trw->uniquify()) {
			sess->trw->get_window()->get_statusbar()->set_message(StatusBarField::Info,
									      QObject::tr("Warning - GPS Upload items may overwrite each other"));
		}
	}

#if REALTIME_GPS_TRACKING_ENABLED
	sess->realtime_tracking_in_progress = tracking;
#endif

	sess->babel_opts = QString("-D 9 %1").arg(BabelProcess::get_trw_string(this->do_tracks, this->do_routes, this->do_waypoints));


	/* Only create dialog if we're going to do some transferring. */
	if (this->do_tracks || this->do_waypoints || this->do_routes) {

		sess->dialog = new BasicDialog(layer->get_window());
		sess->dialog->button_box->button(QDialogButtonBox::Ok)->setEnabled(false);
		sess->dialog->setWindowTitle(sess->window_title);

		int row = 0;

		sess->status_label = new QLabel(QObject::tr("Status: detecting gpsbabel"));
		sess->dialog->grid->addWidget(sess->status_label, row, 0);
		row++;

		sess->gps_device_label = new QLabel(QObject::tr("GPS device: N/A"));
		sess->dialog->grid->addWidget(sess->gps_device_label, row, 0);
		row++;

		sess->ver_label = new QLabel("");
		sess->dialog->grid->addWidget(sess->ver_label, row, 0);
		row++;

		sess->id_label = new QLabel("");
		sess->dialog->grid->addWidget(sess->id_label, row, 0);
		row++;

		sess->wp_label = new QLabel("");
		sess->dialog->grid->addWidget(sess->wp_label, row, 0);
		row++;

		sess->trk_label = new QLabel("");
		sess->dialog->grid->addWidget(sess->trk_label, row, 0);
		row++;

		sess->rte_label = new QLabel("");
		sess->dialog->grid->addWidget(sess->rte_label, row, 0);
		row++;

		sess->progress_label = sess->wp_label;
		sess->total_count = -1;

		/* Starting gps read/write thread. */
		sess->run();

		sess->dialog->button_box->button(QDialogButtonBox::Ok)->setDefault(true);
		sess->dialog->exec();

		delete sess->dialog;

	} else {
		if (!this->turn_off) {
			Dialog::info(QObject::tr("No GPS items selected for transfer."), layer->get_window());
		}
	}

	sess->mutex.lock();

	if (sess->in_progress) {
		sess->in_progress = false;   /* Tell thread to stop. */
		sess->mutex.unlock();
	} else {
		if (this->turn_off) {
			/* No need for thread for powering off device (should be quick operation...) - so use babel command directly: */
			BabelTurnOffDevice turn_off_process(this->gps_protocol, this->serial_port);
			if (sg_ret::ok != turn_off_process.run_process()) {
				Dialog::error(QObject::tr("Could not turn off device."), layer->get_window());
			}
		}
		sess->mutex.unlock();
		delete sess;
	}

	return 0;
}




void LayerGPS::gps_upload_cb(void)
{
	qDebug() << SG_PREFIX_D << "";

	GisViewport * gisview = this->get_window()->get_main_gis_view();
	LayerTRW * trw = this->trw_children[GPS_CHILD_LAYER_TRW_UPLOAD];

	this->upload.run_transfer(trw, NULL, gisview, false);
}




void LayerGPS::gps_download_cb(void) /* Slot. */
{
	qDebug() << SG_PREFIX_D << "";

	GisViewport * gisview = this->get_window()->get_main_gis_view();
	LayerTRW * trw = this->trw_children[GPS_CHILD_LAYER_TRW_DOWNLOAD];

	this->download.run_transfer(trw, NULL, gisview,
#if REALTIME_GPS_TRACKING_ENABLED
			      this->realtime_tracking_in_progress
#else
			      false
#endif
			      );
}




/**
   @reviewed-on: 2019-03-23
*/
void LayerGPS::gps_empty_upload_cb(void)
{
	/* Get confirmation from the user. */
	if (!Dialog::yes_or_no(tr("Are you sure you want to delete GPS Upload data?"), ThisApp::get_main_window())) {
		return;
	}

	this->trw_children[GPS_CHILD_LAYER_TRW_UPLOAD]->delete_all_waypoints();
	this->trw_children[GPS_CHILD_LAYER_TRW_UPLOAD]->delete_all_tracks();
	this->trw_children[GPS_CHILD_LAYER_TRW_UPLOAD]->delete_all_routes();
}




/**
   @reviewed-on: 2019-03-23
*/
void LayerGPS::gps_empty_download_cb(void)
{
	/* Get confirmation from the user. */
	if (!Dialog::yes_or_no(tr("Are you sure you want to delete GPS Download data?"), ThisApp::get_main_window())) {
		return;
	}

	this->trw_children[GPS_CHILD_LAYER_TRW_DOWNLOAD]->delete_all_waypoints();
	this->trw_children[GPS_CHILD_LAYER_TRW_DOWNLOAD]->delete_all_tracks();
	this->trw_children[GPS_CHILD_LAYER_TRW_DOWNLOAD]->delete_all_routes();
}




#if REALTIME_GPS_TRACKING_ENABLED
/**
   @reviewed-on: 2019-03-23
*/
void LayerGPS::rt_empty_realtime_cb(void)
{
	/* Get confirmation from the user. */
	if (!Dialog::yes_or_no(tr("Are you sure you want to delete GPS Realtime data?"), ThisApp::get_main_window())) {
		return;
	}

	this->trw_children[GPS_CHILD_LAYER_TRW_REALTIME]->delete_all_waypoints();
	this->trw_children[GPS_CHILD_LAYER_TRW_REALTIME]->delete_all_tracks();
	/* There are no routes in Realtime TRW layer. */
}
#endif




/**
   @reviewed-on: 2019-03-23
*/
void LayerGPS::gps_empty_all_cb(void) /* Slot. */
{
	/* Get confirmation from the user. */
	if (!Dialog::yes_or_no(tr("Are you sure you want to delete all GPS data?"), ThisApp::get_main_window())) {
		return;
	}

	this->trw_children[GPS_CHILD_LAYER_TRW_UPLOAD]->delete_all_waypoints();
	this->trw_children[GPS_CHILD_LAYER_TRW_UPLOAD]->delete_all_tracks();
	this->trw_children[GPS_CHILD_LAYER_TRW_UPLOAD]->delete_all_routes();
	this->trw_children[GPS_CHILD_LAYER_TRW_DOWNLOAD]->delete_all_waypoints();
	this->trw_children[GPS_CHILD_LAYER_TRW_DOWNLOAD]->delete_all_tracks();
	this->trw_children[GPS_CHILD_LAYER_TRW_DOWNLOAD]->delete_all_routes();
#if REALTIME_GPS_TRACKING_ENABLED
	this->trw_children[GPS_CHILD_LAYER_TRW_REALTIME]->delete_all_waypoints();
	this->trw_children[GPS_CHILD_LAYER_TRW_REALTIME]->delete_all_tracks();
	/* There are no routes in Realtime TRW layer. */
#endif
}




#if REALTIME_GPS_TRACKING_ENABLED
void LayerGPS::rt_tracking_draw(GisViewport * gisview, const RTData & rt_data)
{
	if (std::isnan(rt_data.fix.track)) {
		qDebug() << SG_PREFIX_N << "Skipping drawing - no fix track";
		return;
	}


	/* Get a bbox that is a bit larger (by a margin) than the
	   viewport, so that points that are close to viewport border
	   are drawn nicely. */
	const int margin = 20;
	const LatLonBBox bbox = gisview->get_bbox(-margin, -margin, margin, margin);
	if (!bbox.contains_point(rt_data.lat_lon)) {
		qDebug() << SG_PREFIX_N << "Skipping drawing - point outside of viewport";
		return;
	}

	Coord gps_coord = rt_data.coord;
	gps_coord.recalculate_to_mode(gisview->get_coord_mode()); /* Just in case if current coordinate mode is UTM and data from GPS is LatLon. */

	ScreenPos screen_pos;
	gisview->coord_to_screen_pos(gps_coord, screen_pos);

	double heading_cos = cos(DEG2RAD(rt_data.fix.track));
	double heading_sin = sin(DEG2RAD(rt_data.fix.track));

	int half_back_y = screen_pos.y() + 8 * heading_cos;
	int half_back_x = screen_pos.x() - 8 * heading_sin;
	int half_back_bg_y = screen_pos.y() + 10 * heading_cos;
	int half_back_bg_x = screen_pos.x() - 10 * heading_sin;

	int pt_y = half_back_y - 24 * heading_cos;
	int pt_x = half_back_x + 24 * heading_sin;
	//ptbg_y = half_back_bg_y - 28 * heading_cos;
	int ptbg_x = half_back_bg_x + 28 * heading_sin;

	int side1_y = half_back_y + 9 * heading_sin;
	int side1_x = half_back_x + 9 * heading_cos;
	int side1bg_y = half_back_bg_y + 11 * heading_sin;
	int side1bg_x = half_back_bg_x + 11 * heading_cos;

	int side2_y = half_back_y - 9 * heading_sin;
	int side2_x = half_back_x - 9 * heading_cos;
	int side2bg_y = half_back_bg_y - 11 * heading_sin;
	int side2bg_x = half_back_bg_x - 11 * heading_cos;

	ScreenPos trian[3] = { ScreenPos(pt_x, pt_y), ScreenPos(side1_x, side1_y), ScreenPos(side2_x, side2_y) };
	ScreenPos trian_bg[3] = { ScreenPos(ptbg_x, pt_y), ScreenPos(side1bg_x, side1bg_y), ScreenPos(side2bg_x, side2bg_y) };

	//QPen const & pen, ScreenPos const * points, int npoints, bool filled

	gisview->draw_polygon(this->realtime_track_bg_pen, trian_bg, 3, true);
	gisview->draw_polygon(this->realtime_track_pen, trian, 3, true);
	gisview->fill_rectangle((rt_data.fix.mode > MODE_2D) ? this->realtime_track_pt2_pen.color() : this->realtime_track_pt1_pen.color(), screen_pos.x() - 2, screen_pos.y() - 2, 4, 4);

	//this->realtime_track_pt_pen = (this->realtime_track_pt_pen == this->realtime_track_pt1_pen) ? this->realtime_track_pt2_pen : this->realtime_track_pt1_pen;
}




Trackpoint * LayerGPS::rt_create_trackpoint(bool record_every_tp)
{
	/* Note that fix.time is a double, but it should not affect
	   the precision for most GPS. */
	const time_t cur_timestamp = this->current_rt_data.fix.time;
	const time_t last_timestamp = this->previous_rt_data.fix.time;

	if (cur_timestamp < last_timestamp) {
		qDebug() << SG_PREFIX_N << "Not generating tp, timestamps mismatch:" << cur_timestamp << last_timestamp;
		return NULL;
	}

	if (!this->realtime_record) {
		/* We don't "record" real time gps data, so there is
		   no need to create a trackpoint. */
		qDebug() << SG_PREFIX_N << "Not generating tp, not in 'realtime record' mode";
		return NULL;
	}

	if (this->current_rt_data.saved_to_track) {
		/* Current position obtained from gpsd has been
		   already used to generate a trackpoint. */
		qDebug() << SG_PREFIX_N << "Not generating tp from the same data again";
		return NULL;
	}


	bool create_this_tp = false;

#define CURRENT_HAS_BETTER_FIX  (this->current_rt_data.fix.mode > MODE_2D && this->previous_rt_data.fix.mode <= MODE_2D)

	if (!this->realtime_track->empty() && CURRENT_HAS_BETTER_FIX && (cur_timestamp - last_timestamp) < 2) {
		auto last_tp = std::prev(this->realtime_track->end());
		delete *last_tp;
		this->realtime_track->trackpoints.erase(last_tp);
		create_this_tp = true;
	}

	if (!create_this_tp && record_every_tp) {
		create_this_tp = true;
	}

	if (!create_this_tp && (cur_timestamp != last_timestamp)) {
		if (std::isnan(this->current_rt_data.fix.track) || std::isnan(this->previous_rt_data.fix.track)) {
			/* Can't determine if heading has changed by
			   some margin. Better record this tp to be
			   safe. */
			create_this_tp = true;
		} else {
			const double diff = this->current_rt_data.fix.track - this->previous_rt_data.fix.track;
			if (std::fabs(diff) > 3.0) {
				/* Current heading has changed
				   significantly since last recorded
				   tp. */
				create_this_tp = true;
			}
		}
	}


	const Altitude alt = Altitude(this->current_rt_data.fix.altitude, HeightUnit::Metres); /* Altitude::is_valid() may return false. */
	if (!create_this_tp && (cur_timestamp != last_timestamp)) {
		const Altitude last_alt = Altitude(this->previous_rt_data.fix.altitude, HeightUnit::Metres); /* Altitude::is_valid() may return false. */

		if (!alt.is_valid() || !last_alt.is_valid()) {
			/* Can't determine if altitude has changed by
			   some margin. Better record this tp to be
			   safe. */
			create_this_tp = true;
		} else {
			if (alt.floor() != last_alt.floor()) {
				/* Current altitude has changed
				   significantly since last recorded
				   tp. */
				create_this_tp = true;
			}
		}
	}

	if (create_this_tp) {
		/* TODO_LATER: check for new segments. */
		Trackpoint * new_tp = new Trackpoint();
		new_tp->newsegment = false;
		new_tp->set_timestamp(this->current_rt_data.fix.time);
		new_tp->altitude = alt;
		/* Speed only available for 3D fix. Check for NAN when use this speed. */
		new_tp->gps_speed = this->current_rt_data.fix.speed;
		new_tp->course = Angle(this->current_rt_data.fix.track, AngleUnit::Degrees); /* From the lecture of code of gpsd package, it seems that 'track' is in degrees (e.g. "Course over ground, degrees from true north."). */
		new_tp->nsats = this->current_rt_data.satellites_used;
		new_tp->fix_mode = (GPSFixMode) this->current_rt_data.fix.mode;

		new_tp->coord = this->current_rt_data.coord;

		this->realtime_track->add_trackpoint(new_tp, true); /* Ensure bounds is recalculated. */
		this->current_rt_data.saved_to_track = true;
		this->current_rt_data.satellites_used = 0;
		this->previous_rt_data = this->current_rt_data;
		return new_tp;
	} else {
		qDebug() << SG_PREFIX_N << "Not generating tp - other reason";
		return NULL;
	}
}




/**
   @reviewed-on: 2019-03-22
*/
void LayerGPS::rt_update_statusbar(Window * window)
{
	const QString msg = vu_trackpoint_formatted_message(this->statusbar_format_code, this->tp, this->tp_prev, this->realtime_track, this->previous_rt_data.fix.climb);
	window->get_statusbar()->set_message(StatusBarField::Info, msg);
}




/**
   @reviewed-on: 2019-03-23
*/
void LayerGPS::rt_gpsd_raw_hook(void)
{
	if (!this->realtime_tracking_in_progress) {
		qDebug() << SG_PREFIX_W << "Receiving GPS data while not in realtime mode";
		return;
	}
	if (this->gpsdata.fix.mode < MODE_2D) {
		qDebug() << SG_PREFIX_N << "Ignoring raw data - fix worse than 2D:" << (int) this->gpsdata.fix.mode;
		return;
	}


	if (sg_ret::ok != this->current_rt_data.set(this->gpsdata, this->trw_children[GPS_CHILD_LAYER_TRW_REALTIME]->get_coord_mode())) {
		qDebug() << SG_PREFIX_N << "Can't initialize current_rt_data with current gps data";
		return;
	}


	Window * window = this->get_window();
	GisViewport * gisview = window->get_main_gis_view();
	bool viewport_shifted = false;

	if ((this->vehicle_position == VehiclePosition::Centered) ||
	    (this->realtime_jump_to_start && this->first_realtime_trackpoint)) {
		gisview->set_center_coord(this->current_rt_data.coord, false);
		viewport_shifted = true;
	} else if (this->vehicle_position == VehiclePosition::OnScreen) {
		const int hdiv = 6;
		const int vdiv = 6;
		const int px = 20; /* Adjustment in pixels to make sure vehicle is inside the box. */
		const int width = gisview->central_get_width(); /* TODO_LATER: shouldn't we get central x/y pixels here? */
		const int height = gisview->central_get_height();
		fpixel vx, vy;

		if (sg_ret::ok == gisview->coord_to_screen_pos(this->current_rt_data.coord, &vx, &vy)) {
			viewport_shifted = true;
			if (vx < (width/hdiv)) {
				gisview->set_center_coord(vx - width/2 + width/hdiv + px, vy);

			} else if (vx > (width - width/hdiv)) {
				gisview->set_center_coord(vx + width/2 - width/hdiv - px, vy);

			} else if (vy < (height/vdiv)) {
				gisview->set_center_coord(vx, vy - height/2 + height/vdiv + px);

			} else if (vy > (height - height/vdiv)) {
				gisview->set_center_coord(vx, vy + height/2 - height/vdiv - px);

			} else {
				viewport_shifted = false;
			}
		}
	} else {
		/* VehiclePosition::None */
	}

	this->first_realtime_trackpoint = false;

	this->tp = this->rt_create_trackpoint(false);

	if (this->tp) {
		if (this->realtime_update_statusbar) {
			this->rt_update_statusbar(window);
		}
		this->tp_prev = this->tp;
	}

	/* Update from background thread. */
	if (viewport_shifted) {
		this->emit_tree_item_changed("Requesting redrawing of all layers because viewport has been shifted after adding new tp");
	} else {
		this->trw_children[GPS_CHILD_LAYER_TRW_REALTIME]->emit_tree_item_changed("Requesting redrawing of Realtime TRW Layer after adding new tp");
	}
}




static int gpsd_data_available(GIOChannel *source, GIOCondition condition, void * gps_layer)
{
	qDebug() << SG_PREFIX_D << "-- realtime tracking";

	LayerGPS * layer = (LayerGPS *) gps_layer;

	if (condition == G_IO_IN) {
		if (gps_read(&layer->gpsdata) > -1) {
			/* Reuse old function to perform operations on the new GPS data. */
			layer->rt_gpsd_raw_hook();
			return true;
		} else {
			qDebug() << SG_PREFIX_W << "Disconnected from gpsd. Will call disconnect()";
			layer->rt_gpsd_disconnect();
			/* We can't restart timer because it may take
			   N seconds before the timer is re-triggered,
			   callback is called, and we re-gain
			   connection to gpsd. This would take too
			   long. Instead call the callback directly
			   (this will arm the timer for re-tries if
			   necessary). */
			qDebug() << SG_PREFIX_W << "Disconnected from gpsd. Will try to reconnect";
			layer->rt_gpsd_connect_periodic_retry_cb();
		}
	}

	return false; /* No further calling. */
}




bool LayerGPS::rt_gpsd_connect_try_once(void)
{
	char port[sizeof ("65535")] = { 0 };
	snprintf(port, sizeof (port), "%d", this->gpsd_port);

	if (0 != gps_open(this->gpsd_host.toUtf8().constData(), port, &this->gpsdata)) {
		qDebug() << QObject::tr("WW: Layer GPS: Failed to connect to gpsd at '%1' (port %2): %3.")
			.arg(this->gpsd_host).arg(this->gpsd_port).arg(gps_errstr(errno));
		return false; /* Failed to connect, re-start timer. */
	}
	this->gpsdata_opened = true;
	this->current_rt_data.reset();
	this->previous_rt_data.reset();

	if (this->realtime_record) {
		LayerTRW * trw = this->trw_children[GPS_CHILD_LAYER_TRW_REALTIME];
		this->realtime_track = new Track(false);
		const QString track_name = trw->tracks.new_unique_element_name(QObject::tr("REALTIME"));
		this->realtime_track->set_name(track_name);
		trw->add_track(this->realtime_track);
	}

	this->realtime_io_channel = g_io_channel_unix_new(this->gpsdata.gps_fd);
	this->realtime_io_watch_id = g_io_add_watch(this->realtime_io_channel,
						    (GIOCondition) (G_IO_IN | G_IO_ERR | G_IO_HUP), gpsd_data_available, this);

	gps_stream(&this->gpsdata, WATCH_ENABLE, NULL);

	return true; /* Connection succeeded, no need to restart timer. */
}




/**
   @reviewed-on: 2019-03-23
*/
bool LayerGPS::rt_ask_retry(void)
{
	const QString message = tr("Failed to connect to gpsd at %1 (port %2)\n"
				   "Should Viking keep trying (every %3 seconds)?")
		.arg(this->gpsd_host)
		.arg(this->gpsd_port)
		.arg(this->gpsd_retry_interval.get_ll_value());

	const int reply = QMessageBox::question(this->get_window(), tr("GPS Layer"), message);
	return (reply == QMessageBox::Yes);
}




bool LayerGPS::rt_gpsd_connect_periodic_retry_cb(void)
{
	qDebug() << SG_PREFIX_D << "";

	this->realtime_retry_timer.stop();

	qDebug() << SG_PREFIX_I << "Periodic retry: will now try single connect in this time period";
	if (true == this->rt_gpsd_connect_try_once()) {
		qDebug() << SG_PREFIX_I << "Periodic retry: connected";
		return true;
	}

	if (this->gpsd_retry_interval.get_ll_value() <= 0) {
		qDebug() << SG_PREFIX_I << "Periodic retry: failed to connect, but interval is zero, not re-trying.";
		return false;
	}

	/* Re-start timer. */
	qDebug() << SG_PREFIX_I << "Periodic retry: failed to connect, re-arming timer for" << this->gpsd_retry_interval.to_string() << "seconds";
	this->realtime_retry_timer.start(MSECS_PER_SEC * this->gpsd_retry_interval.get_ll_value());

	return false;
}




void LayerGPS::rt_gpsd_disconnect(void)
{
	qDebug() << SG_PREFIX_D << "";

	this->realtime_retry_timer.stop();

	this->rt_gpsd_disconnect_inner();

	this->rt_cleanup_layer_children();
}




/**
   @reviewed-on: 2019-03-23
*/
void LayerGPS::rt_gpsd_disconnect_inner(void)
{
	if (this->realtime_io_watch_id) {
		g_source_remove(this->realtime_io_watch_id);
		this->realtime_io_watch_id = 0;
	}
	if (this->realtime_io_channel) {
		GError * error = NULL;
		g_io_channel_shutdown(this->realtime_io_channel, false, &error);
		this->realtime_io_channel = NULL;
	}

	if (this->gpsdata_opened) {
		gps_stream(&this->gpsdata, WATCH_DISABLE, NULL);
		gps_close(&this->gpsdata);
		this->gpsdata_opened = false;
	}

	return;
}




/**
   @reviewed-on: 2019-03-23
*/
void LayerGPS::rt_cleanup_layer_children(void)
{
	/* Remove current track if it was created but it never
	   "received" any trackpoints from gpsd (i.e. is empty). */
	if (this->realtime_record && this->realtime_track) {
		if (this->realtime_track->empty()) {
			this->trw_children[GPS_CHILD_LAYER_TRW_REALTIME]->detach_from_container(this->realtime_track);
			this->trw_children[GPS_CHILD_LAYER_TRW_REALTIME]->detach_from_tree(this->realtime_track);
			delete this->realtime_track;
		}
		this->realtime_track = NULL;
	}

	return;
}




void LayerGPS::rt_start_stop_tracking_cb(void)
{
	qDebug() << SG_PREFIX_D << "";

	/* Make sure we are still in the boat with libgps. */
	assert ((((int) GPSFixMode::Fix2D) == MODE_2D) && (((int) GPSFixMode::Fix3D) == MODE_3D));

	if (this->realtime_tracking_in_progress) {
		this->rt_stop_tracking();
		return;
	} else {
		/* Start realtime tracking. */
		this->realtime_tracking_in_progress = true;

		if (true == this->rt_gpsd_connect_try_once()) {
			/* All ok at first try. */
			this->first_realtime_trackpoint = true;
			return;
		}

		if (this->gpsd_retry_interval.get_ll_value() <= 0) {
			/* Failed to start tracking, and we can't retry. */
			qDebug() << SG_PREFIX_W << "Failed to connect to gpsd but will not retry because retry intervel was set to" << this->gpsd_retry_interval.to_string() << "(which is 0 or negative).";
			this->realtime_tracking_in_progress = false;
			this->first_realtime_trackpoint = false;
			this->tp = NULL;
			return;
		}

		if (!this->rt_ask_retry()) {
			/* User doesn't want to re-try periodically. */
			this->realtime_tracking_in_progress = false;
			this->first_realtime_trackpoint = false;
			this->tp = NULL;
			return;
		}

		/* Try repeatedly in the future, every N seconds. */
		this->realtime_retry_timer.start(MSECS_PER_SEC * this->gpsd_retry_interval.get_ll_value());

		return;
	}
}




void LayerGPS::rt_stop_tracking(void)
{
	this->realtime_tracking_in_progress = false;
	this->realtime_retry_timer.stop();
	this->first_realtime_trackpoint = false;
	this->tp = NULL;
	this->rt_gpsd_disconnect();

	return;
}

#endif /* REALTIME_GPS_TRACKING_ENABLED */




LayerGPS::LayerGPS()
{
	this->m_kind = LayerKind::GPS;
	strcpy(this->debug_string, "GPS");
	this->interface = &vik_gps_layer_interface;

#if REALTIME_GPS_TRACKING_ENABLED
	this->realtime_track_pen.setColor(QColor("#203070"));
	this->realtime_track_pen.setWidth(2);

	this->realtime_track_bg_pen.setColor(QColor("grey"));
	this->realtime_track_bg_pen.setWidth(2);

	this->realtime_track_pt1_pen.setColor(QColor("red"));
	this->realtime_track_pt1_pen.setWidth(2);

	this->realtime_track_pt2_pen.setColor(QColor("green"));
	this->realtime_track_pt2_pen.setWidth(2);

	this->realtime_track_pt_pen = this->realtime_track_pt1_pen;

	this->gpsd_host = QObject::tr("host");

	this->realtime_retry_timer.setSingleShot(true);
	connect(&this->realtime_retry_timer, SIGNAL (timeout(void)), this, SLOT (rt_gpsd_connect_periodic_retry_cb()));

#endif /* REALTIME_GPS_TRACKING_ENABLED */

	if (!ApplicationState::get_string(VIK_SETTINGS_GPS_STATUSBAR_FORMAT, this->statusbar_format_code)) {
		/* Otherwise use default. */
		this->statusbar_format_code = "GSA";
		ApplicationState::set_string(VIK_SETTINGS_GPS_STATUSBAR_FORMAT, this->statusbar_format_code);
	}

	this->set_initial_parameter_values();
	this->set_name(Layer::get_translated_layer_kind_string(this->m_kind));

	for (int i = 0; i < GPS_CHILD_LAYER_MAX; i++) {
		this->trw_children[i] = new LayerTRW();
		this->trw_children[i]->set_name(trw_names[i].label);

		/* No cutting or deleting of TRW sublayers. */
		MenuOperation ops = this->trw_children[i]->get_menu_operation_ids();
		ops = (MenuOperation) (ops & (~MenuOperationCut));
		ops = (MenuOperation) (ops & (~MenuOperationDelete));
		this->trw_children[i]->set_menu_operation_ids(ops);
	}
}




/* To be called right after constructor. */
void LayerGPS::set_coord_mode(CoordMode new_mode)
{
	qDebug() << SG_PREFIX_D << "";

	for (int i = 0; i < GPS_CHILD_LAYER_MAX; i++) {
		this->trw_children[i]->set_coord_mode(new_mode);
	}
}




/* Doesn't set the trigger. Should be done by aggregate layer when child emits 'changed' signal. */
void LayerGPS::child_tree_item_changed_cb(const QString & child_tree_item_name) /* Slot. */
{
	qDebug() << SG_PREFIX_D << "-- realtime tracking";

	qDebug() << SG_PREFIX_SLOT << "Layer" << this->name << "received 'child tree item changed' signal from" << child_tree_item_name;
	if (this->is_visible()) {
		/* TODO_LATER: this can used from the background - e.g. in acquire
		   so will need to flow background update status through too. */
		qDebug() << SG_PREFIX_SIGNAL << "Layer" << this->name << "emits 'changed' signal";
		emit this->tree_item_changed(this->get_name());
	}
}




/**
   @reviewed-on: 2019-03-22
*/
sg_ret RTData::set(struct gps_data_t & gpsdata, CoordMode coord_mode)
{
	this->fix = gpsdata.fix;
	this->satellites_used = gpsdata.satellites_used;
	this->saved_to_track = false;


	this->lat_lon = LatLon(this->fix.latitude, this->fix.longitude);
	if (!this->lat_lon.is_valid()) {
		qDebug() << SG_PREFIX_W << "Invalid latitude or longitude in gps fix";
		return sg_ret::err;
	}
	this->coord = Coord(this->lat_lon, coord_mode);


	return sg_ret::ok;
}




/**
   @reviewed-on: 2019-03-22
*/
void RTData::reset(void)
{
	this->saved_to_track = true;
	/* Track alt/time graph uses NAN value as indicator of invalid altitude/speed. */
	this->fix.altitude = NAN;
	this->fix.speed = NAN;
}
