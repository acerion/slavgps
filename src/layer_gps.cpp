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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <mutex>
#include <vector>
#include <list>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <cstring>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif


#include <glib/gstdio.h>
#include <glib/gprintf.h>

#ifdef VIK_CONFIG_REALTIME_GPS_TRACKING
#include <gps.h>
#include "vikutils.h"
#endif

#include "viewport_internal.h"
#include "layers_panel.h"
#include "window.h"
#include "layer_gps.h"
#include "layer_trw.h"
#include "track_internal.h"
#include "settings.h"
#include "globals.h"
typedef int GdkPixdata; /* TODO: remove sooner or later. */
#include "icons/icons.h"
#include "babel.h"
#include "dialog.h"
#include "util.h"




using namespace SlavGPS;



#ifdef K
extern std::vector<BabelDevice *> a_babel_device_list;
#endif



typedef struct {
	LayerGPS * layer;
	LayersPanel * panel;
} gps_layer_data_t;

static void gps_upload_cb(gps_layer_data_t * data);
static void gps_download_cb(gps_layer_data_t * data);
static void gps_empty_upload_cb(gps_layer_data_t * data);
static void gps_empty_download_cb(gps_layer_data_t * data);
static void gps_empty_all_cb(gps_layer_data_t * data);
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
static void gps_empty_realtime_cb(gps_layer_data_t * data);
static void gps_start_stop_tracking_cb(gps_layer_data_t * data);

#endif

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

/* NUM_PORTS not actually used. */
/* #define NUM_PORTS (sizeof(params_ports)/sizeof(params_ports[0]) - 1) */
/* Compatibility with previous versions. */
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




typedef struct {
	std::mutex mutex;
	GPSDirection direction;
	QString port; /* FIXME: this struct is malloced, so this field will be invalid in freshly allocated struct. */
	bool ok;
	int total_count;
	int count;
	LayerTRW * trw = NULL;
	Track * trk = NULL;
	QString babel_args;
	char * window_title = NULL;
	GtkWidget * dialog = NULL;
	QLabel * status_label = NULL;
	QLabel * gps_label = NULL;
	QLabel * ver_label = NULL;
	QLabel * id_label = NULL;
	QLabel * wp_label = NULL;
	QLabel * trk_label = NULL;
	QLabel * rte_label = NULL;
	QLabel * progress_label = NULL;
	GPSTransferType progress_type;
	Viewport * viewport = NULL;
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	bool realtime_tracking;
#endif
} GpsSession;
static void gps_session_delete(GpsSession *sess);

static const char * g_params_groups[] = {
	"Data Mode",
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	"Realtime Tracking Mode",
#endif
};

enum { GROUP_DATA_MODE, GROUP_REALTIME_MODE };




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




#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
static char *params_vehicle_position[] = {
	(char *) N_("Keep vehicle at center"),
	(char *) N_("Keep vehicle on screen"),
	(char *) N_("Disable"),
	NULL
};
enum {
	VEHICLE_POSITION_CENTERED = 0,
	VEHICLE_POSITION_ON_SCREEN,
	VEHICLE_POSITION_NONE,
};




static SGVariant moving_map_method_default(void)   { return SGVariant((int32_t) VEHICLE_POSITION_ON_SCREEN); }
static SGVariant gpsd_host_default(void)           { return SGVariant("localhost"); }
static SGVariant gpsd_port_default(void)           { return SGVariant(DEFAULT_GPSD_PORT); }
static SGVariant gpsd_retry_interval_default(void) { return SGVariant(strdup("10")); }

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
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	PARAM_REALTIME_REC,
	PARAM_REALTIME_CENTER_START,
	PARAM_VEHICLE_POSITION,
	PARAM_REALTIME_UPDATE_STATUSBAR,
	PARAM_GPSD_HOST,
	PARAM_GPSD_PORT,
	PARAM_GPSD_RETRY_INTERVAL,
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */
	NUM_PARAMS
};




static Parameter gps_layer_params[] = {
	/* NB gps_layer_inst_init() is performed after parameter registeration
	   thus to give the protocols some potential values use the old static list. */
	/* TODO: find another way to use gps_layer_inst_init()? */
	{ PARAM_PROTOCOL,                   "gps_protocol",              SGVariantType::STRING,  GROUP_DATA_MODE,     N_("GPS Protocol:"),                     WidgetType::COMBOBOX,      &protocols_args,         gps_protocol_default,        NULL, NULL }, // List reassigned at runtime
	{ PARAM_PORT,                       "gps_port",                  SGVariantType::STRING,  GROUP_DATA_MODE,     N_("Serial Port:"),                      WidgetType::COMBOBOX,      &params_ports,           gps_port_default,            NULL, NULL },
	{ PARAM_DOWNLOAD_TRACKS,            "gps_download_tracks",       SGVariantType::BOOLEAN, GROUP_DATA_MODE,     N_("Download Tracks:"),                  WidgetType::CHECKBUTTON,   NULL,                    sg_variant_true,             NULL, NULL },
	{ PARAM_UPLOAD_TRACKS,              "gps_upload_tracks",         SGVariantType::BOOLEAN, GROUP_DATA_MODE,     N_("Upload Tracks:"),                    WidgetType::CHECKBUTTON,   NULL,                    sg_variant_true,             NULL, NULL },
	{ PARAM_DOWNLOAD_ROUTES,            "gps_download_routes",       SGVariantType::BOOLEAN, GROUP_DATA_MODE,     N_("Download Routes:"),                  WidgetType::CHECKBUTTON,   NULL,                    sg_variant_true,             NULL, NULL },
	{ PARAM_UPLOAD_ROUTES,              "gps_upload_routes",         SGVariantType::BOOLEAN, GROUP_DATA_MODE,     N_("Upload Routes:"),                    WidgetType::CHECKBUTTON,   NULL,                    sg_variant_true,             NULL, NULL },
	{ PARAM_DOWNLOAD_WAYPOINTS,         "gps_download_waypoints",    SGVariantType::BOOLEAN, GROUP_DATA_MODE,     N_("Download Waypoints:"),               WidgetType::CHECKBUTTON,   NULL,                    sg_variant_true,             NULL, NULL },
	{ PARAM_UPLOAD_WAYPOINTS,           "gps_upload_waypoints",      SGVariantType::BOOLEAN, GROUP_DATA_MODE,     N_("Upload Waypoints:"),                 WidgetType::CHECKBUTTON,   NULL,                    sg_variant_true,             NULL, NULL },
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	{ PARAM_REALTIME_REC,               "record_tracking",           SGVariantType::BOOLEAN, GROUP_REALTIME_MODE, N_("Recording tracks"),                  WidgetType::CHECKBUTTON,   NULL,                    sg_variant_true,             NULL, NULL },
	{ PARAM_REALTIME_CENTER_START,      "center_start_tracking",     SGVariantType::BOOLEAN, GROUP_REALTIME_MODE, N_("Jump to current position on start"), WidgetType::CHECKBUTTON,   NULL,                    sg_variant_false,            NULL, NULL },
	{ PARAM_VEHICLE_POSITION,           "moving_map_method",         SGVariantType::INT,     GROUP_REALTIME_MODE, N_("Moving Map Method:"),                WidgetType::RADIOGROUP,    params_vehicle_position, moving_map_method_default,   NULL, NULL },
	{ PARAM_REALTIME_UPDATE_STATUSBAR,  "realtime_update_statusbar", SGVariantType::BOOLEAN, GROUP_REALTIME_MODE, N_("Update Statusbar:"),                 WidgetType::CHECKBUTTON,   NULL,                    sg_variant_true,             NULL, N_("Display information in the statusbar on GPS updates") },
	{ PARAM_GPSD_HOST,                  "gpsd_host",                 SGVariantType::STRING,  GROUP_REALTIME_MODE, N_("Gpsd Host:"),                        WidgetType::ENTRY,         NULL,                    gpsd_host_default,           NULL, NULL },
	{ PARAM_GPSD_PORT,                  "gpsd_port",                 SGVariantType::STRING,  GROUP_REALTIME_MODE, N_("Gpsd Port:"),                        WidgetType::ENTRY,         NULL,                    gpsd_port_default,           NULL, NULL },
	{ PARAM_GPSD_RETRY_INTERVAL,        "gpsd_retry_interval",       SGVariantType::STRING,  GROUP_REALTIME_MODE, N_("Gpsd Retry Interval (seconds):"),    WidgetType::ENTRY,         NULL,                    gpsd_retry_interval_default, NULL, NULL },
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */

	{ NUM_PARAMS,                       NULL,                        SGVariantType::EMPTY,   PARAMETER_GROUP_GENERIC, NULL,                                WidgetType::NONE,          NULL,                    NULL,                        NULL, NULL }, /* Guard. */
};




