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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <vector>
#include <list>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <cstring>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <QDebug>

//#include <glib/gstdio.h>
//#include <glib/gprintf.h>

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




#define PREFIX " Layer GPS:" << __FUNCTION__ << __LINE__ << ">"




extern Tree * g_tree;




/* Shouldn't need to use these much any more as the protocol is now saved as a string.
   They are kept for compatibility loading old .vik files */
typedef enum {
	GARMIN_P = 0,
	MAGELLAN_P,
	DELORME_P,
	NAVILINK_P,
	OLD_NUM_PROTOCOLS
} vik_gps_proto;




static std::vector<SGLabelID> protocols_args = {
	SGLabelID("garmin",   0),
	SGLabelID("magellan", 1),
	SGLabelID("delbin",   2),
	SGLabelID("navilink", 3)
};




static std::vector<SGLabelID> trw_names = {
	SGLabelID(QObject::tr("GPS Download"),          GPS_CHILD_LAYER_TRW_DOWNLOAD),
	SGLabelID(QObject::tr("GPS Upload"),            GPS_CHILD_LAYER_TRW_UPLOAD),
#if REALTIME_GPS_TRACKING_ENABLED
	SGLabelID(QObject::tr("GPS Realtime Tracking"), GPS_CHILD_LAYER_TRW_REALTIME)
#endif
};




#ifdef WINDOWS
static std::vector<SGLabelID> params_ports = {
	SGLabelID("com1", 0),
	SGLabelID("usb:", 1),
};
#else
static std::vector<SGLabelID> params_ports = {
	SGLabelID("/dev/ttyS0",   1),
	SGLabelID("/dev/ttyS1",   2),
	SGLabelID("/dev/ttyUSB0", 3),
	SGLabelID("/dev/ttyUSB1", 4),
	SGLabelID("usb:",         5),
};
#endif




/* For compatibility with previous versions. */
#ifdef WINDOWS
static std::vector<SGLabelID> old_params_ports = {
	SGLabelID("com1", 0),
	SGLabelID("usb:", 1)
};
#else
static std::vector<SGLabelID> old_params_ports = {
	SGLabelID("/dev/ttyS0",   0),
	SGLabelID("/dev/ttyS1",   1),
	SGLabelID("/dev/ttyUSB0", 2),
	SGLabelID("/dev/ttyUSB1", 3),
	SGLabelID("usb:",         4)
};
#endif




enum {
	GROUP_DATA_MODE,
	GROUP_REALTIME_MODE
};

static const char * g_params_groups[] = {
	"Data Mode",
#if REALTIME_GPS_TRACKING_ENABLED
	"Realtime Tracking Mode",
#endif
};




static SGVariant gps_protocol_default(void)
{
	return SGVariant("garmin");
}




static SGVariant gps_port_default(void)
{
	SGVariant data("usb:");
#ifndef WINDOWS
	/* Attempt to auto set default USB serial port entry. */
	/* Ordered to make lowest device favourite if available. */
	if (access("/dev/ttyUSB1", R_OK) == 0) {
		data = SGVariant("/dev/ttyUSB1");
	}
	if (access("/dev/ttyUSB0", R_OK) == 0) {
		data = SGVariant("/dev/ttyUSB0");
	}
#endif
	return data;
}




#if REALTIME_GPS_TRACKING_ENABLED

enum {
	VEHICLE_POSITION_CENTERED = 0,
	VEHICLE_POSITION_ON_SCREEN,
	VEHICLE_POSITION_NONE,
};
static std::vector<SGLabelID> params_vehicle_position = {
	SGLabelID(QObject::tr("Keep vehicle at center"), VEHICLE_POSITION_CENTERED),
	SGLabelID(QObject::tr("Keep vehicle on screen"), VEHICLE_POSITION_ON_SCREEN),
	SGLabelID(QObject::tr("Disable"),                VEHICLE_POSITION_NONE)
};




static SGVariant moving_map_method_default(void)   { return SGVariant((int32_t) VEHICLE_POSITION_ON_SCREEN); }
static SGVariant gpsd_host_default(void)           { return SGVariant("localhost"); }
static SGVariant gpsd_port_default(void)           { return SGVariant(DEFAULT_GPSD_PORT); }
static SGVariant gpsd_retry_interval_default(void) { return SGVariant("10"); }

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
	/* NB gps_layer_inst_init() is performed after parameter registeration
	   thus to give the protocols some potential values use the old static list. */
	/* TODO: find another way to use gps_layer_inst_init()? */
	{ PARAM_PROTOCOL,                   NULL, "gps_protocol",              SGVariantType::String,  GROUP_DATA_MODE,     QObject::tr("GPS Protocol:"),                     WidgetType::ComboBox,      &protocols_args,         gps_protocol_default,        NULL, NULL }, // List reassigned at runtime
	{ PARAM_PORT,                       NULL, "gps_port",                  SGVariantType::String,  GROUP_DATA_MODE,     QObject::tr("Serial Port:"),                      WidgetType::ComboBox,      &params_ports,           gps_port_default,            NULL, NULL },
	{ PARAM_DOWNLOAD_TRACKS,            NULL, "gps_download_tracks",       SGVariantType::Boolean, GROUP_DATA_MODE,     QObject::tr("Download Tracks:"),                  WidgetType::CheckButton,   NULL,                    sg_variant_true,             NULL, NULL },
	{ PARAM_UPLOAD_TRACKS,              NULL, "gps_upload_tracks",         SGVariantType::Boolean, GROUP_DATA_MODE,     QObject::tr("Upload Tracks:"),                    WidgetType::CheckButton,   NULL,                    sg_variant_true,             NULL, NULL },
	{ PARAM_DOWNLOAD_ROUTES,            NULL, "gps_download_routes",       SGVariantType::Boolean, GROUP_DATA_MODE,     QObject::tr("Download Routes:"),                  WidgetType::CheckButton,   NULL,                    sg_variant_true,             NULL, NULL },
	{ PARAM_UPLOAD_ROUTES,              NULL, "gps_upload_routes",         SGVariantType::Boolean, GROUP_DATA_MODE,     QObject::tr("Upload Routes:"),                    WidgetType::CheckButton,   NULL,                    sg_variant_true,             NULL, NULL },
	{ PARAM_DOWNLOAD_WAYPOINTS,         NULL, "gps_download_waypoints",    SGVariantType::Boolean, GROUP_DATA_MODE,     QObject::tr("Download Waypoints:"),               WidgetType::CheckButton,   NULL,                    sg_variant_true,             NULL, NULL },
	{ PARAM_UPLOAD_WAYPOINTS,           NULL, "gps_upload_waypoints",      SGVariantType::Boolean, GROUP_DATA_MODE,     QObject::tr("Upload Waypoints:"),                 WidgetType::CheckButton,   NULL,                    sg_variant_true,             NULL, NULL },
