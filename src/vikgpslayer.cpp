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

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixdata.h>

#include <stdlib.h>
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#include "viking.h"
#include "icons/icons.h"
#include "babel.h"
#include "dialog.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <stdlib.h>
#include <assert.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#ifdef VIK_CONFIG_REALTIME_GPS_TRACKING
#include <gps.h>
#include "vikutils.h"
#endif
#include "vikgpslayer.h"
#include "settings.h"
#include "globals.h"

extern GList * a_babel_device_list;


static VikGpsLayer * vik_gps_layer_new(Viewport * viewport);
static VikGpsLayer * gps_layer_unmarshall(uint8_t *data, int len, Viewport * viewport);
static bool gps_layer_set_param(VikGpsLayer *vgl, uint16_t id, VikLayerParamData data, Viewport * viewport, bool is_file_operation);
static VikLayerParamData gps_layer_get_param(VikGpsLayer *vgl, uint16_t id, bool is_file_operation);


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

// Shouldn't need to use these much any more as the protocol is now saved as a string.
// They are kept for compatibility loading old .vik files
typedef enum {
	GARMIN_P = 0,
	MAGELLAN_P,
	DELORME_P,
	NAVILINK_P,
	OLD_NUM_PROTOCOLS
} vik_gps_proto;

static char * protocols_args[] = {
	(char *) "garmin",
	(char *) "magellan",
	(char *) "delbin",
	(char *) "navilink",
	NULL
};

#ifdef WINDOWS
static char * params_ports[] = {
	(char *) "com1",
	(char *) "usb:",
	NULL
};
#else
static char * params_ports[] = {
	(char *) "/dev/ttyS0",
	(char *) "/dev/ttyS1",
	(char *) "/dev/ttyUSB0",
	(char *) "/dev/ttyUSB1",
	(char *) "usb:",
	NULL
};
#endif

/* NUM_PORTS not actually used */
/* #define NUM_PORTS (sizeof(params_ports)/sizeof(params_ports[0]) - 1) */
/* Compatibility with previous versions */
#ifdef WINDOWS
static char * old_params_ports[] = {
	(char *) "com1",
	(char *) "usb:",
	NULL
};
#else
static char * old_params_ports[] = {
	(char *) "/dev/ttyS0",
	(char *) "/dev/ttyS1",
	(char *) "/dev/ttyUSB0",
	(char *) "/dev/ttyUSB1",
	(char *) "usb:",
	NULL
};
#endif
#define OLD_NUM_PORTS (sizeof(old_params_ports)/sizeof(old_params_ports[0]) - 1)

typedef struct {
	GMutex * mutex;
	vik_gps_dir direction;
	char * port;
	bool ok;
	int total_count;
	int count;
	VikTrwLayer * vtl;
	Track * trk;
	char * babelargs;
	char * window_title;
	GtkWidget * dialog;
	GtkWidget * status_label;
	GtkWidget * gps_label;
	GtkWidget * ver_label;
	GtkWidget * id_label;
	GtkWidget * wp_label;
	GtkWidget * trk_label;
	GtkWidget * rte_label;
	GtkWidget * progress_label;
	vik_gps_xfer_type progress_type;
	Viewport * viewport;
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	bool realtime_tracking;
#endif
} GpsSession;
static void gps_session_delete(GpsSession *sess);

static char *params_groups[] = {
	(char *) N_("Data Mode"),
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	(char *) N_("Realtime Tracking Mode"),
#endif
};

enum {
	GROUP_DATA_MODE,
	GROUP_REALTIME_MODE
};


static VikLayerParamData gps_protocol_default(void)
{
	VikLayerParamData data;
	data.s = g_strdup("garmin");
	return data;
}