LayerGPSInterface vik_gps_layer_interface;




LayerGPSInterface::LayerGPSInterface()
{
	this->parameters_c = gps_layer_params;       /* Parameters. */
	this->parameter_groups = g_params_groups; /* Parameter groups. */

	this->fixed_layer_type_string = "GPS"; /* Non-translatable. */

	// this->action_accelerator = ...; /* Empty accelerator. */
	// this->action_icon = ...; /* Set elsewhere. */

	this->menu_items_selection = LayerMenuItem::ALL;

	this->ui_labels.new_layer = QObject::tr("New GPS Layer");
	this->ui_labels.layer_type = QObject::tr("GPS");
	this->ui_labels.layer_defaults = QObject::tr("Default Settings of GPS Layer");
}




static char * trw_names[] = {
	(char *) N_("GPS Download"),
	(char *) N_("GPS Upload"),
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	(char *) N_("GPS Realtime Tracking"),
#endif
};




/**
 * Overwrite the static setup with dynamically generated GPS Babel device list.
 */
void SlavGPS::layer_gps_init(void)
{
	int new_proto = 0;
#ifdef K
	/* +1 for luck (i.e the NULL terminator) */
	char **new_protocols = (char **) g_malloc_n(1 + a_babel_device_list.size(), sizeof(void *));

	for (auto iter = a_babel_device_list.begin(); iter != a_babel_device_list.end(); iter++) {
		/* Should be using label property but use name for now
		   thus don't need to mess around converting label to name later on. */
		new_protocols[new_proto++] = (*iter)->name;
		fprintf(stderr, "%s:%d: new_protocols: '%s'\n", __FUNCTION__, __LINE__, (*iter)->name);
	}
	new_protocols[new_proto] = NULL;

	vik_gps_layer_interface.params[PARAM_PROTOCOL].widget_data = new_protocols;
#endif
}




QString LayerGPS::tooltip()
{
	return this->protocol;
}



#define alm_append(m_obj, m_sz) {					\
		int m_len = (m_sz);					\
		g_byte_array_append(b, (uint8_t *)&m_len, sizeof (m_len)); \
		g_byte_array_append(b, (uint8_t *)(m_obj), m_len);	\
	}

/* "Copy". */
void LayerGPS::marshall(uint8_t **data, int *datalen)
{
	uint8_t *ld;
	int ll;
	GByteArray* b = g_byte_array_new();

	this->marshall_params(&ld, &ll);
	alm_append(ld, ll);
	free(ld);

	for (int i = 0; i < NUM_TRW; i++) {
		Layer::marshall(this->trw_children[i], &ld, &ll);
		if (ld) {
			alm_append(ld, ll);
			free(ld);
		}
	}
	*data = b->data;
	*datalen = b->len;
	g_byte_array_free(b, false);
}
#undef alm_append



/* "Paste". */
Layer * LayerGPSInterface::unmarshall(uint8_t * data, int len, Viewport * viewport)
{
#define alm_size (*(int *)data)
#define alm_next		 \
	len -= sizeof(int) + alm_size;		\
	data += sizeof(int) + alm_size;

	LayerGPS * layer = new LayerGPS();
	layer->set_coord_mode(viewport->get_coord_mode());

	layer->unmarshall_params(data + sizeof (int), alm_size);
	alm_next;

	int i = 0;
	while (len>0 && i < NUM_TRW) {
		Layer * child_layer = Layer::unmarshall(data + sizeof (int), alm_size, viewport);
		if (child_layer) {
			layer->trw_children[i++] = (LayerTRW *) child_layer;
			/* NB no need to attach signal update handler here
			   as this will always be performed later on in vik_gps_layer_connect_to_tree(). */
		}
		alm_next;
	}
	// qDebug() << "II: Layer GPS: LayerGPSInterface::unmarshall() ended with" << len;
	assert (len == 0);
	return layer;
#undef alm_size
#undef alm_next
}