#if REALTIME_GPS_TRACKING_ENABLED
	{ PARAM_REALTIME_REC,               NULL, "record_tracking",           SGVariantType::Boolean, GROUP_REALTIME_MODE, QObject::tr("Recording tracks"),                  WidgetType::CheckButton,   NULL,                    sg_variant_true,             NULL, NULL },
	{ PARAM_REALTIME_CENTER_START,      NULL, "center_start_tracking",     SGVariantType::Boolean, GROUP_REALTIME_MODE, QObject::tr("Jump to current position on start"), WidgetType::CheckButton,   NULL,                    sg_variant_false,            NULL, NULL },
	{ PARAM_VEHICLE_POSITION,           NULL, "moving_map_method",         SGVariantType::Int,     GROUP_REALTIME_MODE, QObject::tr("Moving Map Method:"),                WidgetType::RadioGroup,    &params_vehicle_position,moving_map_method_default,   NULL, NULL },
	{ PARAM_REALTIME_UPDATE_STATUSBAR,  NULL, "realtime_update_statusbar", SGVariantType::Boolean, GROUP_REALTIME_MODE, QObject::tr("Update Statusbar:"),                 WidgetType::CheckButton,   NULL,                    sg_variant_true,             NULL, N_("Display information in the statusbar on GPS updates") },
	{ PARAM_GPSD_HOST,                  NULL, "gpsd_host",                 SGVariantType::String,  GROUP_REALTIME_MODE, QObject::tr("Gpsd Host:"),                        WidgetType::Entry,         NULL,                    gpsd_host_default,           NULL, NULL },
	{ PARAM_GPSD_PORT,                  NULL, "gpsd_port",                 SGVariantType::String,  GROUP_REALTIME_MODE, QObject::tr("Gpsd Port:"),                        WidgetType::Entry,         NULL,                    gpsd_port_default,           NULL, NULL },
	{ PARAM_GPSD_RETRY_INTERVAL,        NULL, "gpsd_retry_interval",       SGVariantType::String,  GROUP_REALTIME_MODE, QObject::tr("Gpsd Retry Interval (seconds):"),    WidgetType::Entry,         NULL,                    gpsd_retry_interval_default, NULL, NULL },
#endif /* REALTIME_GPS_TRACKING_ENABLED */

	{ NUM_PARAMS,                       NULL, NULL,                        SGVariantType::Empty,   PARAMETER_GROUP_GENERIC, QString(""),                                  WidgetType::None,          NULL,                    NULL,                        NULL, NULL }, /* Guard. */
};




GPSSession::GPSSession(GPSDirection dir, LayerTRW * new_trw, Track * new_trk, const QString & new_port, Viewport * new_viewport, bool new_in_progress)
{
	this->direction = dir;
	this->window_title = (this->direction == GPSDirection::DOWN) ? QObject::tr("GPS Download") : QObject::tr("GPS Upload");
	this->trw = new_trw;
	this->trk = new_trk;
	this->port = new_port;
	this->viewport = new_viewport;
	this->in_progress = new_in_progress;
}





LayerGPSInterface vik_gps_layer_interface;




LayerGPSInterface::LayerGPSInterface()
{
	this->parameters_c = gps_layer_param_specs; /* Parameters. */
	this->parameter_groups = g_params_groups;   /* Parameter groups. */

	this->fixed_layer_type_string = "GPS"; /* Non-translatable. */

	// this->action_accelerator = ...; /* Empty accelerator. */
	// this->action_icon = ...; /* Set elsewhere. */

	this->menu_items_selection = LayerMenuItem::ALL;

	this->ui_labels.new_layer = QObject::tr("New GPS Layer");
	this->ui_labels.layer_type = QObject::tr("GPS");
	this->ui_labels.layer_defaults = QObject::tr("Default Settings of GPS Layer");
}




/**
   Overwrite the static setup with dynamically generated GPS Babel device list.
*/
void SlavGPS::layer_gps_init(void)
{
	protocols_args.clear();

	int i = 0;
	for (auto iter = Babel::devices.begin(); iter != Babel::devices.end(); iter++) {
		/* Should be using 'label' property but use
		   'identifier' for now thus don't need to mess around
		   converting 'label' to 'identifier' later on. */
		protocols_args.push_back(SGLabelID((*iter)->identifier, i));
		qDebug() << "II:" PREFIX << "new protocol:" << (*iter)->identifier;
	}
}




QString LayerGPS::get_tooltip()
{
	return QObject::tr("Protocol: %1").arg(this->protocol);
}




#define alm_append(m_obj, m_sz) {					\
		int m_len = (m_sz);					\
		g_byte_array_append(b, (uint8_t *)&m_len, sizeof (m_len)); \
		g_byte_array_append(b, (uint8_t *)(m_obj), m_len);	\
	}

/* "Copy". */
void LayerGPS::marshall(uint8_t ** data, size_t * data_len)
{
	uint8_t *ld;
	size_t ll;
	GByteArray* b = g_byte_array_new();

	this->marshall_params(&ld, &ll);
	alm_append(ld, ll);
	free(ld);

	for (int i = 0; i < GPS_CHILD_LAYER_MAX; i++) {
		Layer::marshall(this->trw_children[i], &ld, &ll);
		if (ld) {
			alm_append(ld, ll);
			free(ld);
		}
	}
	*data = b->data;
	*data_len = b->len;
	g_byte_array_free(b, false);
}
#undef alm_append