static VikLayerParamData gps_port_default(void)
{
	VikLayerParamData data;
	data.s = g_strdup("usb:");
#ifndef WINDOWS
	/* Attempt to auto set default USB serial port entry */
	/* Ordered to make lowest device favourite if available */
	if (g_access("/dev/ttyUSB1", R_OK) == 0) {
		if (data.s) {
			free((char *)data.s);
		}
		data.s = g_strdup("/dev/ttyUSB1");
	}
	if (g_access("/dev/ttyUSB0", R_OK) == 0) {
		if (data.s) {
			free((char *)data.s);
		}
		data.s = g_strdup("/dev/ttyUSB0");
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

static VikLayerParamData moving_map_method_default(void) { return VIK_LPD_UINT (VEHICLE_POSITION_ON_SCREEN); }

static VikLayerParamData gpsd_host_default(void)
{
	VikLayerParamData data;
	data.s = g_strdup("localhost");
	return data;
}

static VikLayerParamData gpsd_port_default(void)
{
	VikLayerParamData data;
	data.s = g_strdup(DEFAULT_GPSD_PORT);
	return data;
}

static VikLayerParamData gpsd_retry_interval_default(void)
{
	VikLayerParamData data;
	data.s = g_strdup("10");
	return data;
}

#endif

static VikLayerParam gps_layer_params[] = {
	//	NB gps_layer_inst_init() is performed after parameter registeration
	//  thus to give the protocols some potential values use the old static list
	// TODO: find another way to use gps_layer_inst_init()?
	{ VIK_LAYER_GPS, "gps_protocol",              VIK_LAYER_PARAM_STRING,  GROUP_DATA_MODE,     N_("GPS Protocol:"),                     VIK_LAYER_WIDGET_COMBOBOX,          protocols_args,          NULL, NULL, gps_protocol_default,        NULL, NULL }, // List reassigned at runtime
	{ VIK_LAYER_GPS, "gps_port",                  VIK_LAYER_PARAM_STRING,  GROUP_DATA_MODE,     N_("Serial Port:"),                      VIK_LAYER_WIDGET_COMBOBOX,          params_ports,            NULL, NULL, gps_port_default,            NULL, NULL },
	{ VIK_LAYER_GPS, "gps_download_tracks",       VIK_LAYER_PARAM_BOOLEAN, GROUP_DATA_MODE,     N_("Download Tracks:"),                  VIK_LAYER_WIDGET_CHECKBUTTON,       NULL,                    NULL, NULL, vik_lpd_true_default,        NULL, NULL },
	{ VIK_LAYER_GPS, "gps_upload_tracks",         VIK_LAYER_PARAM_BOOLEAN, GROUP_DATA_MODE,     N_("Upload Tracks:"),                    VIK_LAYER_WIDGET_CHECKBUTTON,       NULL,                    NULL, NULL, vik_lpd_true_default,        NULL, NULL },
	{ VIK_LAYER_GPS, "gps_download_routes",       VIK_LAYER_PARAM_BOOLEAN, GROUP_DATA_MODE,     N_("Download Routes:"),                  VIK_LAYER_WIDGET_CHECKBUTTON,       NULL,                    NULL, NULL, vik_lpd_true_default,        NULL, NULL },
	{ VIK_LAYER_GPS, "gps_upload_routes",         VIK_LAYER_PARAM_BOOLEAN, GROUP_DATA_MODE,     N_("Upload Routes:"),                    VIK_LAYER_WIDGET_CHECKBUTTON,       NULL,                    NULL, NULL, vik_lpd_true_default,        NULL, NULL },
	{ VIK_LAYER_GPS, "gps_download_waypoints",    VIK_LAYER_PARAM_BOOLEAN, GROUP_DATA_MODE,     N_("Download Waypoints:"),               VIK_LAYER_WIDGET_CHECKBUTTON,       NULL,                    NULL, NULL, vik_lpd_true_default,        NULL, NULL },
	{ VIK_LAYER_GPS, "gps_upload_waypoints",      VIK_LAYER_PARAM_BOOLEAN, GROUP_DATA_MODE,     N_("Upload Waypoints:"),                 VIK_LAYER_WIDGET_CHECKBUTTON,       NULL,                    NULL, NULL, vik_lpd_true_default,        NULL, NULL },
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	{ VIK_LAYER_GPS, "record_tracking",           VIK_LAYER_PARAM_BOOLEAN, GROUP_REALTIME_MODE, N_("Recording tracks"),                  VIK_LAYER_WIDGET_CHECKBUTTON,       NULL,                    NULL, NULL, vik_lpd_true_default,        NULL, NULL },
	{ VIK_LAYER_GPS, "center_start_tracking",     VIK_LAYER_PARAM_BOOLEAN, GROUP_REALTIME_MODE, N_("Jump to current position on start"), VIK_LAYER_WIDGET_CHECKBUTTON,       NULL,                    NULL, NULL, vik_lpd_false_default,       NULL, NULL },
	{ VIK_LAYER_GPS, "moving_map_method",         VIK_LAYER_PARAM_UINT,    GROUP_REALTIME_MODE, N_("Moving Map Method:"),                VIK_LAYER_WIDGET_RADIOGROUP_STATIC, params_vehicle_position, NULL, NULL, moving_map_method_default,   NULL, NULL },
	{ VIK_LAYER_GPS, "realtime_update_statusbar", VIK_LAYER_PARAM_BOOLEAN, GROUP_REALTIME_MODE, N_("Update Statusbar:"),                 VIK_LAYER_WIDGET_CHECKBUTTON,       NULL,                    NULL, N_("Display information in the statusbar on GPS updates"), vik_lpd_true_default, NULL, NULL },
	{ VIK_LAYER_GPS, "gpsd_host",                 VIK_LAYER_PARAM_STRING,  GROUP_REALTIME_MODE, N_("Gpsd Host:"),                        VIK_LAYER_WIDGET_ENTRY,             NULL,                    NULL, NULL, gpsd_host_default,           NULL, NULL },
	{ VIK_LAYER_GPS, "gpsd_port",                 VIK_LAYER_PARAM_STRING,  GROUP_REALTIME_MODE, N_("Gpsd Port:"),                        VIK_LAYER_WIDGET_ENTRY,             NULL,                    NULL, NULL, gpsd_port_default,           NULL, NULL },
	{ VIK_LAYER_GPS, "gpsd_retry_interval",       VIK_LAYER_PARAM_STRING,  GROUP_REALTIME_MODE, N_("Gpsd Retry Interval (seconds):"),    VIK_LAYER_WIDGET_ENTRY,             NULL,                    NULL, NULL, gpsd_retry_interval_default, NULL, NULL },
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */
};
enum {
	PARAM_PROTOCOL=0,
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
	NUM_PARAMS};

VikLayerInterface vik_gps_layer_interface = {
	"GPS",
	N_("GPS"),
	NULL,
	&vikgpslayer_pixbuf,

	NULL,
	0,

	gps_layer_params,
	NUM_PARAMS,
	params_groups,
	sizeof(params_groups)/sizeof(params_groups[0]),

	VIK_MENU_ITEM_ALL,

	(VikLayerFuncUnmarshall)		gps_layer_unmarshall,

	(VikLayerFuncSetParam)                gps_layer_set_param,
	(VikLayerFuncGetParam)                gps_layer_get_param,
	(VikLayerFuncChangeParam)             NULL,
};

static char * trw_names[] = {
	(char *) N_("GPS Download"),
	(char *) N_("GPS Upload"),
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	(char *) N_("GPS Realtime Tracking"),
#endif
};



/**
 * Overwrite the static setup with dynamically generated GPS Babel device list
 */
static void gps_layer_inst_init(VikGpsLayer *self)
{
	int new_proto = 0;
	// +1 for luck (i.e the NULL terminator)
	char **new_protocols = (char **) g_malloc_n(1 + g_list_length(a_babel_device_list), sizeof(void *));

	GList *gl = g_list_first(a_babel_device_list);
	while (gl) {
		// should be using label property but use name for now
		//  thus don't need to mess around converting label to name later on
		new_protocols[new_proto++] = ((BabelDevice*)gl->data)->name;
		gl = g_list_next(gl);
	}
	new_protocols[new_proto] = NULL;

	vik_gps_layer_interface.params[PARAM_PROTOCOL].widget_data = new_protocols;
}

GType vik_gps_layer_get_type()
{
	static GType val_type = 0;

	if (!val_type) {
		static const GTypeInfo val_info = {
			sizeof (VikGpsLayerClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			NULL, /* class init */
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (VikGpsLayer),
			0,
			(GInstanceInitFunc) gps_layer_inst_init,
		};
		val_type = g_type_register_static(VIK_LAYER_TYPE, "VikGpsLayer", &val_info, (GTypeFlags) 0);
	}

	return val_type;
}

#if 0
VikGpsLayer * vik_gps_layer_create(Viewport * viewport)
{
	VikGpsLayer *rv = vik_gps_layer_new(viewport);
	LayerGPS * layer = (LayerGPS *) ((VikLayer *) rv)->layer;
	layer->rename(vik_gps_layer_interface.name);

	for (int i = 0; i < NUM_TRW; i++) {
		layer->trw_children[i] = new LayerTRW(viewport);
		vik_layer_set_menu_items_selection(layer->trw_children[i]->vl, VIK_MENU_ITEM_ALL & ~(VIK_MENU_ITEM_CUT|VIK_MENU_ITEM_DELETE));
	}

	return rv;
}
#endif

char const * LayerGPS::tooltip()
{
	return this->protocol;
}

/* "Copy" */
void LayerGPS::marshall(uint8_t **data, int *datalen)
{
	VikLayer *child_layer;
	uint8_t *ld;
	int ll;
	GByteArray* b = g_byte_array_new();
	int len;
	int i;

#define alm_append(obj, sz) 	\
	len = (sz);						\
	g_byte_array_append(b, (uint8_t *)&len, sizeof(len));	\
	g_byte_array_append(b, (uint8_t *)(obj), len);

	vik_layer_marshall_params(VIK_LAYER(this->vl), &ld, &ll);
	alm_append(ld, ll);
	free(ld);

	for (i = 0; i < NUM_TRW; i++) {
		child_layer = this->trw_children[i]->vl;
		vik_layer_marshall(child_layer, &ld, &ll);
		if (ld) {
			alm_append(ld, ll);
			free(ld);
		}
	}
	*data = b->data;
	*datalen = b->len;
	g_byte_array_free(b, false);
#undef alm_append
}

/* "Paste" */
static VikGpsLayer * gps_layer_unmarshall(uint8_t *data, int len, Viewport * viewport)
{
#define alm_size (*(int *)data)
#define alm_next		 \
	len -= sizeof(int) + alm_size;		\
	data += sizeof(int) + alm_size;

	VikGpsLayer *rv = vik_gps_layer_new(viewport);
	LayerGPS * layer = (LayerGPS *) ((VikLayer *) rv)->layer;
	VikLayer *child_layer;
	int i;

	vik_layer_unmarshall_params(VIK_LAYER(rv), data+sizeof(int), alm_size, viewport);
	alm_next;

	i = 0;
	while (len>0 && i < NUM_TRW) {
		child_layer = vik_layer_unmarshall(data + sizeof(int), alm_size, viewport);
		if (child_layer) {
			layer->trw_children[i++] = ((VikTrwLayer *) child_layer)->trw;
			// NB no need to attach signal update handler here
			//  as this will always be performed later on in vik_gps_layer_realize()
		}
		alm_next;
	}
	//  fprintf(stdout, "gps_layer_unmarshall ended with len=%d\n", len);
	assert (len == 0);
	return rv;
#undef alm_size
#undef alm_next
}

static bool gps_layer_set_param(VikGpsLayer *vgl, uint16_t id, VikLayerParamData data, Viewport * viewport, bool is_file_operation)
{
	LayerGPS * layer = (LayerGPS *) ((VikLayer *) vgl)->layer;

	switch (id) {
	case PARAM_PROTOCOL:
		if (data.s) {
			free(layer->protocol);
			// Backwards Compatibility: previous versions <v1.4 stored protocol as an array index
			int index = data.s[0] - '0';
			if (data.s[0] != '\0' &&
			    g_ascii_isdigit (data.s[0]) &&
			    data.s[1] == '\0' &&
			    index < OLD_NUM_PROTOCOLS) {

				// It is a single digit: activate compatibility
				layer->protocol = g_strdup(protocols_args[index]);
			} else {
				layer->protocol = g_strdup(data.s);
			}
			fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, layer->protocol);
		} else {
			fprintf(stderr, _("WARNING: Unknown GPS Protocol\n"));
		}
		break;
	case PARAM_PORT:
		if (data.s) {
			free(layer->serial_port);
			// Backwards Compatibility: previous versions <v0.9.91 stored serial_port as an array index
			int index = data.s[0] - '0';
			if (data.s[0] != '\0' &&
			    g_ascii_isdigit(data.s[0]) &&
			    data.s[1] == '\0' &&
			    index < OLD_NUM_PORTS) {

				/* It is a single digit: activate compatibility */
				layer->serial_port = g_strdup(old_params_ports[index]);

			} else {
				layer->serial_port = g_strdup(data.s);
			}
			fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, layer->serial_port);
		} else {
			fprintf(stderr, _("WARNING: Unknown serial port device\n"));
		}
		break;
	case PARAM_DOWNLOAD_TRACKS:
		layer->download_tracks = data.b;
		break;
	case PARAM_UPLOAD_TRACKS:
		layer->upload_tracks = data.b;
		break;
	case PARAM_DOWNLOAD_ROUTES:
		layer->download_routes = data.b;
		break;
	case PARAM_UPLOAD_ROUTES:
		layer->upload_routes = data.b;
		break;
	case PARAM_DOWNLOAD_WAYPOINTS:
		layer->download_waypoints = data.b;
		break;
	case PARAM_UPLOAD_WAYPOINTS:
		layer->upload_waypoints = data.b;
		break;
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	case PARAM_GPSD_HOST:
		if (data.s) {
			if (layer->gpsd_host) {
				free(layer->gpsd_host);
			}
			layer->gpsd_host = g_strdup(data.s);
		}
		break;
	case PARAM_GPSD_PORT:
		if (data.s) {
			if (layer->gpsd_port) {
				free(layer->gpsd_port);
			}
			layer->gpsd_port = g_strdup(data.s);
		}
		break;
	case PARAM_GPSD_RETRY_INTERVAL:
		layer->gpsd_retry_interval = strtol(data.s, NULL, 10);
		break;
	case PARAM_REALTIME_REC:
		layer->realtime_record = data.b;
		break;
	case PARAM_REALTIME_CENTER_START:
		layer->realtime_jump_to_start = data.b;
		break;
	case PARAM_VEHICLE_POSITION:
		layer->vehicle_position = data.u;
		break;
	case PARAM_REALTIME_UPDATE_STATUSBAR:
		layer->realtime_update_statusbar = data.b;
		break;
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */
	default:
		fprintf(stderr, "WARNING: gps_layer_set_param(): unknown parameter\n");
	}

	return true;
}

static VikLayerParamData gps_layer_get_param(VikGpsLayer *vgl, uint16_t id, bool is_file_operation)
{
	LayerGPS * layer = (LayerGPS *) ((VikLayer *) vgl)->layer;

	VikLayerParamData rv;
	switch (id) {
	case PARAM_PROTOCOL:
		rv.s = layer->protocol;
		fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, rv.s);
		break;
	case PARAM_PORT:
		rv.s = layer->serial_port;
		fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, rv.s);
		break;
	case PARAM_DOWNLOAD_TRACKS:
		rv.b = layer->download_tracks;
		break;
	case PARAM_UPLOAD_TRACKS:
		rv.b = layer->upload_tracks;
		break;
	case PARAM_DOWNLOAD_ROUTES:
		rv.b = layer->download_routes;
		break;
	case PARAM_UPLOAD_ROUTES:
		rv.b = layer->upload_routes;
		break;
	case PARAM_DOWNLOAD_WAYPOINTS:
		rv.b = layer->download_waypoints;
		break;
	case PARAM_UPLOAD_WAYPOINTS:
		rv.b = layer->upload_waypoints;
		break;
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	case PARAM_GPSD_HOST:
		rv.s = layer->gpsd_host ? layer->gpsd_host : "";
		break;
	case PARAM_GPSD_PORT:
		rv.s = layer->gpsd_port ? layer->gpsd_port : g_strdup(DEFAULT_GPSD_PORT);
		break;
	case PARAM_GPSD_RETRY_INTERVAL:
		rv.s = g_strdup_printf("%d", layer->gpsd_retry_interval);
		break;
	case PARAM_REALTIME_REC:
		rv.b = layer->realtime_record;
		break;
	case PARAM_REALTIME_CENTER_START:
		rv.b = layer->realtime_jump_to_start;
		break;
	case PARAM_VEHICLE_POSITION:
		rv.u = layer->vehicle_position;
		break;
	case PARAM_REALTIME_UPDATE_STATUSBAR:
		rv.u = layer->realtime_update_statusbar;
		break;
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */
	default:
		fprintf(stderr, _("WARNING: %s: unknown parameter\n"), __FUNCTION__);
	}

	return rv;
}