bool LayerGPS::set_param_value(uint16_t id, const SGVariant & data, bool is_file_operation)
{
	switch (id) {
	case PARAM_PROTOCOL:
		if (!data.s.isEmpty()) {
			/* Backwards Compatibility: previous versions <v1.4 stored protocol as an array index. */
#ifdef K
			int index_ = data.s[0] - '0';
			if (data.s[0] != '\0' &&
			    g_ascii_isdigit (data.s[0]) &&
			    data.s[1] == '\0' &&
			    index_ < OLD_NUM_PROTOCOLS) {

				/* It is a single digit: activate compatibility. */
				this->protocol = protocols_args[index_].label; /* FIXME: memory leak. */
			} else {
				this->protocol = data.s;
			}
#endif
			qDebug() << "DD: Layer GPS: Protocol:" << this->protocol;
		} else {
			qDebug() << "WW: Layer GPS: Protocol: unknown GPS Protocol";
		}
		break;
	case PARAM_PORT:
		if (!data.s.isEmpty()) {
			/* Backwards Compatibility: previous versions <v0.9.91 stored serial_port as an array index. */
#ifdef K
			int index_ = data.s[0] - '0';
			if (data.s[0] != '\0' &&
			    g_ascii_isdigit(data.s[0]) &&
			    data.s[1] == '\0' &&
			    index_ < old_params_ports.size()) {

				/* It is a single digit: activate compatibility. */
				this->serial_port = old_params_ports[index_].label;

			} else {
				this->serial_port = data.s;
			}
#endif
			qDebug() << "DD: Layer GPS: Serial Port:" << this->serial_port;
		} else {
			qDebug() << "WW: Layer GPS: Serial Port: unknown serial port device";
		}
		break;
	case PARAM_DOWNLOAD_TRACKS:
		this->download_tracks = data.b;
		break;
	case PARAM_UPLOAD_TRACKS:
		this->upload_tracks = data.b;
		break;
	case PARAM_DOWNLOAD_ROUTES:
		this->download_routes = data.b;
		break;
	case PARAM_UPLOAD_ROUTES:
		this->upload_routes = data.b;
		break;
	case PARAM_DOWNLOAD_WAYPOINTS:
		this->download_waypoints = data.b;
		break;
	case PARAM_UPLOAD_WAYPOINTS:
		this->upload_waypoints = data.b;
		break;
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	case PARAM_GPSD_HOST:
		if (!data.s.isEmpty()) {
			this->gpsd_host = data.s;
		}
		break;
	case PARAM_GPSD_PORT:
		if (!data.s.isEmpty()) {
			this->gpsd_port = data.s;
		}
		break;
	case PARAM_GPSD_RETRY_INTERVAL:
		this->gpsd_retry_interval = strtol(data.s.toUtf8().constData(), NULL, 10);
		break;
	case PARAM_REALTIME_REC:
		this->realtime_record = data.b;
		break;
	case PARAM_REALTIME_CENTER_START:
		this->realtime_jump_to_start = data.b;
		break;
	case PARAM_VEHICLE_POSITION:
		this->vehicle_position = data.i;
		break;
	case PARAM_REALTIME_UPDATE_STATUSBAR:
		this->realtime_update_statusbar = data.b;
		break;
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */
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
		qDebug() << "DD: Layer GPS: Protocol:" << rv.s;
		break;
	case PARAM_PORT:
		rv = SGVariant(this->serial_port);
		qDebug() << "DD: Layer GPS: Serial Port:" << rv.s;
		break;
	case PARAM_DOWNLOAD_TRACKS:
		rv.b = this->download_tracks;
		break;
	case PARAM_UPLOAD_TRACKS:
		rv.b = this->upload_tracks;
		break;
	case PARAM_DOWNLOAD_ROUTES:
		rv.b = this->download_routes;
		break;
	case PARAM_UPLOAD_ROUTES:
		rv.b = this->upload_routes;
		break;
	case PARAM_DOWNLOAD_WAYPOINTS:
		rv.b = this->download_waypoints;
		break;
	case PARAM_UPLOAD_WAYPOINTS:
		rv.b = this->upload_waypoints;
		break;
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
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
		rv.b = this->realtime_record;
		break;
	case PARAM_REALTIME_CENTER_START:
		rv.b = this->realtime_jump_to_start;
		break;
	case PARAM_VEHICLE_POSITION:
		rv.i = this->vehicle_position;
		break;
	case PARAM_REALTIME_UPDATE_STATUSBAR:
		rv.u = this->realtime_update_statusbar;
		break;
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */
	default:
		fprintf(stderr, _("WARNING: %s: unknown parameter\n"), __FUNCTION__);
	}

	return rv;
}




void LayerGPS::draw(Viewport * viewport)
{
	Layer * trigger = viewport->get_trigger();

	for (int i = 0; i < NUM_TRW; i++) {
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
			trw->draw_visible(viewport);
		}
	}
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	if (this->realtime_tracking) {
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
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */
}




void LayerGPS::change_coord_mode(CoordMode mode)
{
	for (int i = 0; i < NUM_TRW; i++) {
		this->trw_children[i]->change_coord_mode(mode);
	}
}




void LayerGPS::add_menu_items(QMenu & menu)
{
#ifdef K
	static gps_layer_data_t pass_along;
	pass_along.layer = this;
	pass_along.panel = (LayersPanel *) panel;

	QAction * action = NULL;

	action = new QAction(QObject::tr("&Upload to GPS"), this);
	action->setIcon(QIcon::fromTheme("GTK_STOCK_GO_UP"));
	QObject::connect(action, SIGNAL (triggered(bool)), &pass_along, SLOT (gps_upload_cb));
	menu->addAction(action);

	action = new QAction(QObject::tr("Download from &GPS"), this);
	action->setIcon(QIcon::fromTheme("GTK_STOCK_GO_DOWN"));
	QObject::connect(action, SIGNAL (triggered(bool)), &pass_along, SLOT (gps_download_cb));
	menu->addAction(action);

#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	action = new QAction(this->realtime_tracking ? QObject::tr("_Stop Realtime Tracking") : QObject::tr("_Start Realtime Tracking"), this);
	action->setIcon(this->realtime_tracking ? QIcon::fromTheme("GTK_STOCK_MEDIA_STOP") : QIcon::fromTheme("GTK_STOCK_MEDIA_PLAY"));
	QObject::connect(action, SIGNAL (triggered(bool)), &pass_along, SLOT (gps_start_stop_tracking_cb));
	menu->addAction(action);

	action = new QAction(QObject::tr("Empty &Realtime"), this);
	action->setIcon(QIcon::fromTheme("GTK_STOCK_REMOVE"));
	QObject::connect(action, SIGNAL (triggered(bool)), &pass_along, SLOT (gps_empty_realtime_cb));
	menu->addAction(action);
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */

	action = new QAction(QObject::tr("E&mpty Upload"), this);
	action->setIcon(QIcon::fromTheme("GTK_STOCK_REMOVE"));
	QObject::connect(action, SIGNAL (triggered(bool)), &pass_along, SLOT (gps_empty_upload_cb));
	menu->addAction(action);

	action = new QAction(QObject::tr("&Empty Download"), this);
	action->setIcon(QIcon::fromTheme("GTK_STOCK_REMOVE"));
	QObject::connect(action, SIGNAL (triggered(bool)), &pass_along, SLOT (gps_empty_download_cb));
	menu->addAction(action);

	action = new QAction(QObject::tr("Empty &All"), this);
	action->setIcon(QIcon::fromTheme("GTK_STOCK_REMOVE"));
	QObject::connect(action, SIGNAL (triggered(bool)), &pass_along, SLOT (gps_empty_all_cb));
	menu->addAction(action);
#endif
}