/* "Paste". */
Layer * LayerGPSInterface::unmarshall(uint8_t * data, size_t data_len, Viewport * viewport)
{
#define alm_size (*(int *)data)
#define alm_next		 \
	data_len -= sizeof(int) + alm_size;		\
	data += sizeof(int) + alm_size;

	LayerGPS * layer = new LayerGPS();
	layer->set_coord_mode(viewport->get_coord_mode());

	layer->unmarshall_params(data + sizeof (int), alm_size);
	alm_next;

	int i = 0;
	while (data_len > 0 && i < GPS_CHILD_LAYER_MAX) {
		Layer * child_layer = Layer::unmarshall(data + sizeof (int), alm_size, viewport);
		if (child_layer) {
			layer->trw_children[i++] = (LayerTRW *) child_layer;
			/* NB no need to attach signal update handler here
			   as this will always be performed later on in LayerGPS::add_children_to_tree(). */
		}
		alm_next;
	}
	// qDebug() << "II: Layer GPS: LayerGPSInterface::unmarshall() ended with" << data_len;
	assert (data_len == 0);
	return layer;
#undef alm_size
#undef alm_next
}




/*
  Extract an index value from the beginning of a string.
  Index can't be equal to or larger than limit.

  Return index value on success.
  Return -1 on failure.
*/
static int get_legacy_index(const QString & string, int limit)
{
	int idx = -1;

	if (!string.at(0).isNull() && string.at(0).isDigit() && string.at(1).isNull()) {
		idx = string.at(0).digitValue();
	}
	if (idx >= 0 && idx < limit) {
		return idx;
	} else {
		return -1;
	}
}




bool LayerGPS::set_param_value(uint16_t id, const SGVariant & data, bool is_file_operation)
{
	switch (id) {
	case PARAM_PROTOCOL:
		if (!data.val_string.isEmpty()) {
			/* Backwards Compatibility: previous versions <v1.4 stored protocol as an array index. */
			const int idx = get_legacy_index(data.val_string, OLD_NUM_PROTOCOLS);
			if (idx != -1) {
				/* It is a valid index: activate compatibility. */
				this->protocol = protocols_args[idx].label;
			} else {
				this->protocol = data.val_string;
			}
			qDebug() << "DD:" PREFIX << "selected GPS Protocol is" << this->protocol;
		} else {
			qDebug() << "WW:" PREFIX << "unknown GPS Protocol";
		}
		break;
	case PARAM_PORT:
		if (!data.val_string.isEmpty()) {
			/* Backwards Compatibility: previous versions <v0.9.91 stored serial_port as an array index. */
			const int idx = get_legacy_index(data.val_string, old_params_ports.size());
			if (idx != -1) {
				/* It is a valid index: activate compatibility. */
				this->serial_port = old_params_ports[idx].label;
			} else {
				this->serial_port = data.val_string;
			}
			qDebug() << "DD:" PREFIX << "selected Serial Port is" << this->serial_port;
		} else {
			qDebug() << "WW:" PREFIX << "unknown Serial Port device";
		}
		break;
	case PARAM_DOWNLOAD_TRACKS:
		this->download_tracks = data.val_bool;
		break;
	case PARAM_UPLOAD_TRACKS:
		this->upload_tracks = data.val_bool;
		break;
	case PARAM_DOWNLOAD_ROUTES:
		this->download_routes = data.val_bool;
		break;
	case PARAM_UPLOAD_ROUTES:
		this->upload_routes = data.val_bool;
		break;
	case PARAM_DOWNLOAD_WAYPOINTS:
		this->download_waypoints = data.val_bool;
		break;
	case PARAM_UPLOAD_WAYPOINTS:
		this->upload_waypoints = data.val_bool;
		break;
#if REALTIME_GPS_TRACKING_ENABLED
	case PARAM_GPSD_HOST:
		if (!data.val_string.isEmpty()) {
			this->gpsd_host = data.val_string;
		}
		break;
	case PARAM_GPSD_PORT:
		if (!data.val_string.isEmpty()) {
			this->gpsd_port = data.val_string;
		}
		break;
	case PARAM_GPSD_RETRY_INTERVAL:
		this->gpsd_retry_interval = strtol(data.val_string.toUtf8().constData(), NULL, 10);
		break;
	case PARAM_REALTIME_REC:
		this->realtime_record = data.val_bool;
		break;
	case PARAM_REALTIME_CENTER_START:
		this->realtime_jump_to_start = data.val_bool;
		break;
	case PARAM_VEHICLE_POSITION:
		this->vehicle_position = data.val_int;
		break;
	case PARAM_REALTIME_UPDATE_STATUSBAR:
		this->realtime_update_statusbar = data.val_bool;
		break;
#endif /* REALTIME_GPS_TRACKING_ENABLED */
	default:
		qDebug() << "WW: Layer GPS: Set Param Value: unknown parameter" << id;
	}

	return true;
}




SGVariant LayerGPS::get_param_value(param_id_t id, bool is_file_operation) const
{
	SGVariant rv;
	switch (id) {
	case PARAM_PROTOCOL:
		rv = SGVariant(this->protocol);
		break;
	case PARAM_PORT:
		rv = SGVariant(this->serial_port);
		break;
	case PARAM_DOWNLOAD_TRACKS:
		rv = SGVariant(this->download_tracks);
		break;
	case PARAM_UPLOAD_TRACKS:
		rv = SGVariant(this->upload_tracks);
		break;
	case PARAM_DOWNLOAD_ROUTES:
		rv = SGVariant(this->download_routes);
		break;
	case PARAM_UPLOAD_ROUTES:
		rv = SGVariant(this->upload_routes);
		break;
	case PARAM_DOWNLOAD_WAYPOINTS:
		rv = SGVariant(this->download_waypoints);
		break;
	case PARAM_UPLOAD_WAYPOINTS:
		rv = SGVariant(this->upload_waypoints);
		break;
#if REALTIME_GPS_TRACKING_ENABLED
	case PARAM_GPSD_HOST:
		rv = SGVariant(this->gpsd_host);
		break;
	case PARAM_GPSD_PORT:
		rv = SGVariant(this->gpsd_port.isEmpty() ? DEFAULT_GPSD_PORT: this->gpsd_port);
		break;
	case PARAM_GPSD_RETRY_INTERVAL:
		rv = SGVariant(QString("%1").arg(this->gpsd_retry_interval));
		break;
	case PARAM_REALTIME_REC:
		rv = SGVariant(this->realtime_record);
		break;
	case PARAM_REALTIME_CENTER_START:
		rv = SGVariant(this->realtime_jump_to_start);
		break;
	case PARAM_VEHICLE_POSITION:
		rv = SGVariant(this->vehicle_position);
		break;
	case PARAM_REALTIME_UPDATE_STATUSBAR:
		rv = SGVariant(this->realtime_update_statusbar); /* kamilkamil: in viking code there is a mismatch of data types. */
		break;
#endif /* REALTIME_GPS_TRACKING_ENABLED */
	default:
		qDebug() << "WW:" PREFIX << "unknown parameter" << (int) id;
	}

	return rv;
}