VikGpsLayer *vik_gps_layer_new(Viewport * viewport)
{
	VikGpsLayer *vgl = VIK_GPS_LAYER (g_object_new(VIK_GPS_LAYER_TYPE, NULL));
	((VikLayer *) vgl)->layer = new LayerGPS((VikLayer *) vgl);
	LayerGPS * layer = (LayerGPS *) ((VikLayer *) vgl)->layer;

	for (int i = 0; i < NUM_TRW; i++) {
		layer->trw_children[i] = NULL;
	}
	layer->children = NULL;
	layer->cur_read_child = 0;

#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	layer->vgpsd = NULL;
	layer->first_realtime_trackpoint = false;
	layer->realtime_track = NULL;

	layer->realtime_io_channel = NULL;
	layer->realtime_io_watch_id = 0;
	layer->realtime_retry_timer = 0;
	if (viewport) {
		layer->realtime_track_gc = viewport->new_gc("#203070", 2);
		layer->realtime_track_bg_gc = viewport->new_gc("grey", 2);
		layer->realtime_track_pt1_gc = viewport->new_gc("red", 2);
		layer->realtime_track_pt2_gc = viewport->new_gc("green", 2);
		layer->realtime_track_pt_gc = layer->realtime_track_pt1_gc;
	}

	layer->gpsd_host = NULL;
	layer->gpsd_port = NULL;

	layer->tp = NULL;
	layer->tp_prev = NULL;

#endif // VIK_CONFIG_REALTIME_GPS_TRACKING

	layer->protocol = NULL;
	layer->serial_port = NULL;

	vik_layer_set_defaults(VIK_LAYER(vgl), viewport);

	return vgl;
}