LayerGPS::~LayerGPS()
{
	for (int i = 0; i < NUM_TRW; i++) {
		if (this->connected_to_tree) {
			//this->disconnect_layer_signal(this->trw_children[i]);
		}
		this->trw_children[i]->unref();
	}
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	this->rt_gpsd_disconnect();
	if (this->realtime_track_pen != NULL) {
#ifdef K
		g_object_unref(this->realtime_track_pen);
#endif
	}

	if (this->realtime_track_bg_pen != NULL) {
#ifdef K
		g_object_unref(this->realtime_track_bg_pen);
#endif
	}

	if (this->realtime_track_pt1_pen != NULL) {
#ifdef K
		g_object_unref(this->realtime_track_pt1_pen);
#endif
	}

	if (this->realtime_track_pt2_pen != NULL) {
#ifdef K
		g_object_unref(this->realtime_track_pt2_pen);
#endif
	}
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */
}




void LayerGPS::connect_to_tree(TreeView * tree_view_, TreeIndex *layer_iter)
{
#ifdef K
	GtkTreeIter iter;

	this->tree_view = tree_view_;
	this->iter = *layer_iter;
	this->connected_to_tree = true;

	/* TODO set to garmin by default.
	   if (a_babel_device_list)
	           device = ((BabelDevice*)g_list_nth_data(a_babel_device_list, last_active))->name;
	   Need to access uibuild widgets somehow.... */

	for (int ix = 0; ix < NUM_TRW; ix++) {
		LayerTRW * trw = this->trw_children[ix];

		/* TODO: find a way to pass trw->get_timestamp() to tree view's item. */
		/* TODO: find a way to pass 'above==true' argument to function adding new tree item. */
		TreeIndex const & iter = this->tree_view->add_tree_item(layer_iter, trw, _(trw_names[ix]));

		if (!trw->visible) {
			this->tree_view->set_visibility(&iter, false);
		}
		trw->connect_to_tree(this->tree_view, &iter);
		QObject::connect(trw, SIGNAL("update"), (Layer *) this, SLOT (Layer::child_layer_changed_cb));
	}
#endif
}




std::list<Layer const * > * LayerGPS::get_children()
{
	std::list<Layer const * > * children_ = new std::list<Layer const *>;
	for (int i = NUM_TRW - 1; i >= 0; i--) {
		children_->push_front((Layer const *) this->trw_children[i]);
	}
	return children_;
}




LayerTRW * LayerGPS::get_a_child()
{
	assert ((this->cur_read_child >= 0) && (this->cur_read_child < NUM_TRW));

	LayerTRW * trw = this->trw_children[this->cur_read_child];
	if (++(this->cur_read_child) >= NUM_TRW) {
		this->cur_read_child = 0;
	}
	return(trw);
}




bool LayerGPS::is_empty()
{
	if (this->trw_children[0]) {
		return false;
	}
	return true;
}




static void gps_session_delete(GpsSession *sess)
{
	free(sess);
}




static void set_total_count(int cnt, GpsSession *sess)
{
#ifdef K
	char s[128];
	gdk_threads_enter();
	sess->mutex.lock();
	if (sess->ok) {
		const char *tmp_str;
		if (sess->direction == GPSDirection::DOWN) {
			switch (sess->progress_type) {
			case GPSTransferType::WPT:
				tmp_str = ngettext("Downloading %d waypoint...", "Downloading %d waypoints...", cnt); sess->total_count = cnt;
				break;
			case GPSTransferType::TRK:
				tmp_str = ngettext("Downloading %d trackpoint...", "Downloading %d trackpoints...", cnt); sess->total_count = cnt;
				break;
			default: {
				/* Maybe a gpsbabel bug/feature (upto at least v1.4.3 or maybe my Garmin device) but the count always seems x2 too many for routepoints. */
				int mycnt = (cnt / 2) + 1;
				tmp_str = ngettext("Downloading %d routepoint...", "Downloading %d routepoints...", mycnt);
				sess->total_count = mycnt;
				break;
			}
			}
		} else {
			switch (sess->progress_type) {
			case GPSTransferType::WPT:
				tmp_str = ngettext("Uploading %d waypoint...", "Uploading %d waypoints...", cnt);
				break;
			case GPSTransferType::TRK:
				tmp_str = ngettext("Uploading %d trackpoint...", "Uploading %d trackpoints...", cnt);
				break;
			default:
				tmp_str = ngettext("Uploading %d routepoint...", "Uploading %d routepoints...", cnt);
				break;
			}
		}

		snprintf(s, 128, tmp_str, cnt);
		sess->progress_label->setText(s);
		gtk_widget_show(sess->progress_label);
		sess->total_count = cnt;
	}
	sess->mutex.unlock();
	gdk_threads_leave();
#endif
}