void LayerGPS::draw(Viewport * viewport)
{
	Layer * trigger = viewport->get_trigger();

	for (int i = 0; i < GPS_CHILD_LAYER_MAX; i++) {
		LayerTRW * trw = this->trw_children[i];
		if (trw->the_same_object(trigger)) {
			if (viewport->get_half_drawn()) {
				viewport->set_half_drawn(false);
				viewport->snapshot_load();
			} else {
				viewport->snapshot_save();
			}
		}
		if (!viewport->get_half_drawn()) {
			trw->draw_if_visible(viewport);
		}
	}
#if REALTIME_GPS_TRACKING_ENABLED
	if (this->realtime_tracking_in_progress) {
		if (this->the_same_object(trigger)) {
			if (viewport->get_half_drawn()) {
				viewport->set_half_drawn(false);
				viewport->snapshot_load();
			} else {
				viewport->snapshot_save();
			}
		}
		if (!viewport->get_half_drawn()) {
			this->realtime_tracking_draw(viewport);
		}
	}
#endif /* REALTIME_GPS_TRACKING_ENABLED */
}




void LayerGPS::change_coord_mode(CoordMode mode)
{
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
	QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (gps_start_stop_tracking_cb()));
	menu.addAction(action);


	action = new QAction(QObject::tr("Empty &Realtime"), this);
	action->setIcon(QIcon::fromTheme("edit-clear"));
	QObject::connect(action, SIGNAL (triggered(bool)), this, SLOT (gps_empty_realtime_cb(void)));
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
		this->trw_children[i]->unref();
	}
#if REALTIME_GPS_TRACKING_ENABLED
	this->rt_gpsd_disconnect();
#endif /* REALTIME_GPS_TRACKING_ENABLED */
}




void LayerGPS::add_children_to_tree(void)
{
	/* TODO set to garmin by default.
	   if (Babel::devices)
	           device = ((BabelDevice*)g_list_nth_data(Babel::devices, last_active))->name;
	   Need to access uibuild widgets somehow.... */

	if (!this->index.isValid()) {
		qDebug() << "EE:" PREFIX  << "layer is not connected to tree";
		return;
	}

	for (int ix = 0; ix < GPS_CHILD_LAYER_MAX; ix++) {
		LayerTRW * trw = this->trw_children[ix];

		/* TODO: find a way to pass 'above=true' argument to function adding new tree item. */

		/* This call sets TreeItem::index and TreeItem::tree_view of added item. */
		this->tree_view->add_tree_item(this->index, trw, trw_names[ix].label);

		this->tree_view->set_tree_item_timestamp(trw->index, trw->get_timestamp());

		QObject::connect(trw, SIGNAL (layer_changed(const QString &)), this, SLOT (child_layer_changed_cb(const QString &)));
	}
}




std::list<Layer const * > * LayerGPS::get_children(void) const
{
	std::list<Layer const * > * children_ = new std::list<Layer const *>;
	for (int i = GPS_CHILD_LAYER_MAX - 1; i >= 0; i--) {
		children_->push_front((Layer const *) this->trw_children[i]);
	}
	return children_;
}




LayerTRW * LayerGPS::get_a_child()
{
	assert ((this->cur_read_child >= 0) && (this->cur_read_child < GPS_CHILD_LAYER_MAX));

	LayerTRW * trw = this->trw_children[this->cur_read_child];
	if (++(this->cur_read_child) >= GPS_CHILD_LAYER_MAX) {
		this->cur_read_child = 0;
	}
	return(trw);
}




bool LayerGPS::is_empty(void) const
{
	if (this->trw_children[0]) {
		return false;
	}
	return true;
}