void LayerGPS::draw(Viewport * viewport)
{
	VikLayer *trigger = VIK_LAYER(viewport->get_trigger());

	for (int i = 0; i < NUM_TRW; i++) {
		LayerTRW * trw = this->trw_children[i];
		if (trw->vl == trigger) {
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
		if (this->vl == trigger) {
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

void LayerGPS::change_coord_mode(VikCoordMode mode)
{
	for (int i = 0; i < NUM_TRW; i++) {
		vik_layer_change_coord_mode(this->trw_children[i]->vl, mode);
	}
}

void LayerGPS::add_menu_items(GtkMenu * menu, void * vlp)
{
	static gps_layer_data_t pass_along;
	GtkWidget *item;
	pass_along.layer = this;
	pass_along.panel = ((VikLayersPanel *) vlp)->panel_ref;

	item = gtk_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	/* Now with icons */
	item = gtk_image_menu_item_new_with_mnemonic(_("_Upload to GPS"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_GO_UP, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(gps_upload_cb), &pass_along);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	item = gtk_image_menu_item_new_with_mnemonic(_("Download from _GPS"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(gps_download_cb), &pass_along);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	item = gtk_image_menu_item_new_with_mnemonic(this->realtime_tracking  ?
						       "_Stop Realtime Tracking" :
						       "_Start Realtime Tracking");
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, this->realtime_tracking ?
					gtk_image_new_from_stock(GTK_STOCK_MEDIA_STOP, GTK_ICON_SIZE_MENU) :
					gtk_image_new_from_stock(GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(gps_start_stop_tracking_cb), &pass_along);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);

	item = gtk_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	item = gtk_image_menu_item_new_with_mnemonic(_("Empty _Realtime"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(gps_empty_realtime_cb), &pass_along);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */

	item = gtk_image_menu_item_new_with_mnemonic(_("E_mpty Upload"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(gps_empty_upload_cb), &pass_along);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);

	item = gtk_image_menu_item_new_with_mnemonic(_("_Empty Download"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(gps_empty_download_cb), &pass_along);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);

	item = gtk_image_menu_item_new_with_mnemonic(_("Empty _All"));
	gtk_image_menu_item_set_image((GtkImageMenuItem*)item, gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(gps_empty_all_cb), &pass_along);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	gtk_widget_show(item);

}

void LayerGPS::disconnect_layer_signal(VikLayer * vl)
{
	unsigned int number_handlers = g_signal_handlers_disconnect_matched(vl, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this->vl);
	if (number_handlers != 1) {
		fprintf(stderr, _("CRITICAL: Unexpected number of disconnected handlers: %d\n"), number_handlers);
	}
}

void LayerGPS::free_()
{
	for (int i = 0; i < NUM_TRW; i++) {
		if (this->realized) {
			this->disconnect_layer_signal(this->trw_children[i]->vl);
		}
		g_object_unref(this->trw_children[i]->vl);
	}
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	this->rt_gpsd_disconnect();
	if (this->realtime_track_gc != NULL) {
		g_object_unref(this->realtime_track_gc);
	}

	if (this->realtime_track_bg_gc != NULL) {
		g_object_unref(this->realtime_track_bg_gc);
	}

	if (this->realtime_track_pt1_gc != NULL) {
		g_object_unref(this->realtime_track_pt1_gc);
	}

	if (this->realtime_track_pt2_gc != NULL) {
		g_object_unref(this->realtime_track_pt2_gc);
	}
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */
}

void LayerGPS::realize(VikTreeview *vt, GtkTreeIter *layer_iter)
{
	GtkTreeIter iter;

	this->vt = vt;
	this->iter = *layer_iter;
	this->realized = true;

	// TODO set to garmin by default
	//if (a_babel_device_list)
	// device = ((BabelDevice*)g_list_nth_data(a_babel_device_list, last_active))->name;
	// Need to access uibuild widgets somehow....

	for (int ix = 0; ix < NUM_TRW; ix++) {
		LayerTRW * trw = this->trw_children[ix];
		this->vt->tree->add_layer(layer_iter, &iter,
					  _(trw_names[ix]), this, true,
					  trw, trw->type, trw->type, trw->get_timestamp());
		if (!trw->visible) {
			this->vt->tree->set_visibility(&iter, false);
		}
		trw->realize(this->vt, &iter);
		g_signal_connect_swapped(G_OBJECT(trw->vl), "update", G_CALLBACK(vik_layer_emit_update_secondary), this->vl);
	}
}

const GList * LayerGPS::get_children()
{
	if (this->children == NULL) {
		for (int i = NUM_TRW - 1; i >= 0; i--) {
			this->children = g_list_prepend(this->children, this->trw_children[i]);
		}
	}
	return this->children;
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
	vik_mutex_free(sess->mutex);
	free(sess->babelargs);
	free(sess);
}

static void set_total_count(int cnt, GpsSession *sess)
{
	char s[128];
	gdk_threads_enter();
	g_mutex_lock(sess->mutex);
	if (sess->ok) {
		const char *tmp_str;
		if (sess->direction == GPS_DOWN) {
			switch (sess->progress_type) {
			case WPT:
				tmp_str = ngettext("Downloading %d waypoint...", "Downloading %d waypoints...", cnt); sess->total_count = cnt;
				break;
			case TRK:
				tmp_str = ngettext("Downloading %d trackpoint...", "Downloading %d trackpoints...", cnt); sess->total_count = cnt;
				break;
			default: {
				// Maybe a gpsbabel bug/feature (upto at least v1.4.3 or maybe my Garmin device) but the count always seems x2 too many for routepoints
				int mycnt = (cnt / 2) + 1;
				tmp_str = ngettext("Downloading %d routepoint...", "Downloading %d routepoints...", mycnt);
				sess->total_count = mycnt;
				break;
			}
			}
		} else {
			switch (sess->progress_type) {
			case WPT:
				tmp_str = ngettext("Uploading %d waypoint...", "Uploading %d waypoints...", cnt);
				break;
			case TRK:
				tmp_str = ngettext("Uploading %d trackpoint...", "Uploading %d trackpoints...", cnt);
				break;
			default:
				tmp_str = ngettext("Uploading %d routepoint...", "Uploading %d routepoints...", cnt);
				break;
			}
		}

		snprintf(s, 128, tmp_str, cnt);
		gtk_label_set_text(GTK_LABEL(sess->progress_label), s);
		gtk_widget_show(sess->progress_label);
		sess->total_count = cnt;
	}
	g_mutex_unlock(sess->mutex);
	gdk_threads_leave();
}

static void set_current_count(int cnt, GpsSession *sess)
{
	char s[128];
	const char *tmp_str;

	gdk_threads_enter();
	g_mutex_lock(sess->mutex);
	if (sess->ok) {
		if (cnt < sess->total_count) {
			if (sess->direction == GPS_DOWN) {
				switch (sess->progress_type) {
				case WPT:
					tmp_str = ngettext("Downloaded %d out of %d waypoint...", "Downloaded %d out of %d waypoints...", sess->total_count);
					break;
				case TRK:
					tmp_str = ngettext("Downloaded %d out of %d trackpoint...", "Downloaded %d out of %d trackpoints...", sess->total_count);
					break;
				default:
					tmp_str = ngettext("Downloaded %d out of %d routepoint...", "Downloaded %d out of %d routepoints...", sess->total_count);
					break;
				}
			} else {
				switch (sess->progress_type) {
				case WPT:
					tmp_str = ngettext("Uploaded %d out of %d waypoint...", "Uploaded %d out of %d waypoints...", sess->total_count);
					break;
				case TRK:
					tmp_str = ngettext("Uploaded %d out of %d trackpoint...", "Uploaded %d out of %d trackpoints...", sess->total_count);
					break;
				default:
					tmp_str = ngettext("Uploaded %d out of %d routepoint...", "Uploaded %d out of %d routepoints...", sess->total_count);
					break;
				}
			}
			snprintf(s, 128, tmp_str, cnt, sess->total_count);
		} else {
			if (sess->direction == GPS_DOWN) {
				switch (sess->progress_type) {
				case WPT:
					tmp_str = ngettext("Downloaded %d waypoint", "Downloaded %d waypoints", cnt);
					break;
				case TRK:
					tmp_str = ngettext("Downloaded %d trackpoint", "Downloaded %d trackpoints", cnt);
					break;
				default:
					tmp_str = ngettext("Downloaded %d routepoint", "Downloaded %d routepoints", cnt);
					break;
				}
			} else {
				switch (sess->progress_type) {
				case WPT:
					tmp_str = ngettext("Uploaded %d waypoint", "Uploaded %d waypoints", cnt);
					break;
				case TRK:
					tmp_str = ngettext("Uploaded %d trackpoint", "Uploaded %d trackpoints", cnt);
					break;
				default:
					tmp_str = ngettext("Uploaded %d routepoint", "Uploaded %d routepoints", cnt);
					break;
				}
			}
			snprintf(s, 128, tmp_str, cnt);
		}
		gtk_label_set_text(GTK_LABEL(sess->progress_label), s);
	}
	g_mutex_unlock(sess->mutex);
	gdk_threads_leave();
}

static void set_gps_info(const char *info, GpsSession *sess)
{
	char s[256];
	gdk_threads_enter();
	g_mutex_lock(sess->mutex);
	if (sess->ok) {
		snprintf(s, 256, _("GPS Device: %s"), info);
		gtk_label_set_text (GTK_LABEL(sess->gps_label), s);
	}
	g_mutex_unlock(sess->mutex);
	gdk_threads_leave();
}

/*
 * Common processing for GPS Device information
 * It doesn't matter whether we're uploading or downloading
 */
static void process_line_for_gps_info(const char *line, GpsSession *sess)
{
	if (strstr(line, "PRDDAT")) {
		char **tokens = g_strsplit(line, " ", 0);
		char info[128];
		int ilen = 0;
		int i;
		int n_tokens = 0;

		while (tokens[n_tokens]) {
			n_tokens++;
		}

		// I'm not entirely clear what information this is trying to get...
		//  Obviously trying to decipher some kind of text/naming scheme
		//  Anyway this will be superceded if there is 'Unit:' information
		if (n_tokens > 8) {
			for (i=8; tokens[i] && ilen < sizeof(info)-2 && strcmp(tokens[i], "00"); i++) {
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
	char *line;

	gdk_threads_enter();
	g_mutex_lock(sess->mutex);
	if (!sess->ok) {
		g_mutex_unlock(sess->mutex);
		gps_session_delete(sess);
		gdk_threads_leave();
		g_thread_exit(NULL);
	}
	g_mutex_unlock(sess->mutex);
	gdk_threads_leave();

	switch(c) {
	case BABEL_DIAG_OUTPUT:
		line = (char *)data;

		gdk_threads_enter();
		g_mutex_lock(sess->mutex);
		if (sess->ok) {
			gtk_label_set_text(GTK_LABEL(sess->status_label), _("Status: Working..."));
		}
		g_mutex_unlock(sess->mutex);
		gdk_threads_leave();

		/* tells us the type of items that will follow */
		if (strstr(line, "Xfer Wpt")) {
			sess->progress_label = sess->wp_label;
			sess->progress_type = WPT;
		}
		if (strstr(line, "Xfer Trk")) {
			sess->progress_label = sess->trk_label;
			sess->progress_type = TRK;
		}
		if (strstr(line, "Xfer Rte")) {
			sess->progress_label = sess->rte_label;
			sess->progress_type = RTE;
		}

		process_line_for_gps_info(line, sess);

		if (strstr(line, "RECORD")) {
			int lsb, msb, cnt;

			if (strlen(line) > 20) {
				sscanf(line+17, "%x", &lsb);
				sscanf(line+20, "%x", &msb);
				cnt = lsb + msb * 256;
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

}

static void gps_upload_progress_func(BabelProgressCode c, void * data, GpsSession * sess)
{
	char *line;
	static int cnt = 0;

	gdk_threads_enter();
	g_mutex_lock(sess->mutex);
	if (!sess->ok) {
		g_mutex_unlock(sess->mutex);
		gps_session_delete(sess);
		gdk_threads_leave();
		g_thread_exit(NULL);
	}
	g_mutex_unlock(sess->mutex);
	gdk_threads_leave();

	switch(c) {
	case BABEL_DIAG_OUTPUT:
		line = (char *)data;

		gdk_threads_enter();
		g_mutex_lock(sess->mutex);
		if (sess->ok) {
			gtk_label_set_text(GTK_LABEL(sess->status_label), _("Status: Working..."));
		}
		g_mutex_unlock(sess->mutex);
		gdk_threads_leave();

		process_line_for_gps_info(line, sess);

		if (strstr(line, "RECORD")) {
			int lsb, msb;

			if (strlen(line) > 20) {
				sscanf(line+17, "%x", &lsb);
				sscanf(line+20, "%x", &msb);
				cnt = lsb + msb * 256;
				/* set_total_count(cnt, sess); */
				sess->count = 0;
			}
		}
		if (strstr(line, "WPTDAT")) {
			if (sess->count == 0) {
				sess->progress_label = sess->wp_label;
				sess->progress_type = WPT;
				set_total_count(cnt, sess);
			}
			sess->count++;
			set_current_count(sess->count, sess);
		}
		if (strstr(line, "RTEHDR") || strstr(line, "RTEWPT")) {
			if (sess->count == 0) {
				sess->progress_label = sess->rte_label;
				sess->progress_type = RTE;
				// Maybe a gpsbabel bug/feature (upto at least v1.4.3 or maybe my Garmin device) but the count always seems x2 too many for routepoints
				// Anyway since we're uploading - we should know how many points we're going to put!
				cnt = (cnt / 2) + 1;
				set_total_count(cnt, sess);
			}
			sess->count++;
			set_current_count(sess->count, sess);
		}
		if (strstr(line, "TRKHDR") || strstr(line, "TRKDAT")) {
			if (sess->count == 0) {
				sess->progress_label = sess->trk_label;
				sess->progress_type = TRK;
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

}

static void gps_comm_thread(GpsSession *sess)
{
	bool result;

	if (sess->direction == GPS_DOWN) {
		ProcessOptions po = { sess->babelargs, sess->port, NULL, NULL, NULL, NULL };
		result = a_babel_convert_from(sess->vtl, &po, (BabelStatusFunc) gps_download_progress_func, sess, NULL);
	} else {
		result = a_babel_convert_to(sess->vtl, sess->trk, sess->babelargs, sess->port,
					     (BabelStatusFunc) gps_upload_progress_func, sess);
	}

	if (!result) {
		gtk_label_set_text(GTK_LABEL(sess->status_label), _("Error: couldn't find gpsbabel."));
	} else {
		g_mutex_lock(sess->mutex);
		if (sess->ok) {
			gtk_label_set_text(GTK_LABEL(sess->status_label), _("Done."));
			gtk_dialog_set_response_sensitive(GTK_DIALOG(sess->dialog), GTK_RESPONSE_ACCEPT, true);
			gtk_dialog_set_response_sensitive(GTK_DIALOG(sess->dialog), GTK_RESPONSE_REJECT, false);

			/* Do not change the view if we are following the current GPS position */
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
			if (!sess->realtime_tracking)
#endif
				{
					if (sess->viewport && sess->direction == GPS_DOWN) {
						vik_layer_post_read(VIK_LAYER(sess->vtl), sess->viewport, true);
						/* View the data available */
						sess->vtl->trw->auto_set_view(sess->viewport) ;
						vik_layer_emit_update(VIK_LAYER(sess->vtl)); // NB update from background thread
					}
				}
		} else {
			/* canceled */
		}
		g_mutex_unlock(sess->mutex);
	}

	g_mutex_lock(sess->mutex);
	if (sess->ok) {
		sess->ok = false;
		g_mutex_unlock(sess->mutex);
	} else {
		g_mutex_unlock(sess->mutex);
		gps_session_delete(sess);
	}
	g_thread_exit(NULL);
}

/**
 * vik_gps_comm:
 * @vtl: The TrackWaypoint layer to operate on
 * @track: Operate on a particular track when specified
 * @dir: The direction of the transfer
 * @protocol: The GPS device communication protocol
 * @port: The GPS serial port
 * @tracking: If tracking then viewport display update will be skipped
 * @vvp: A viewport is required as the display may get updated
 * @vlp: A layers panel is needed for uploading as the items maybe modified
 * @do_tracks: Whether tracks shoud be processed
 * @do_waypoints: Whether waypoints shoud be processed
 * @turn_off: Whether we should attempt to turn off the GPS device after the transfer (only some devices support this)
 *
 * Talk to a GPS Device using a thread which updates a dialog with the progress
 */
int vik_gps_comm(VikTrwLayer *vtl,
		 Track * trk,
		 vik_gps_dir dir,
		 char *protocol,
		 char *port,
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

	sess->mutex = vik_mutex_new();
	sess->direction = dir;
	sess->vtl = vtl;
	sess->trk = trk;
	sess->port = g_strdup(port);
	sess->ok = true;
	sess->window_title = (dir == GPS_DOWN) ? _("GPS Download") : _("GPS Upload");
	sess->viewport = viewport;

	// This must be done inside the main thread as the uniquify causes screen updates
	//  (originally performed this nearer the point of upload in the thread)
	if (dir == GPS_UP) {
		// Enforce unique names in the layer upload to the GPS device
		// NB this may only be a Garmin device restriction (and may be not every Garmin device either...)
		// Thus this maintains the older code in built restriction
		if (!sess->vtl->trw->uniquify(panel)) {
			vik_statusbar_set_message(vik_window_get_statusbar(VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(sess->vtl))), VIK_STATUSBAR_INFO,
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

	sess->babelargs = g_strdup_printf("-D 9 %s %s %s -%c %s",
					  tracks, routes, waypoints, (dir == GPS_DOWN) ? 'i' : 'o', protocol);
	tracks = NULL;
	waypoints = NULL;

	// Only create dialog if we're going to do some transferring
	if (do_tracks || do_waypoints || do_routes) {
		sess->dialog = gtk_dialog_new_with_buttons("", VIK_GTK_WINDOW_FROM_LAYER(vtl), (GtkDialogFlags) 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);
		gtk_dialog_set_response_sensitive(GTK_DIALOG(sess->dialog),
						    GTK_RESPONSE_ACCEPT, false);
		gtk_window_set_title(GTK_WINDOW(sess->dialog), sess->window_title);

		sess->status_label = gtk_label_new(_("Status: detecting gpsbabel"));
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(sess->dialog))), sess->status_label, false, false, 5);
		gtk_widget_show_all(sess->status_label);

		sess->gps_label = gtk_label_new(_("GPS device: N/A"));
		sess->ver_label = gtk_label_new("");
		sess->id_label = gtk_label_new("");
		sess->wp_label = gtk_label_new("");
		sess->trk_label = gtk_label_new("");
		sess->rte_label = gtk_label_new("");

		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(sess->dialog))), sess->gps_label, false, false, 5);
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(sess->dialog))), sess->wp_label, false, false, 5);
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(sess->dialog))), sess->trk_label, false, false, 5);
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(sess->dialog))), sess->rte_label, false, false, 5);

		gtk_widget_show_all(sess->dialog);

		sess->progress_label = sess->wp_label;
		sess->total_count = -1;

		// Starting gps read/write thread
#if GLIB_CHECK_VERSION (2, 32, 0)
		g_thread_try_new("gps_comm_thread", (GThreadFunc)gps_comm_thread, sess, NULL);
#else
		g_thread_create((GThreadFunc)gps_comm_thread, sess, false, NULL);
#endif

		gtk_dialog_set_default_response(GTK_DIALOG(sess->dialog), GTK_RESPONSE_ACCEPT);
		gtk_dialog_run(GTK_DIALOG(sess->dialog));

		gtk_widget_destroy(sess->dialog);
	} else {
		if (!turn_off) {
			a_dialog_info_msg(VIK_GTK_WINDOW_FROM_LAYER(vtl), _("No GPS items selected for transfer."));
		}
	}

	g_mutex_lock(sess->mutex);

	if (sess->ok) {
		sess->ok = false;   /* tell thread to stop */
		g_mutex_unlock(sess->mutex);
	} else {
		if (turn_off) {
			// No need for thread for powering off device (should be quick operation...) - so use babel command directly:
			char *device_off = g_strdup_printf("-i %s,%s", protocol, "power_off");
			ProcessOptions po = { device_off, port, NULL, NULL, NULL, NULL };
			bool result = a_babel_convert_from(NULL, &po, NULL, NULL, NULL);
			if (!result) {
				a_dialog_error_msg(VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Could not turn off device."));
			}
			free(device_off);
		}
		g_mutex_unlock(sess->mutex);
		gps_session_delete(sess);
	}

	return 0;
}

static void gps_upload_cb(gps_layer_data_t * data)
{
	LayersPanel * panel = data->panel;
	LayerGPS * layer = data->layer;

	VikWindow * vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(layer->vl));
	Viewport * viewport = vik_window_viewport(vw);
	VikTrwLayer * vtl = (VikTrwLayer *) layer->trw_children[TRW_UPLOAD]->vl;

	vik_gps_comm(vtl,
		     NULL,
		     GPS_UP,
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

	VikWindow * vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(layer->vl));
	Viewport * viewport = vik_window_viewport(vw);
	VikTrwLayer * vtl = (VikTrwLayer *) layer->trw_children[TRW_DOWNLOAD]->vl;

#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	vik_gps_comm(vtl,
		     NULL,
		     GPS_DOWN,
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
	vik_gps_comm(vtl,
		     NULL,
		     GPS_DOWN,
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

	// Get confirmation from the user
	if (! a_dialog_yes_or_no(VIK_GTK_WINDOW_FROM_WIDGET(panel->gob),
				 _("Are you sure you want to delete GPS Upload data?"),
				 NULL)) {
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

	// Get confirmation from the user
	if (! a_dialog_yes_or_no(VIK_GTK_WINDOW_FROM_WIDGET(panel->gob),
				    _("Are you sure you want to delete GPS Download data?"),
				    NULL)) {
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

	// Get confirmation from the user
	if (! a_dialog_yes_or_no(VIK_GTK_WINDOW_FROM_WIDGET(panel->gob),
				    _("Are you sure you want to delete GPS Realtime data?"),
				    NULL)) {
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

	// Get confirmation from the user
	if (! a_dialog_yes_or_no(VIK_GTK_WINDOW_FROM_WIDGET(panel->gob),
				    _("Are you sure you want to delete All GPS data?"),
				    NULL)) {
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
	struct LatLon ll;
	VikCoord nw, se;
	struct LatLon lnw, lse;
	viewport->screen_to_coord(-20, -20, &nw);
	viewport->screen_to_coord(viewport->get_width() + 20, viewport->get_width() + 20, &se);
	vik_coord_to_latlon(&nw, &lnw);
	vik_coord_to_latlon(&se, &lse);
	if (this->realtime_fix.fix.latitude > lse.lat &&
	     this->realtime_fix.fix.latitude < lnw.lat &&
	     this->realtime_fix.fix.longitude > lnw.lon &&
	     this->realtime_fix.fix.longitude < lse.lon &&
	     !isnan(this->realtime_fix.fix.track)) {
		VikCoord gps;
		int x, y;
		int half_back_x, half_back_y;
		int half_back_bg_x, half_back_bg_y;
		int pt_x, pt_y;
		int ptbg_x;
		int side1_x, side1_y, side2_x, side2_y;
		int side1bg_x, side1bg_y, side2bg_x, side2bg_y;

		ll.lat = this->realtime_fix.fix.latitude;
		ll.lon = this->realtime_fix.fix.longitude;
		vik_coord_load_from_latlon(&gps, viewport->get_coord_mode(), &ll);
		viewport->coord_to_screen(&gps, &x, &y);

		double heading_cos = cos(DEG2RAD(this->realtime_fix.fix.track));
		double heading_sin = sin(DEG2RAD(this->realtime_fix.fix.track));

		half_back_y = y+8*heading_cos;
		half_back_x = x-8*heading_sin;
		half_back_bg_y = y+10*heading_cos;
		half_back_bg_x = x-10*heading_sin;

		pt_y = half_back_y-24*heading_cos;
		pt_x = half_back_x+24*heading_sin;
		//ptbg_y = half_back_bg_y-28*heading_cos;
		ptbg_x = half_back_bg_x+28*heading_sin;

		side1_y = half_back_y+9*heading_sin;
		side1_x = half_back_x+9*heading_cos;
		side1bg_y = half_back_bg_y+11*heading_sin;
		side1bg_x = half_back_bg_x+11*heading_cos;

		side2_y = half_back_y-9*heading_sin;
		side2_x = half_back_x-9*heading_cos;
		side2bg_y = half_back_bg_y-11*heading_sin;
		side2bg_x = half_back_bg_x-11*heading_cos;

		GdkPoint trian[3] = { { pt_x, pt_y }, {side1_x, side1_y}, {side2_x, side2_y} };
		GdkPoint trian_bg[3] = { { ptbg_x, pt_y }, {side1bg_x, side1bg_y}, {side2bg_x, side2bg_y} };

		viewport->draw_polygon(this->realtime_track_bg_gc, true, trian_bg, 3);
		viewport->draw_polygon(this->realtime_track_gc, true, trian, 3);
		viewport->draw_rectangle((this->realtime_fix.fix.mode > MODE_2D) ? this->realtime_track_pt2_gc : this->realtime_track_pt1_gc,
					true, x-2, y-2, 4, 4);
		//this->realtime_track_pt_gc = (this->realtime_track_pt_gc == this->realtime_track_pt1_gc) ? this->realtime_track_pt2_gc : this->realtime_track_pt1_gc;
	}
}

Trackpoint * LayerGPS::create_realtime_trackpoint(bool forced)
{
	struct LatLon ll;
	GList *last_tp;

	/* Note that fix.time is a double, but it should not affect the precision
	   for most GPS */
	time_t cur_timestamp = this->realtime_fix.fix.time;
	time_t last_timestamp = this->last_fix.fix.time;

	if (cur_timestamp < last_timestamp) {
		return NULL;
	}

	if (this->realtime_record && this->realtime_fix.dirty) {
		bool replace = false;
		int heading = isnan(this->realtime_fix.fix.track) ? 0 : (int)floor(this->realtime_fix.fix.track);
		int last_heading = isnan(this->last_fix.fix.track) ? 0 : (int)floor(this->last_fix.fix.track);
		int alt = isnan(this->realtime_fix.fix.altitude) ? VIK_DEFAULT_ALTITUDE : floor(this->realtime_fix.fix.altitude);
		int last_alt = isnan(this->last_fix.fix.altitude) ? VIK_DEFAULT_ALTITUDE : floor(this->last_fix.fix.altitude);
		if (((last_tp = g_list_last(this->realtime_track->trackpoints)) != NULL) &&
		    (this->realtime_fix.fix.mode > MODE_2D) &&
		    (this->last_fix.fix.mode <= MODE_2D) &&
		    ((cur_timestamp - last_timestamp) < 2)) {
			free(last_tp->data);
			this->realtime_track->trackpoints = g_list_delete_link(this->realtime_track->trackpoints, last_tp);
			replace = true;
		}
		if (replace ||
		    ((cur_timestamp != last_timestamp) &&
		     ((forced ||
		       ((heading < last_heading) && (heading < (last_heading - 3))) ||
		       ((heading > last_heading) && (heading > (last_heading + 3))) ||
		       ((alt != VIK_DEFAULT_ALTITUDE) && (alt != last_alt)))))) {
			/* TODO: check for new segments */
			Trackpoint * tp = new Trackpoint();
			tp->newsegment = false;
			tp->has_timestamp = true;
			tp->timestamp = this->realtime_fix.fix.time;
			tp->altitude = alt;
			/* speed only available for 3D fix. Check for NAN when use this speed */
			tp->speed = this->realtime_fix.fix.speed;
			tp->course = this->realtime_fix.fix.track;
			tp->nsats = this->realtime_fix.satellites_used;
			tp->fix_mode = (FixMode) this->realtime_fix.fix.mode;

			ll.lat = this->realtime_fix.fix.latitude;
			ll.lon = this->realtime_fix.fix.longitude;
			vik_coord_load_from_latlon(&tp->coord,
						   this->trw_children[TRW_REALTIME]->get_coord_mode(), &ll);

			this->realtime_track->add_trackpoint(tp, true); // Ensure bounds is recalculated
			this->realtime_fix.dirty = false;
			this->realtime_fix.satellites_used = 0;
			this->last_fix = this->realtime_fix;
			return tp;
		}
	}
	return NULL;
}

#define VIK_SETTINGS_GPS_STATUSBAR_FORMAT "gps_statusbar_format"

void LayerGPS::update_statusbar(VikWindow * vw)
{
	char *statusbar_format_code = NULL;
	bool need2free = false;
	if (!a_settings_get_string(VIK_SETTINGS_GPS_STATUSBAR_FORMAT, &statusbar_format_code)) {
		// Otherwise use default
		statusbar_format_code = g_strdup("GSA");
		need2free = true;
	}

	char *msg = vu_trackpoint_formatted_message(statusbar_format_code, this->tp, this->tp_prev, this->realtime_track, this->last_fix.fix.climb);
	vik_statusbar_set_message(vik_window_get_statusbar(vw), VIK_STATUSBAR_INFO, msg);
	free(msg);

	if (need2free) {
		free(statusbar_format_code);
	}

}

static void gpsd_raw_hook(VglGpsd *vgpsd, char *data)
{
	bool update_all = false;
	VikGpsLayer *vgl = vgpsd->vgl;
	LayerGPS * layer = (LayerGPS *) ((VikLayer *) vgl)->layer;

	if (!layer->realtime_tracking) {
		fprintf(stderr, "WARNING: %s: receiving GPS data while not in realtime mode\n", __PRETTY_FUNCTION__);
		return;
	}

	if ((vgpsd->gpsd.fix.mode >= MODE_2D) &&
	    !isnan(vgpsd->gpsd.fix.latitude) &&
	    !isnan(vgpsd->gpsd.fix.longitude)) {

		VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vgl));
		Viewport * viewport = vik_window_viewport(vw);
		layer->realtime_fix.fix = vgpsd->gpsd.fix;
		layer->realtime_fix.satellites_used = vgpsd->gpsd.satellites_used;
		layer->realtime_fix.dirty = true;

		struct LatLon ll;
		VikCoord vehicle_coord;

		ll.lat = layer->realtime_fix.fix.latitude;
		ll.lon = layer->realtime_fix.fix.longitude;
		vik_coord_load_from_latlon(&vehicle_coord,
					   layer->trw_children[TRW_REALTIME]->get_coord_mode(), &ll);

		if ((layer->vehicle_position == VEHICLE_POSITION_CENTERED) ||
		    (layer->realtime_jump_to_start && layer->first_realtime_trackpoint)) {
			viewport->set_center_coord(&vehicle_coord, false);
			update_all = true;
		} else if (layer->vehicle_position == VEHICLE_POSITION_ON_SCREEN) {
			const int hdiv = 6;
			const int vdiv = 6;
			const int px = 20; /* adjust ment in pixels to make sure vehicle is inside the box */
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
				layer->update_statusbar(vw);
			}
			layer->tp_prev = layer->tp;
		}

		vik_layer_emit_update(update_all ? VIK_LAYER(vgl) : layer->trw_children[TRW_REALTIME]->vl); // NB update from background thread
	}
}

static int gpsd_data_available(GIOChannel *source, GIOCondition condition, void * data)
{
	VikGpsLayer *vgl = (VikGpsLayer *) data;
	LayerGPS * layer = (LayerGPS *) ((VikLayer *) vgl)->layer;

	if (condition == G_IO_IN) {
#if GPSD_API_MAJOR_VERSION == 3 || GPSD_API_MAJOR_VERSION == 4
		if (!gps_poll(&layer->vgpsd->gpsd)) {
#elif GPSD_API_MAJOR_VERSION == 5 || GPSD_API_MAJOR_VERSION == 6
		if (gps_read(&layer->vgpsd->gpsd) > -1) {
			// Reuse old function to perform operations on the new GPS data
			gpsd_raw_hook(layer->vgpsd, NULL);
#else
		// Broken compile
#endif
			return true;
		} else {
			fprintf(stderr, "WARNING: Disconnected from gpsd. Trying to reconnect\n");
			layer->rt_gpsd_disconnect();
			layer->rt_gpsd_connect(false);
		}
	}
	return false; /* no further calling */
}

static char *make_track_name(VikTrwLayer *vtl)
{
	const char basename[] = "REALTIME";
	const int bufsize = sizeof(basename) + 5;
	char *name = (char *) malloc(bufsize);
	strcpy(name, basename);
	int i = 2;

	while (vtl->trw->get_track(name) != NULL) {
		snprintf(name, bufsize, "%s#%d", basename, i);
		i++;
	}
	return(name);

}

static bool rt_gpsd_try_connect(void * *data)
{
	VikGpsLayer *vgl = (VikGpsLayer *)data;
	LayerGPS * layer = (LayerGPS *) ((VikLayer *) vgl)->layer;

#if GPSD_API_MAJOR_VERSION == 3
	struct gps_data_t *gpsd = gps_open(layer->gpsd_host, layer->gpsd_port);

	if (gpsd == NULL) {
#elif GPSD_API_MAJOR_VERSION == 4
	layer->vgpsd = malloc(sizeof(VglGpsd));

	if (gps_open_r(layer->gpsd_host, layer->gpsd_port, /*(struct gps_data_t *)*/layer->vgpsd) != 0) {
#elif GPSD_API_MAJOR_VERSION == 5 || GPSD_API_MAJOR_VERSION == 6
	layer->vgpsd = (VglGpsd *) malloc(sizeof(VglGpsd));
	if (gps_open(layer->gpsd_host, layer->gpsd_port, &layer->vgpsd->gpsd) != 0) {
#else
	// Delibrately break compilation...
#endif
		fprintf(stderr, "WARNING: Failed to connect to gpsd at %s (port %s). Will retry in %d seconds\n",
			layer->gpsd_host, layer->gpsd_port, layer->gpsd_retry_interval);
		return true;   /* keep timer running */
	}

#if GPSD_API_MAJOR_VERSION == 3
	layer->vgpsd = realloc(gpsd, sizeof(VglGpsd));
#endif
	layer->vgpsd->vgl = vgl;

	layer->realtime_fix.dirty = layer->last_fix.dirty = false;
  /* track alt/time graph uses VIK_DEFAULT_ALTITUDE (0.0) as invalid */
	layer->realtime_fix.fix.altitude = layer->last_fix.fix.altitude = VIK_DEFAULT_ALTITUDE;
	layer->realtime_fix.fix.speed = layer->last_fix.fix.speed = NAN;

	if (layer->realtime_record) {
		LayerTRW * trw = layer->trw_children[TRW_REALTIME];
		layer->realtime_track = new Track();
		layer->realtime_track->visible = true;
		trw->add_track(layer->realtime_track, make_track_name((VikTrwLayer *) trw->vl));
	}

#if GPSD_API_MAJOR_VERSION == 3 || GPSD_API_MAJOR_VERSION == 4
	gps_set_raw_hook(&layer->vgpsd->gpsd, gpsd_raw_hook);
#endif

	layer->realtime_io_channel = g_io_channel_unix_new(layer->vgpsd->gpsd.gps_fd);
	layer->realtime_io_watch_id = g_io_add_watch(layer->realtime_io_channel,
						    (GIOCondition) (G_IO_IN | G_IO_ERR | G_IO_HUP), gpsd_data_available, vgl);

#if GPSD_API_MAJOR_VERSION == 3
	gps_query(&layer->vgpsd->gpsd, "w+x");
#endif
#if GPSD_API_MAJOR_VERSION == 4 || GPSD_API_MAJOR_VERSION == 5 || GPSD_API_MAJOR_VERSION == 6
	gps_stream(&layer->vgpsd->gpsd, WATCH_ENABLE, NULL);
#endif

	return false;  /* no longer called by timeout */
}

bool LayerGPS::rt_ask_retry()
{
	GtkWidget * dialog = gtk_message_dialog_new(VIK_GTK_WINDOW_FROM_LAYER(this->vl),
						    GTK_DIALOG_DESTROY_WITH_PARENT,
						    GTK_MESSAGE_QUESTION,
						    GTK_BUTTONS_YES_NO,
						    "Failed to connect to gpsd at %s (port %s)\n"
						    "Should Viking keep trying (every %d seconds)?",
						    this->gpsd_host, this->gpsd_port,
						    this->gpsd_retry_interval);

	int res = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	return (res == GTK_RESPONSE_YES);
}

bool LayerGPS::rt_gpsd_connect(bool ask_if_failed)
{
	this->realtime_retry_timer = 0;
	if (rt_gpsd_try_connect((void * *)this->vl)) {
		if (this->gpsd_retry_interval <= 0) {
			fprintf(stderr, "WARNING: Failed to connect to gpsd but will not retry because retry intervel was set to %d (which is 0 or negative)\n", this->gpsd_retry_interval);
			return false;
		} else if (ask_if_failed && !this->rt_ask_retry()) {
			return false;
		} else {
			this->realtime_retry_timer = g_timeout_add_seconds(this->gpsd_retry_interval,
									   (GSourceFunc)rt_gpsd_try_connect, (void **) this->vl);
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
		if ((this->realtime_track->trackpoints == NULL) || (this->realtime_track->trackpoints->next == NULL)) {
			this->trw_children[TRW_REALTIME]->delete_track(this->realtime_track);
		}
		this->realtime_track = NULL;
	}
}

static void gps_start_stop_tracking_cb(gps_layer_data_t * data)
{
	LayerGPS * layer = data->layer;

	layer->realtime_tracking = (layer->realtime_tracking == false);

	/* Make sure we are still in the boat with libgps */
	assert ((VIK_GPS_MODE_2D == MODE_2D) && (VIK_GPS_MODE_3D == MODE_3D));

	if (layer->realtime_tracking) {
		layer->first_realtime_trackpoint = true;
		if (!layer->rt_gpsd_connect(true)) {
			layer->first_realtime_trackpoint = false;
			layer->realtime_tracking = false;
			layer->tp = NULL;
		}
	} else {  /* stop realtime tracking */
		layer->first_realtime_trackpoint = false;
		layer->tp = NULL;
		layer->rt_gpsd_disconnect();
	}
}
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */





LayerGPS::LayerGPS()
{
	this->type = VIK_LAYER_GPS;
	strcpy(this->type_string, "GPS");
}





LayerGPS::LayerGPS(VikLayer * vl) : Layer(vl)
{
	this->type = VIK_LAYER_GPS;

	strcpy(this->type_string, "GPS");
};





LayerGPS::LayerGPS(Viewport * viewport) : LayerGPS()
{

	/* vik_gps_layer_new() */

	VikGpsLayer * vgl = VIK_GPS_LAYER (g_object_new(VIK_GPS_LAYER_TYPE, NULL));
	((VikLayer *) vgl)->layer = this;
	this->vl = (VikLayer *) vgl;

	for (int i = 0; i < NUM_TRW; i++) {
		this->trw_children[i] = NULL;
	}
	this->children = NULL;
	this->cur_read_child = 0;

#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
	this->vgpsd = NULL;
	this->first_realtime_trackpoint = false;
	this->realtime_track = NULL;

	this->realtime_io_channel = NULL;
	this->realtime_io_watch_id = 0;
	this->realtime_retry_timer = 0;
	if (viewport) {
		this->realtime_track_gc = viewport->new_gc("#203070", 2);
		this->realtime_track_bg_gc = viewport->new_gc("grey", 2);
		this->realtime_track_pt1_gc = viewport->new_gc("red", 2);
		this->realtime_track_pt2_gc = viewport->new_gc("green", 2);
		this->realtime_track_pt_gc = this->realtime_track_pt1_gc;
	}

	this->gpsd_host = strdup("host");
	this->gpsd_port = strdup("port");

	this->tp = NULL;
	this->tp_prev = NULL;

#endif // VIK_CONFIG_REALTIME_GPS_TRACKING

	this->protocol = NULL;
	this->serial_port = NULL;

	vik_layer_set_defaults(this->vl, viewport);




	/* vik_gps_layer_create() */

	this->rename(vik_gps_layer_interface.name);

	for (int i = 0; i < NUM_TRW; i++) {
		this->trw_children[i] = new LayerTRW(viewport);
		vik_layer_set_menu_items_selection(this->trw_children[i]->vl, VIK_MENU_ITEM_ALL & ~(VIK_MENU_ITEM_CUT|VIK_MENU_ITEM_DELETE));
	}
};