static void set_current_count(int cnt, GpsSession *sess)
{
#ifdef K
	char s[128];
	const char *tmp_str;

	gdk_threads_enter();
	sess->mutex.lock();
	if (sess->ok) {
		if (cnt < sess->total_count) {
			if (sess->direction == GPSDirection::DOWN) {
				switch (sess->progress_type) {
				case GPSTransferType::WPT:
					tmp_str = ngettext("Downloaded %d out of %d waypoint...", "Downloaded %d out of %d waypoints...", sess->total_count);
					break;
				case GPSTransferType::TRK:
					tmp_str = ngettext("Downloaded %d out of %d trackpoint...", "Downloaded %d out of %d trackpoints...", sess->total_count);
					break;
				default:
					tmp_str = ngettext("Downloaded %d out of %d routepoint...", "Downloaded %d out of %d routepoints...", sess->total_count);
					break;
				}
			} else {
				switch (sess->progress_type) {
				case GPSTransferType::WPT:
					tmp_str = ngettext("Uploaded %d out of %d waypoint...", "Uploaded %d out of %d waypoints...", sess->total_count);
					break;
				case GPSTransferType::TRK:
					tmp_str = ngettext("Uploaded %d out of %d trackpoint...", "Uploaded %d out of %d trackpoints...", sess->total_count);
					break;
				default:
					tmp_str = ngettext("Uploaded %d out of %d routepoint...", "Uploaded %d out of %d routepoints...", sess->total_count);
					break;
				}
			}
			snprintf(s, 128, tmp_str, cnt, sess->total_count);
		} else {
			if (sess->direction == GPSDirection::DOWN) {
				switch (sess->progress_type) {
				case GPSTransferType::WPT:
					tmp_str = ngettext("Downloaded %d waypoint", "Downloaded %d waypoints", cnt);
					break;
				case GPSTransferType::TRK:
					tmp_str = ngettext("Downloaded %d trackpoint", "Downloaded %d trackpoints", cnt);
					break;
				default:
					tmp_str = ngettext("Downloaded %d routepoint", "Downloaded %d routepoints", cnt);
					break;
				}
			} else {
				switch (sess->progress_type) {
				case GPSTransferType::WPT:
					tmp_str = ngettext("Uploaded %d waypoint", "Uploaded %d waypoints", cnt);
					break;
				case GPSTransferType::TRK:
					tmp_str = ngettext("Uploaded %d trackpoint", "Uploaded %d trackpoints", cnt);
					break;
				default:
					tmp_str = ngettext("Uploaded %d routepoint", "Uploaded %d routepoints", cnt);
					break;
				}
			}
			snprintf(s, 128, tmp_str, cnt);
		}
		sess->progress_label->setText(s);
	}
	sess->mutex.unlock();
	gdk_threads_leave();
#endif
}




static void set_gps_info(const char *info, GpsSession *sess)
{
#ifdef K
	char s[256];
	gdk_threads_enter();
	sess->mutex.lock();
	if (sess->ok) {
		snprintf(s, 256, _("GPS Device: %s"), info);
		sess->gps_label->setText(s);
	}
	sess->mutex.unlock();
	gdk_threads_leave();
#endif
}




/*
 * Common processing for GPS Device information.
 * It doesn't matter whether we're uploading or downloading.
 */
static void process_line_for_gps_info(const char *line, GpsSession *sess)
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
			set_gps_info(info, sess);
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
			set_gps_info(tokens[1], sess);
		}
		g_strfreev(tokens);
	}
}




static void gps_download_progress_func(BabelProgressCode c, void * data, GpsSession * sess)
{
#ifdef K
	char *line;

	gdk_threads_enter();
	sess->mutex.lock();
	if (!sess->ok) {
		sess->mutex.unlock();
		gps_session_delete(sess);
		gdk_threads_leave();
		g_thread_exit(NULL);
	}
	sess->mutex.unlock();
	gdk_threads_leave();

	switch(c) {
	case BABEL_DIAG_OUTPUT:
		line = (char *)data;

		gdk_threads_enter();
		sess->mutex.lock();
		if (sess->ok) {
			sess->status_label->setText(QObject::tr("Status: Working..."));
		}
		sess->mutex.unlock();
		gdk_threads_leave();

		/* Tells us the type of items that will follow. */
		if (strstr(line, "Xfer Wpt")) {
			sess->progress_label = sess->wp_label;
			sess->progress_type = GPSTransferType::WPT;
		}
		if (strstr(line, "Xfer Trk")) {
			sess->progress_label = sess->trk_label;
			sess->progress_type = GPSTransferType::TRK;
		}
		if (strstr(line, "Xfer Rte")) {
			sess->progress_label = sess->rte_label;
			sess->progress_type = GPSTransferType::RTE;
		}

		process_line_for_gps_info(line, sess);

		if (strstr(line, "RECORD")) {
			if (strlen(line) > 20) {
				int lsb, msb;
				sscanf(line + 17, "%x", &lsb);
				sscanf(line + 20, "%x", &msb);
				int cnt = lsb + msb * 256;
				set_total_count(cnt, sess);
				sess->count = 0;
			}
		}
		if (strstr(line, "WPTDAT") || strstr(line, "TRKHDR") || strstr(line, "TRKDAT") || strstr(line, "RTEHDR") || strstr(line, "RTEWPT")) {
			sess->count++;
			set_current_count(sess->count, sess);
		}
		break;
	case BABEL_DONE:
		break;
	default:
		break;
	}
#endif
}




static void gps_upload_progress_func(BabelProgressCode c, void * data, GpsSession * sess)
{
#ifdef K
	char *line;
	static int cnt = 0;

	gdk_threads_enter();
	sess->mutex.lock();
	if (!sess->ok) {
		sess->mutex.unlock();
		gps_session_delete(sess);
		gdk_threads_leave();
		g_thread_exit(NULL);
	}
	sess->mutex.unlock();
	gdk_threads_leave();

	switch(c) {
	case BABEL_DIAG_OUTPUT:
		line = (char *)data;

		gdk_threads_enter();
		sess->mutex.lock();
		if (sess->ok) {
			sess->status_label->setText(QObject::tr("Status: Working..."));
		}
		sess->mutex.unlock();
		gdk_threads_leave();

		process_line_for_gps_info(line, sess);

		if (strstr(line, "RECORD")) {


			if (strlen(line) > 20) {
				int lsb, msb;
				sscanf(line + 17, "%x", &lsb);
				sscanf(line + 20, "%x", &msb);
				cnt = lsb + msb * 256;
				/* set_total_count(cnt, sess); */
				sess->count = 0;
			}
		}
		if (strstr(line, "WPTDAT")) {
			if (sess->count == 0) {
				sess->progress_label = sess->wp_label;
				sess->progress_type = GPSTransferType::WPT;
				set_total_count(cnt, sess);
			}
			sess->count++;
			set_current_count(sess->count, sess);
		}
		if (strstr(line, "RTEHDR") || strstr(line, "RTEWPT")) {
			if (sess->count == 0) {
				sess->progress_label = sess->rte_label;
				sess->progress_type = GPSTransferType::RTE;
				/* Maybe a gpsbabel bug/feature (upto at least v1.4.3 or maybe my Garmin device) but the count always seems x2 too many for routepoints.
				   Anyway since we're uploading - we should know how many points we're going to put! */
				cnt = (cnt / 2) + 1;
				set_total_count(cnt, sess);
			}
			sess->count++;
			set_current_count(sess->count, sess);
		}
		if (strstr(line, "TRKHDR") || strstr(line, "TRKDAT")) {
			if (sess->count == 0) {
				sess->progress_label = sess->trk_label;
				sess->progress_type = GPSTransferType::TRK;
				set_total_count(cnt, sess);
			}
			sess->count++;
			set_current_count(sess->count, sess);
		}
		break;
	case BABEL_DONE:
		break;
	default:
		break;
	}
#endif
}