void GPSSession::set_total_count(int cnt)
{
	this->mutex.lock();
	if (this->in_progress) {
		QString msg;
		if (this->direction == GPSDirection::DOWN) {
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
	this->mutex.lock();
	if (!this->in_progress) {
		this->mutex.unlock();
		return;
	}

	QString msg;

	if (cnt < this->total_count) {
		QString fmt;
		if (this->direction == GPSDirection::DOWN) {
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
		if (this->direction == GPSDirection::DOWN) {
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
	if (strstr(line, "PRDDAT")) { /* kamilTODO: there is a very similar code in datasource_gps_progress() */
		char **tokens = g_strsplit(line, " ", 0);
		char info[128];
		size_t ilen = 0;
		int n_tokens = 0;

		while (tokens[n_tokens]) {
			n_tokens++;
		}

		/* I'm not entirely clear what information this is trying to get...
		   Obviously trying to decipher some kind of text/naming scheme.
		   Anyway this will be superceded if there is 'Unit:' information. */
		if (n_tokens > 8) {
			for (int i = 8; tokens[i] && ilen < sizeof(info) - 2 && strcmp(tokens[i], "00"); i++) {
				unsigned int ch;
				sscanf(tokens[i], "%x", &ch);
				info[ilen++] = ch;
			}
			info[ilen++] = 0;
			this->set_gps_device_info(info);
		}
		g_strfreev(tokens);
	}

	/* eg: "Unit:\teTrex Legend HCx Software Version 2.90\n" */
	if (strstr(line, "Unit:")) {
		char **tokens = g_strsplit(line, "\t", 0);
		int n_tokens = 0;
		while (tokens[n_tokens]) {
			n_tokens++;
		}

		if (n_tokens > 1) {
			this->set_gps_device_info(tokens[1]);
		}
		g_strfreev(tokens);
	}
}




void GPSSession::import_progress_cb(BabelProgressCode code, void * data)
{
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
	case BabelProgressCode::DiagOutput:
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
	case BabelProgressCode::Completed:
		break;
	default:
		break;
	}
}




void GPSSession::export_progress_cb(BabelProgressCode code, void * data)
{
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
	case BabelProgressCode::DiagOutput:
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
	case BabelProgressCode::Completed:
		break;
	default:
		break;
	}
}




/* Re-implementation of QRunnable::run() */
void GPSSession::run(void)
{
	bool result;

	if (this->direction == GPSDirection::DOWN) {
		ProcessOptions po(this->babel_args, this->port, NULL, NULL);
		result = a_babel_convert_import(this->trw, &po, NULL, this);
	} else {
		result = a_babel_convert_export(this->trw, this->trk, this->babel_args, this->port, this);
	}

	if (!result) {
		this->status_label->setText(QObject::tr("Error: couldn't find gpsbabel."));
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
				if (this->viewport && this->direction == GPSDirection::DOWN) {
					this->trw->post_read(this->viewport, true);
					/* View the data available. */
					this->trw->move_viewport_to_show_all(this->viewport) ;
					this->trw->emit_layer_changed(); /* NB update from background thread. */
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
		delete this; /* TODO: is is valid? Can we delete ourselves here? */
	}
}





/**
 * @layer: The TrackWaypoint layer to operate on
 * @track: Operate on a particular track when specified
 * @dir: The direction of the transfer
 * @protocol: The GPS device communication protocol
 * @port: The GPS serial port
 * @tracking: If tracking then viewport display update will be skipped
 * @viewport: A viewport is required as the display may get updated
 * @panel: A layers panel is needed for uploading as the items maybe modified
 * @do_tracks: Whether tracks shoud be processed
 * @do_waypoints: Whether waypoints shoud be processed
 * @turn_off: Whether we should attempt to turn off the GPS device after the transfer (only some devices support this)
 *
 * Talk to a GPS Device using a thread which updates a dialog with the progress.
 */
int SlavGPS::vik_gps_comm(LayerTRW * layer,
			  Track * trk,
			  GPSDirection dir,
			  const QString & protocol,
			  const QString & port,
			  bool tracking,
			  Viewport * viewport,
			  LayersPanel * panel,
			  bool do_tracks,
			  bool do_routes,
			  bool do_waypoints,
			  bool turn_off)
{
	GPSSession * sess = new GPSSession(dir, layer, trk, port, viewport, true);
	sess->setAutoDelete(false);

	/* This must be done inside the main thread as the uniquify causes screen updates
	   (originally performed this nearer the point of upload in the thread). */
	if (dir == GPSDirection::UP) {
		/* Enforce unique names in the layer upload to the GPS device.
		   NB this may only be a Garmin device restriction (and may be not every Garmin device either...).
		   Thus this maintains the older code in built restriction. */
		if (!sess->trw->uniquify(panel)) {
			sess->trw->get_window()->get_statusbar()->set_message(StatusBarField::INFO,
									      QObject::tr("Warning - GPS Upload items may overwrite each other"));
		}
	}

#if REALTIME_GPS_TRACKING_ENABLED
	sess->realtime_tracking_in_progress = tracking;
#endif

	const QString tracks_arg = do_tracks ? "-t" : "";
	const QString routes_arg = do_routes ? "-r" : "";
	const QString waypoints_arg = do_waypoints ? "-w" : "";

	sess->babel_args = QString("-D 9 %1 %2 %3 -%4 %5")
		.arg(tracks_arg)
		.arg(routes_arg)
		.arg(waypoints_arg)
		.arg((dir == GPSDirection::DOWN) ? "i" : "o")
		.arg(protocol);

	/* Only create dialog if we're going to do some transferring. */
	if (do_tracks || do_waypoints || do_routes) {

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
		if (!turn_off) {
			Dialog::info(QObject::tr("No GPS items selected for transfer."), layer->get_window());
		}
	}

	sess->mutex.lock();

	if (sess->in_progress) {
		sess->in_progress = false;   /* Tell thread to stop. */
		sess->mutex.unlock();
	} else {
		if (turn_off) {
			/* No need for thread for powering off device (should be quick operation...) - so use babel command directly: */
			const QString device_off = QString("-i %1,%2").arg(protocol).arg("power_off");
			ProcessOptions po(device_off, port, NULL, NULL);
			bool result = a_babel_convert_import(NULL, &po, NULL, NULL);
			if (!result) {
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
	LayersPanel * panel = g_tree->tree_get_items_tree();
	Viewport * viewport = this->get_window()->get_viewport();
	LayerTRW * trw = this->trw_children[GPS_CHILD_LAYER_TRW_UPLOAD];

	SlavGPS::vik_gps_comm(trw,
			      NULL,
			      GPSDirection::UP,
			      this->protocol,
			      this->serial_port,
			      false,
			      viewport,
			      panel,
			      this->upload_tracks,
			      this->upload_routes,
			      this->upload_waypoints,
			      false);
}




void LayerGPS::gps_download_cb(void) /* Slot. */
{
	Viewport * viewport = this->get_window()->get_viewport();
	LayerTRW * trw = this->trw_children[GPS_CHILD_LAYER_TRW_DOWNLOAD];

	SlavGPS::vik_gps_comm(trw,
			      NULL,
			      GPSDirection::DOWN,
			      this->protocol,
			      this->serial_port,

#if REALTIME_GPS_TRACKING_ENABLED
			      this->realtime_tracking_in_progress,
#else
			      false,
#endif

			      viewport,
			      NULL,
			      this->download_tracks,
			      this->download_routes,
			      this->download_waypoints,
			      false);
}




void LayerGPS::gps_empty_upload_cb(void)
{
	/* Get confirmation from the user. */
	if (!Dialog::yes_or_no(tr("Are you sure you want to delete GPS Upload data?"), g_tree->tree_get_main_window())) {
		return;
	}

	this->trw_children[GPS_CHILD_LAYER_TRW_UPLOAD]->delete_all_waypoints();
	this->trw_children[GPS_CHILD_LAYER_TRW_UPLOAD]->delete_all_tracks();
	this->trw_children[GPS_CHILD_LAYER_TRW_UPLOAD]->delete_all_routes();
}




void LayerGPS::gps_empty_download_cb(void)
{
	/* Get confirmation from the user. */
	if (!Dialog::yes_or_no(tr("Are you sure you want to delete GPS Download data?"), g_tree->tree_get_main_window())) {
		return;
	}

	this->trw_children[GPS_CHILD_LAYER_TRW_DOWNLOAD]->delete_all_waypoints();
	this->trw_children[GPS_CHILD_LAYER_TRW_DOWNLOAD]->delete_all_tracks();
	this->trw_children[GPS_CHILD_LAYER_TRW_DOWNLOAD]->delete_all_routes();
}




#if REALTIME_GPS_TRACKING_ENABLED
void LayerGPS::gps_empty_realtime_cb(void)
{
	/* Get confirmation from the user. */
	if (!Dialog::yes_or_no(QObject::tr("Are you sure you want to delete GPS Realtime data?"), g_tree->tree_get_main_window())) {
		return;
	}

	this->trw_children[GPS_CHILD_LAYER_TRW_REALTIME]->delete_all_waypoints();
	this->trw_children[GPS_CHILD_LAYER_TRW_REALTIME]->delete_all_tracks();
}
#endif




void LayerGPS::gps_empty_all_cb(void) /* Slot. */
{
	/* Get confirmation from the user. */
	if (!Dialog::yes_or_no(tr("Are you sure you want to delete all GPS data?"), g_tree->tree_get_main_window())) {
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
#endif
}




#if REALTIME_GPS_TRACKING_ENABLED
void LayerGPS::realtime_tracking_draw(Viewport * viewport)
{
	const Coord coord_nw = viewport->screen_pos_to_coord(-20, -20);
	const Coord coord_se = viewport->screen_pos_to_coord(viewport->get_width() + 20, viewport->get_width() + 20);
	const LatLon lnw = coord_nw.get_latlon();
	const LatLon lse = coord_se.get_latlon();

	if (this->realtime_fix.fix.latitude > lse.lat &&
	     this->realtime_fix.fix.latitude < lnw.lat &&
	     this->realtime_fix.fix.longitude > lnw.lon &&
	     this->realtime_fix.fix.longitude < lse.lon &&
	     !std::isnan(this->realtime_fix.fix.track)) {

		const Coord gps_coord(LatLon(this->realtime_fix.fix.latitude, this->realtime_fix.fix.longitude), viewport->get_coord_mode());
		const ScreenPos screen_pos = viewport->coord_to_screen_pos(gps_coord);

		double heading_cos = cos(DEG2RAD(this->realtime_fix.fix.track));
		double heading_sin = sin(DEG2RAD(this->realtime_fix.fix.track));

		int half_back_y = screen_pos.y + 8 * heading_cos;
		int half_back_x = screen_pos.x - 8 * heading_sin;
		int half_back_bg_y = screen_pos.y + 10 * heading_cos;
		int half_back_bg_x = screen_pos.x - 10 * heading_sin;

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

		QPoint trian[3] = { QPoint(pt_x, pt_y), QPoint(side1_x, side1_y), QPoint(side2_x, side2_y) };
		QPoint trian_bg[3] = { QPoint(ptbg_x, pt_y), QPoint(side1bg_x, side1bg_y), QPoint(side2bg_x, side2bg_y) };

		//QPen const & pen, QPoint const * points, int npoints, bool filled

		viewport->draw_polygon(this->realtime_track_bg_pen, trian_bg, 3, true);
		viewport->draw_polygon(this->realtime_track_pen, trian, 3, true);
		viewport->fill_rectangle((this->realtime_fix.fix.mode > MODE_2D) ? this->realtime_track_pt2_pen.color() : this->realtime_track_pt1_pen.color(), screen_pos.x - 2, screen_pos.y - 2, 4, 4);

		//this->realtime_track_pt_pen = (this->realtime_track_pt_pen == this->realtime_track_pt1_pen) ? this->realtime_track_pt2_pen : this->realtime_track_pt1_pen;
	}
}




Trackpoint * LayerGPS::create_realtime_trackpoint(bool forced)
{
	/* Note that fix.time is a double, but it should not affect
	   the precision for most GPS. */
	time_t cur_timestamp = this->realtime_fix.fix.time;
	time_t last_timestamp = this->last_fix.fix.time;

	if (cur_timestamp < last_timestamp) {
		return NULL;
	}

	if (this->realtime_record && this->realtime_fix.dirty) {
		bool replace = false;
		int heading = std::isnan(this->realtime_fix.fix.track) ? 0 : (int)floor(this->realtime_fix.fix.track);
		int last_heading = std::isnan(this->last_fix.fix.track) ? 0 : (int)floor(this->last_fix.fix.track);
		int alt = std::isnan(this->realtime_fix.fix.altitude) ? VIK_DEFAULT_ALTITUDE : floor(this->realtime_fix.fix.altitude);
		int last_alt = std::isnan(this->last_fix.fix.altitude) ? VIK_DEFAULT_ALTITUDE : floor(this->last_fix.fix.altitude);

		if (!this->realtime_track->empty()
		    && this->realtime_fix.fix.mode > MODE_2D
		    && this->last_fix.fix.mode <= MODE_2D
		    && (cur_timestamp - last_timestamp) < 2) {

			auto last_tp = std::prev(this->realtime_track->end());
			delete *last_tp;
			this->realtime_track->trackpoints.erase(last_tp);
			replace = true;
		}

		if (replace || ((cur_timestamp != last_timestamp)
				&& ((forced
				     || ((heading < last_heading) && (heading < (last_heading - 3)))
				     || ((heading > last_heading) && (heading > (last_heading + 3)))
				     || ((alt != VIK_DEFAULT_ALTITUDE) && (alt != last_alt)))))) {

			/* TODO: check for new segments. */
			Trackpoint * tp_ = new Trackpoint();
			tp_->newsegment = false;
			tp_->has_timestamp = true;
			tp_->timestamp = this->realtime_fix.fix.time;
			tp_->altitude = alt;
			/* Speed only available for 3D fix. Check for NAN when use this speed. */
			tp_->speed = this->realtime_fix.fix.speed;
			tp_->course = this->realtime_fix.fix.track;
			tp_->nsats = this->realtime_fix.satellites_used;
			tp_->fix_mode = (GPSFixMode) this->realtime_fix.fix.mode;

			tp_->coord = Coord(LatLon(this->realtime_fix.fix.latitude, this->realtime_fix.fix.longitude),
					   this->trw_children[GPS_CHILD_LAYER_TRW_REALTIME]->get_coord_mode());

			this->realtime_track->add_trackpoint(tp_, true); /* Ensure bounds is recalculated. */
			this->realtime_fix.dirty = false;
			this->realtime_fix.satellites_used = 0;
			this->last_fix = this->realtime_fix;
			return tp_;
		}
	}
	return NULL;
}




#define VIK_SETTINGS_GPS_STATUSBAR_FORMAT "gps_statusbar_format"

void LayerGPS::update_statusbar(Window * window_)
{
	QString statusbar_format_code;
	if (!ApplicationState::get_string(VIK_SETTINGS_GPS_STATUSBAR_FORMAT, statusbar_format_code)) {
		/* Otherwise use default. */
		statusbar_format_code = "GSA";
	}

	const QString msg = vu_trackpoint_formatted_message(statusbar_format_code.toUtf8().constData(), this->tp, this->tp_prev, this->realtime_track, this->last_fix.fix.climb);
	window_->get_statusbar()->set_message(StatusBarField::INFO, msg);
}




static void gpsd_raw_hook(VglGpsd *vgpsd, char *data)
{
	bool update_all = false;
	LayerGPS * layer = vgpsd->gps_layer;

	if (!layer->realtime_tracking_in_progress) {
		qDebug() << "WW:" PREFIX << "receiving GPS data while not in realtime mode";
		return;
	}

	if ((vgpsd->gpsd.fix.mode >= MODE_2D) &&
	    !std::isnan(vgpsd->gpsd.fix.latitude) &&
	    !std::isnan(vgpsd->gpsd.fix.longitude)) {

		Window * window_ = layer->get_window();
		Viewport * viewport = layer->get_window()->get_viewport();
		layer->realtime_fix.fix = vgpsd->gpsd.fix;
		layer->realtime_fix.satellites_used = vgpsd->gpsd.satellites_used;
		layer->realtime_fix.dirty = true;

		const Coord vehicle_coord(LatLon(layer->realtime_fix.fix.latitude, layer->realtime_fix.fix.longitude),
					  layer->trw_children[GPS_CHILD_LAYER_TRW_REALTIME]->get_coord_mode());

		if ((layer->vehicle_position == VEHICLE_POSITION_CENTERED) ||
		    (layer->realtime_jump_to_start && layer->first_realtime_trackpoint)) {
			viewport->set_center_from_coord(vehicle_coord, false);
			update_all = true;
		} else if (layer->vehicle_position == VEHICLE_POSITION_ON_SCREEN) {
			const int hdiv = 6;
			const int vdiv = 6;
			const int px = 20; /* Adjustment in pixels to make sure vehicle is inside the box. */
			int width = viewport->get_width();
			int height = viewport->get_height();
			int vx, vy;

			viewport->coord_to_screen_pos(vehicle_coord, &vx, &vy);
			update_all = true;
			if (vx < (width/hdiv)) {
				viewport->set_center_from_screen_pos(vx - width/2 + width/hdiv + px, vy);
			} else if (vx > (width - width/hdiv)) {
				viewport->set_center_from_screen_pos(vx + width/2 - width/hdiv - px, vy);
			} else if (vy < (height/vdiv)) {
				viewport->set_center_from_screen_pos(vx, vy - height/2 + height/vdiv + px);
			} else if (vy > (height - height/vdiv)) {
				viewport->set_center_from_screen_pos(vx, vy + height/2 - height/vdiv - px);
			} else {
				update_all = false;
			}
		}

		layer->first_realtime_trackpoint = false;

		layer->tp = layer->create_realtime_trackpoint(false);

		if (layer->tp) {
			if (layer->realtime_update_statusbar) {
				layer->update_statusbar(window_);
			}
			layer->tp_prev = layer->tp;
		}

		/* NB update from background thread. */
		if (update_all) {
			layer->emit_layer_changed();
		} else {
			layer->trw_children[GPS_CHILD_LAYER_TRW_REALTIME]->emit_layer_changed();
		}
	}
}




static int gpsd_data_available(GIOChannel *source, GIOCondition condition, void * gps_layer)
{
	LayerGPS * layer = (LayerGPS *) gps_layer;

	if (condition == G_IO_IN) {
#if GPSD_API_MAJOR_VERSION == 3 || GPSD_API_MAJOR_VERSION == 4
		if (!gps_poll(&layer->vgpsd->gpsd)) {
#elif GPSD_API_MAJOR_VERSION == 5 || GPSD_API_MAJOR_VERSION == 6
		if (gps_read(&layer->vgpsd->gpsd) > -1) {
			/* Reuse old function to perform operations on the new GPS data. */
			gpsd_raw_hook(layer->vgpsd, NULL);
#else
			/* Broken compile. */
#endif
			return true;
		} else {
			qDebug() << "WW: Disconnected from gpsd. Trying to reconnect";
			layer->rt_gpsd_disconnect();
			layer->rt_gpsd_connect(false);
		}
	}

	return false; /* No further calling. */
}




/* TODO: maybe we could have a common code in LayerTRW for this. */
static QString make_track_name(LayerTRW * trw)
{
	const QString base_name = "REALTIME";

	QString name = base_name;
	int i = 2;

	while (trw->get_tracks_node().find_track_by_name(name) != NULL) {
		name = QString("%1#%2").arg(base_name).arg(i);
		i++;
	}

	return name;
}




static bool rt_gpsd_try_connect(void * gps_layer)
{
	LayerGPS * layer = (LayerGPS *) gps_layer;

#if GPSD_API_MAJOR_VERSION == 3
	struct gps_data_t *gpsd = gps_open(layer->gpsd_host.toUtf8().constData(), layer->gpsd_port.toUtf8().constData());

	if (gpsd == NULL) {
#elif GPSD_API_MAJOR_VERSION == 4
	layer->vgpsd = malloc(sizeof(VglGpsd));

	if (gps_open_r(layer->gpsd_host.toUtf8().constData(), layer->gpsd_port.toUtf8().constData(), /*(struct gps_data_t *)*/layer->vgpsd) != 0) {
#elif GPSD_API_MAJOR_VERSION == 5 || GPSD_API_MAJOR_VERSION == 6
	layer->vgpsd = (VglGpsd *) malloc(sizeof(VglGpsd));
	if (gps_open(layer->gpsd_host.toUtf8().constData(), layer->gpsd_port.toUtf8().constData(), &layer->vgpsd->gpsd) != 0) {
#else
		/* Delibrately break compilation... */
#endif
		qDebug() << QString("WW: Layer GPS: Failed to connect to gpsd at %1 (port %2). Will retry in %3 seconds")
			.arg(layer->gpsd_host).arg(layer->gpsd_port).arg(layer->gpsd_retry_interval);
		return true;   /* Keep timer running. */
	}

#if GPSD_API_MAJOR_VERSION == 3
	layer->vgpsd = realloc(gpsd, sizeof(VglGpsd));
#endif
	layer->vgpsd->gps_layer = layer;

	layer->realtime_fix.dirty = layer->last_fix.dirty = false;
	/* Track alt/time graph uses VIK_DEFAULT_ALTITUDE (0.0) as invalid. */
	layer->realtime_fix.fix.altitude = layer->last_fix.fix.altitude = VIK_DEFAULT_ALTITUDE;
	layer->realtime_fix.fix.speed = layer->last_fix.fix.speed = NAN;

	if (layer->realtime_record) {
		LayerTRW * trw = layer->trw_children[GPS_CHILD_LAYER_TRW_REALTIME];
		layer->realtime_track = new Track(false);
		layer->realtime_track->visible = true;
		layer->realtime_track->set_name(make_track_name(trw));
		trw->add_track(layer->realtime_track);
	}

#if GPSD_API_MAJOR_VERSION == 3 || GPSD_API_MAJOR_VERSION == 4
	gps_set_raw_hook(&layer->vgpsd->gpsd, gpsd_raw_hook);
#endif

	layer->realtime_io_channel = g_io_channel_unix_new(layer->vgpsd->gpsd.gps_fd);
	layer->realtime_io_watch_id = g_io_add_watch(layer->realtime_io_channel,
						    (GIOCondition) (G_IO_IN | G_IO_ERR | G_IO_HUP), gpsd_data_available, layer);

#if GPSD_API_MAJOR_VERSION == 3
	gps_query(&layer->vgpsd->gpsd, "w+x");
#endif
#if GPSD_API_MAJOR_VERSION == 4 || GPSD_API_MAJOR_VERSION == 5 || GPSD_API_MAJOR_VERSION == 6
	gps_stream(&layer->vgpsd->gpsd, WATCH_ENABLE, NULL);
#endif

	return false;  /* No longer called by timeout. */
}




bool LayerGPS::rt_ask_retry()
{
	const QString message = QString(tr("Failed to connect to gpsd at %1 (port %2)\n"
					   "Should Viking keep trying (every %3 seconds)?"))
		.arg(this->gpsd_host)
		.arg(this->gpsd_port)
		.arg(this->gpsd_retry_interval);

	const int reply = QMessageBox::question(this->get_window(), "title", message);
	return (reply == QMessageBox::Yes);
}




bool LayerGPS::rt_gpsd_connect(bool ask_if_failed)
{
	this->realtime_retry_timer = 0;
	if (rt_gpsd_try_connect((void * ) this)) {
		if (this->gpsd_retry_interval <= 0) {
			qDebug() << "WW: Failed to connect to gpsd but will not retry because retry intervel was set to" << this->gpsd_retry_interval << "(which is 0 or negative).";
			return false;
		} else if (ask_if_failed && !this->rt_ask_retry()) {
			return false;
		} else {
			this->realtime_retry_timer = g_timeout_add_seconds(this->gpsd_retry_interval,
									   (GSourceFunc)rt_gpsd_try_connect, (void *) this);
		}
	}
	return true;
}




void LayerGPS::rt_gpsd_disconnect()
{
	if (this->realtime_retry_timer) {
		g_source_remove(this->realtime_retry_timer);
		this->realtime_retry_timer = 0;
	}
	if (this->realtime_io_watch_id) {
		g_source_remove(this->realtime_io_watch_id);
		this->realtime_io_watch_id = 0;
	}
	if (this->realtime_io_channel) {
		GError *error = NULL;
		g_io_channel_shutdown(this->realtime_io_channel, false, &error);
		this->realtime_io_channel = NULL;
	}

	if (this->vgpsd) {
#if GPSD_API_MAJOR_VERSION == 4 || GPSD_API_MAJOR_VERSION == 5 || GPSD_API_MAJOR_VERSION == 6
		gps_stream(&this->vgpsd->gpsd, WATCH_DISABLE, NULL);
#endif
		gps_close(&this->vgpsd->gpsd);

#if GPSD_API_MAJOR_VERSION == 3
		free(this->vgpsd);
#elif GPSD_API_MAJOR_VERSION == 4 || GPSD_API_MAJOR_VERSION == 5 || GPSD_API_MAJOR_VERSION == 6
		free(this->vgpsd);
#endif

		this->vgpsd = NULL;
	}

	if (this->realtime_record && this->realtime_track) {
		if (!this->realtime_track->empty()) {
			this->trw_children[GPS_CHILD_LAYER_TRW_REALTIME]->delete_track(this->realtime_track);
		}
		this->realtime_track = NULL;
	}
}




void LayerGPS::gps_start_stop_tracking_cb(void)
{
	this->realtime_tracking_in_progress = (this->realtime_tracking_in_progress == false);

	/* Make sure we are still in the boat with libgps. */
	assert ((((int) GPSFixMode::Fix2D) == MODE_2D) && (((int) GPSFixMode::Fix3D) == MODE_3D));

	if (this->realtime_tracking_in_progress) {
		this->first_realtime_trackpoint = true;
		if (!this->rt_gpsd_connect(true)) {
			this->first_realtime_trackpoint = false;
			this->realtime_tracking_in_progress = false;
			this->tp = NULL;
		}
	} else {  /* Stop realtime tracking. */
		this->first_realtime_trackpoint = false;
		this->tp = NULL;
		this->rt_gpsd_disconnect();
	}
}
#endif /* REALTIME_GPS_TRACKING_ENABLED */




LayerGPS::LayerGPS()
{
	this->type = LayerType::GPS;
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
	this->gpsd_port = QObject::tr("port");

#endif /* REALTIME_GPS_TRACKING_ENABLED */

	this->set_initial_parameter_values();
	this->set_name(Layer::get_type_ui_label(this->type));

	for (int i = 0; i < GPS_CHILD_LAYER_MAX; i++) {
		this->trw_children[i] = new LayerTRW();

		/* FIXME: this doesn't work. */
		uint16_t menu_items = (uint16_t) LayerMenuItem::ALL;
		menu_items &= ~((uint16_t) LayerMenuItem::CUT | (uint16_t) LayerMenuItem::DELETE);
		this->trw_children[i]->set_menu_selection((LayerMenuItem) menu_items);
	}
}




/* To be called right after constructor. */
void LayerGPS::set_coord_mode(CoordMode new_mode)
{
	for (int i = 0; i < GPS_CHILD_LAYER_MAX; i++) {
		this->trw_children[i]->set_coord_mode(new_mode);
	}
}