static void gps_comm_thread(GpsSession *sess)
{
	bool result;

	if (sess->direction == GPSDirection::DOWN) {
		ProcessOptions po(sess->babel_args, sess->port, NULL, NULL); /* kamil FIXME: memory leak through these pointers? */
		result = a_babel_convert_from(sess->trw, &po, (BabelCallback) gps_download_progress_func, sess, NULL);
	} else {
		result = a_babel_convert_to(sess->trw, sess->trk, sess->babel_args, sess->port,
					    (BabelCallback) gps_upload_progress_func, sess);
	}
#ifdef K
	if (!result) {
		sess->status_label->setText(QObject::tr("Error: couldn't find gpsbabel."));
	} else {
		sess->mutex.lock();
		if (sess->ok) {
			sess->status_label->setText(QObject::tr("Done."));
			gtk_dialog_set_response_sensitive(GTK_DIALOG(sess->dialog), GTK_RESPONSE_ACCEPT, true);
			gtk_dialog_set_response_sensitive(GTK_DIALOG(sess->dialog), GTK_RESPONSE_REJECT, false);

			/* Do not change the view if we are following the current GPS position. */
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
			if (!sess->realtime_tracking)
#endif
			{
				if (sess->viewport && sess->direction == GPSDirection::DOWN) {
					sess->trw->post_read(sess->viewport, true);
					/* View the data available. */
					sess->trw->auto_set_view(sess->viewport) ;
					sess->trw->emit_layer_changed(); /* NB update from background thread. */
				}
			}
		} else {
			/* Cancelled. */
		}
		sess->mutex.unlock();
	}

	sess->mutex.lock();
	if (sess->ok) {
		sess->ok = false;
		sess->mutex.unlock();
	} else {
		sess->mutex.unlock();
		gps_session_delete(sess);
	}
	g_thread_exit(NULL);
#endif
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
	GpsSession *sess = (GpsSession *) malloc(sizeof(GpsSession));
	char *tracks = NULL;
	char *routes = NULL;
	char *waypoints = NULL;

	sess->direction = dir;
	sess->trw = layer;
	sess->trk = trk;
	sess->port = port;
	sess->ok = true;
	sess->window_title = (dir == GPSDirection::DOWN) ? (char *) _("GPS Download") : (char *) _("GPS Upload");
	sess->viewport = viewport;

	/* This must be done inside the main thread as the uniquify causes screen updates
	   (originally performed this nearer the point of upload in the thread). */
	if (dir == GPSDirection::UP) {
		/* Enforce unique names in the layer upload to the GPS device.
		   NB this may only be a Garmin device restriction (and may be not every Garmin device either...).
		   Thus this maintains the older code in built restriction. */
		if (!sess->trw->uniquify(panel)) {
			sess->trw->get_window()->get_statusbar()->set_message(StatusBarField::INFO,
						  _("Warning - GPS Upload items may overwrite each other"));
		}
	}

#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	sess->realtime_tracking = tracking;
#endif

	if (do_tracks) {
		tracks = (char *) "-t";
	} else {
		tracks = (char *) "";
	}

	if (do_routes) {
		routes = (char *) "-r";
	} else {
		routes = (char *) "";
	}

	if (do_waypoints) {
		waypoints = (char *) "-w";
	} else {
		waypoints = (char *) "";
	}

	sess->babel_args = QString("-D 9 %s %s %s -%c %s")
		.arg(tracks)
		.arg(routes)
		.arg(waypoints)
		.arg((dir == GPSDirection::DOWN) ? 'i' : 'o')
		.arg(protocol);
	tracks = NULL;
	waypoints = NULL;

	/* Only create dialog if we're going to do some transferring. */
	if (do_tracks || do_waypoints || do_routes) {
#ifdef K
		sess->dialog = gtk_dialog_new_with_buttons("", layer->get_window(), (GtkDialogFlags) 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);
		gtk_dialog_set_response_sensitive(GTK_DIALOG(sess->dialog),
						    GTK_RESPONSE_ACCEPT, false);
		gtk_window_set_title(GTK_WINDOW(sess->dialog), sess->window_title);

		sess->status_label = new QLabel(QObject::tr("Status: detecting gpsbabel"));
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(sess->dialog))), sess->status_label, false, false, 5);
		gtk_widget_show_all(sess->status_label);

		sess->gps_label = new QLabel(QObject::tr("GPS device: N/A"));
		sess->ver_label = new QLabel("");
		sess->id_label = new QLabel("");
		sess->wp_label = new QLabel("");
		sess->trk_label = new QLabel("");
		sess->rte_label = new QLabel("");

		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(sess->dialog))), sess->gps_label, false, false, 5);
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(sess->dialog))), sess->wp_label, false, false, 5);
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(sess->dialog))), sess->trk_label, false, false, 5);
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(sess->dialog))), sess->rte_label, false, false, 5);

		gtk_widget_show_all(sess->dialog);

		sess->progress_label = sess->wp_label;
		sess->total_count = -1;

		/* Starting gps read/write thread. */
#if GLIB_CHECK_VERSION (2, 32, 0)
		g_thread_try_new("gps_comm_thread", (GThreadFunc)gps_comm_thread, sess, NULL);
#else
		g_thread_create((GThreadFunc)gps_comm_thread, sess, false, NULL);
#endif

		gtk_dialog_set_default_response(GTK_DIALOG(sess->dialog), GTK_RESPONSE_ACCEPT);
		gtk_dialog_run(GTK_DIALOG(sess->dialog));

		gtk_widget_destroy(sess->dialog);
#endif
	} else {
		if (!turn_off) {
			Dialog::info(QObject::tr("No GPS items selected for transfer."), layer->get_window());
		}
	}

	sess->mutex.lock();

	if (sess->ok) {
		sess->ok = false;   /* Tell thread to stop. */
		sess->mutex.unlock();
	} else {
		if (turn_off) {
			/* No need for thread for powering off device (should be quick operation...) - so use babel command directly: */
			char *device_off = g_strdup_printf("-i %s,%s", protocol.toUtf8().constData(), "power_off");
			ProcessOptions po(device_off, port, NULL, NULL); /* kamil FIXME: memory leak through these pointers? */
			bool result = a_babel_convert_from(NULL, &po, NULL, NULL, NULL);
			if (!result) {
				Dialog::error(QObject::tr("Could not turn off device."), layer->get_window());
			}
			free(device_off);
		}
		sess->mutex.unlock();
		gps_session_delete(sess);
	}

	return 0;
}




static void gps_upload_cb(gps_layer_data_t * data)
{
	LayersPanel * panel = data->panel;
	LayerGPS * layer = data->layer;

	Viewport * viewport = layer->get_window()->get_viewport();
	LayerTRW * trw = layer->trw_children[TRW_UPLOAD];

	SlavGPS::vik_gps_comm(trw,
			      NULL,
			      GPSDirection::UP,
			      layer->protocol,
			      layer->serial_port,
			      false,
			      viewport,
			      panel,
			      layer->upload_tracks,
			      layer->upload_routes,
			      layer->upload_waypoints,
			      false);
}




static void gps_download_cb(gps_layer_data_t * data)
{
	LayerGPS * layer = data->layer;

	Viewport * viewport = layer->get_window()->get_viewport();
	LayerTRW * trw = layer->trw_children[TRW_DOWNLOAD];

#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	SlavGPS::vik_gps_comm(trw,
			      NULL,
			      GPSDirection::DOWN,
			      layer->protocol,
			      layer->serial_port,
			      layer->realtime_tracking,
			      viewport,
			      NULL,
			      layer->download_tracks,
			      layer->download_routes,
			      layer->download_waypoints,
			      false);
#else
	SlavGPS::vik_gps_comm(trw,
			      NULL,
			      GPSDirection::DOWN,
			      layer->protocol,
			      layer->serial_port,
			      false,
			      viewport,
			      NULL,
			      layer->download_tracks,
			      layer->download_routes,
			      layer->download_waypoints,
			      false);
#endif
}




static void gps_empty_upload_cb(gps_layer_data_t * data)
{
	LayerGPS * layer = data->layer;
	LayersPanel * panel = data->panel;

	/* Get confirmation from the user. */
	if (!Dialog::yes_or_no(QObject::tr("Are you sure you want to delete GPS Upload data?"), panel->get_window())) {
		return;
	}

	layer->trw_children[TRW_UPLOAD]->delete_all_waypoints();
	layer->trw_children[TRW_UPLOAD]->delete_all_tracks();
	layer->trw_children[TRW_UPLOAD]->delete_all_routes();
}




static void gps_empty_download_cb(gps_layer_data_t * data)
{
	LayerGPS * layer = data->layer;
	LayersPanel * panel = data->panel;

	/* Get confirmation from the user. */
	if (!Dialog::yes_or_no(QObject::tr("Are you sure you want to delete GPS Download data?"), panel->get_window())) {
		return;
	}

	layer->trw_children[TRW_DOWNLOAD]->delete_all_waypoints();
	layer->trw_children[TRW_DOWNLOAD]->delete_all_tracks();
	layer->trw_children[TRW_DOWNLOAD]->delete_all_routes();
}




#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
static void gps_empty_realtime_cb(gps_layer_data_t * data)
{
	LayerGPS * layer = data->layer;
	LayersPanel * panel = data->panel;

	/* Get confirmation from the user. */
	if (!Dialog::yes_or_no(QObject::tr("Are you sure you want to delete GPS Realtime data?"), panel->get_window())) {
		return;
	}

	layer->trw_children[TRW_REALTIME]->delete_all_waypoints();
	layer->trw_children[TRW_REALTIME]->delete_all_tracks();
}
#endif




static void gps_empty_all_cb(gps_layer_data_t * data)
{
	LayerGPS * layer = data->layer;
	LayersPanel * panel = data->panel;

	/* Get confirmation from the user. */
	if (!Dialog::yes_or_no(QObject::tr("Are you sure you want to delete All GPS data?"), panel->get_window())) {
		return;
	}

	layer->trw_children[TRW_UPLOAD]->delete_all_waypoints();
	layer->trw_children[TRW_UPLOAD]->delete_all_tracks();
	layer->trw_children[TRW_UPLOAD]->delete_all_routes();
	layer->trw_children[TRW_DOWNLOAD]->delete_all_waypoints();
	layer->trw_children[TRW_DOWNLOAD]->delete_all_tracks();
	layer->trw_children[TRW_DOWNLOAD]->delete_all_routes();
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	layer->trw_children[TRW_REALTIME]->delete_all_waypoints();
	layer->trw_children[TRW_REALTIME]->delete_all_tracks();
#endif
}




#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
void LayerGPS::realtime_tracking_draw(Viewport * viewport)
{
	Coord nw = viewport->screen_to_coord(-20, -20);
	Coord se = viewport->screen_to_coord(viewport->get_width() + 20, viewport->get_width() + 20);
	struct LatLon lnw = nw.get_latlon();
	struct LatLon lse = se.get_latlon();
	if (this->realtime_fix.fix.latitude > lse.lat &&
	     this->realtime_fix.fix.latitude < lnw.lat &&
	     this->realtime_fix.fix.longitude > lnw.lon &&
	     this->realtime_fix.fix.longitude < lse.lon &&
	     !std::isnan(this->realtime_fix.fix.track)) {

		struct LatLon ll;
		ll.lat = this->realtime_fix.fix.latitude;
		ll.lon = this->realtime_fix.fix.longitude;
		Coord gps(ll, viewport->get_coord_mode());

		int x, y;
		viewport->coord_to_screen(&gps, &x, &y);

		double heading_cos = cos(DEG2RAD(this->realtime_fix.fix.track));
		double heading_sin = sin(DEG2RAD(this->realtime_fix.fix.track));

		int half_back_y = y + 8 * heading_cos;
		int half_back_x = x - 8 * heading_sin;
		int half_back_bg_y = y + 10 * heading_cos;
		int half_back_bg_x = x -10 * heading_sin;

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
#ifdef K
		viewport->draw_polygon(this->realtime_track_bg_pen, trian_bg, 3, true);
		viewport->draw_polygon(this->realtime_track_pen, trian, 3, true);
		viewport->fill_rectangle((this->realtime_fix.fix.mode > MODE_2D) ? this->realtime_track_pt2_pen : this->realtime_track_pt1_pen, x-2, y-2, 4, 4);
#endif
		//this->realtime_track_pt_pen = (this->realtime_track_pt_pen == this->realtime_track_pt1_pen) ? this->realtime_track_pt2_pen : this->realtime_track_pt1_pen;
	}
}




Trackpoint * LayerGPS::create_realtime_trackpoint(bool forced)
{
	struct LatLon ll;

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

			ll.lat = this->realtime_fix.fix.latitude;
			ll.lon = this->realtime_fix.fix.longitude;
			tp_->coord = Coord(ll, this->trw_children[TRW_REALTIME]->get_coord_mode());

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
	char *statusbar_format_code = NULL;
	bool need2free = false;
	if (!a_settings_get_string(VIK_SETTINGS_GPS_STATUSBAR_FORMAT, &statusbar_format_code)) {
		/* Otherwise use default. */
		statusbar_format_code = strdup("GSA");
		need2free = true;
	}

	const QString msg = vu_trackpoint_formatted_message(statusbar_format_code, this->tp, this->tp_prev, this->realtime_track, this->last_fix.fix.climb);
	window_->get_statusbar()->set_message(StatusBarField::INFO, msg);

	if (need2free) {
		free(statusbar_format_code);
	}

}




static void gpsd_raw_hook(VglGpsd *vgpsd, char *data)
{
	bool update_all = false;
	LayerGPS * layer = vgpsd->gps_layer;

	if (!layer->realtime_tracking) {
		fprintf(stderr, "WARNING: %s: receiving GPS data while not in realtime mode\n", __PRETTY_FUNCTION__);
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

		struct LatLon ll;
		ll.lat = layer->realtime_fix.fix.latitude;
		ll.lon = layer->realtime_fix.fix.longitude;

		Coord vehicle_coord(ll, layer->trw_children[TRW_REALTIME]->get_coord_mode());

		if ((layer->vehicle_position == VEHICLE_POSITION_CENTERED) ||
		    (layer->realtime_jump_to_start && layer->first_realtime_trackpoint)) {
			viewport->set_center_coord(vehicle_coord, false);
			update_all = true;
		} else if (layer->vehicle_position == VEHICLE_POSITION_ON_SCREEN) {
			const int hdiv = 6;
			const int vdiv = 6;
			const int px = 20; /* Adjustment in pixels to make sure vehicle is inside the box. */
			int width = viewport->get_width();
			int height = viewport->get_height();
			int vx, vy;

			viewport->coord_to_screen(&vehicle_coord, &vx, &vy);
			update_all = true;
			if (vx < (width/hdiv)) {
				viewport->set_center_screen(vx - width/2 + width/hdiv + px, vy);
			} else if (vx > (width - width/hdiv)) {
				viewport->set_center_screen(vx + width/2 - width/hdiv - px, vy);
			} else if (vy < (height/vdiv)) {
				viewport->set_center_screen(vx, vy - height/2 + height/vdiv + px);
			} else if (vy > (height - height/vdiv)) {
				viewport->set_center_screen(vx, vy + height/2 - height/vdiv - px);
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
			layer->trw_children[TRW_REALTIME]->emit_layer_changed();
		}
	}
}




static int gpsd_data_available(GIOChannel *source, GIOCondition condition, void * gps_layer)
{
	LayerGPS * layer = (LayerGPS *) gps_layer;
#ifdef K
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
			fprintf(stderr, "WARNING: Disconnected from gpsd. Trying to reconnect\n");
			layer->rt_gpsd_disconnect();
			layer->rt_gpsd_connect(false);
		}
	}
#endif
	return false; /* No further calling. */
}




static char *make_track_name(LayerTRW * trw)
{
	const char basename[] = "REALTIME";
	const int bufsize = sizeof(basename) + 5;
	char *name = (char *) malloc(bufsize);
	strcpy(name, basename);
	int i = 2;

	while (trw->get_tracks_node().find_track_by_name(name) != NULL) {
		snprintf(name, bufsize, "%s#%d", basename, i);
		i++;
	}
	return(name);

}




static bool rt_gpsd_try_connect(void * gps_layer)
{
	LayerGPS * layer = (LayerGPS *) gps_layer;
#ifdef K
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
		LayerTRW * trw = layer->trw_children[TRW_REALTIME];
		layer->realtime_track = new Track(false);
		layer->realtime_track->visible = true;
		char * name = make_track_name(trw);
		layer->realtime_track->set_name(name);
		free(name);
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
			fprintf(stderr, "WARNING: Failed to connect to gpsd but will not retry because retry intervel was set to %d (which is 0 or negative)\n", this->gpsd_retry_interval);
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
#ifdef K
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
#endif

	if (this->realtime_record && this->realtime_track) {
		if (!this->realtime_track->empty()) {
			this->trw_children[TRW_REALTIME]->delete_track(this->realtime_track);
		}
		this->realtime_track = NULL;
	}
}




static void gps_start_stop_tracking_cb(gps_layer_data_t * data)
{
	LayerGPS * layer = data->layer;

	layer->realtime_tracking = (layer->realtime_tracking == false);

	/* Make sure we are still in the boat with libgps. */
	assert ((((int) GPSFixMode::FIX_2D) == MODE_2D) && (((int) GPSFixMode::FIX_3D) == MODE_3D));

	if (layer->realtime_tracking) {
		layer->first_realtime_trackpoint = true;
		if (!layer->rt_gpsd_connect(true)) {
			layer->first_realtime_trackpoint = false;
			layer->realtime_tracking = false;
			layer->tp = NULL;
		}
	} else {  /* Stop realtime tracking. */
		layer->first_realtime_trackpoint = false;
		layer->tp = NULL;
		layer->rt_gpsd_disconnect();
	}
}
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */




LayerGPS::LayerGPS()
{
	this->type = LayerType::GPS;
	strcpy(this->debug_string, "GPS");
	this->interface = &vik_gps_layer_interface;

#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
#ifdef K
	this->realtime_track_pen = viewport->new_pen("#203070", 2);
	this->realtime_track_bg_pen = viewport->new_pen("grey", 2);
	this->realtime_track_pt1_pen = viewport->new_pen("red", 2);
	this->realtime_track_pt2_pen = viewport->new_pen("green", 2);
	this->realtime_track_pt_pen = this->realtime_track_pt1_pen;
#endif
	this->gpsd_host = ""; //strdup("host"); TODO
	this->gpsd_port = ""; //strdup("port"); TODO

#endif // VIK_CONFIG_REALTIME_GPS_TRACKING

	this->set_initial_parameter_values();
	this->set_name(Layer::get_type_ui_label(this->type));

	for (int i = 0; i < NUM_TRW; i++) {
		this->trw_children[i] = new LayerTRW();
		uint16_t new_value = ~((uint16_t) LayerMenuItem::CUT | (uint16_t) LayerMenuItem::DELETE);
		new_value &= ((uint16_t) LayerMenuItem::ALL);
		this->trw_children[i]->set_menu_selection((LayerMenuItem) new_value);
	}
}




/* To be called right after constructor. */
void LayerGPS::set_coord_mode(CoordMode mode)
{
	for (int i = 0; i < NUM_TRW; i++) {
		this->trw_children[i]->set_coord_mode(mode);
	}
}
