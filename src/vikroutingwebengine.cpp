/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (c) 2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

/**
 * SECTION:vikroutingwebengine
 * @short_description: A generic class for WEB based routing engine
 *
 * The #VikRoutingWebEngine class handles WEB based
 * routing engine.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>
#include <cstdlib>

#include <glib.h>
#include <glib/gstdio.h>

#include "babel.h"

#include "vikroutingwebengine.h"




using namespace SlavGPS;



static void vik_routing_web_engine_finalize(GObject * gob);

static bool vik_routing_web_engine_find(VikRoutingEngine * self, VikLayer * vtl, struct LatLon start, struct LatLon end);
static char *vik_routing_web_engine_get_url_from_directions(VikRoutingEngine * self, const char * start, const char * end);
static bool vik_routing_web_engine_supports_direction(VikRoutingEngine * self);
static bool vik_routing_web_engine_refine(VikRoutingEngine * self, VikLayer *vtl, Track * trk);
static bool vik_routing_web_engine_supports_refine(VikRoutingEngine * self);




typedef struct _VikRoutingWebEnginePrivate VikRoutingWebEnginePrivate;
struct _VikRoutingWebEnginePrivate
{
	char * url_base;

	/* LatLon. */
	char * url_start_ll_fmt;
	char * url_stop_ll_fmt;
	char * url_via_ll_fmt;

	/* Directions. */
	char * url_start_dir_fmt;
	char * url_stop_dir_fmt;

	DownloadFileOptions options;
};

#define VIK_ROUTING_WEB_ENGINE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), VIK_ROUTING_WEB_ENGINE_TYPE, VikRoutingWebEnginePrivate))

/* Properties. */
enum {
	PROP_0,

	PROP_URL_BASE,

	/* LatLon. */
	PROP_URL_START_LL,
	PROP_URL_STOP_LL,
	PROP_URL_VIA_LL,

	/* Direction. */
	PROP_URL_START_DIR,
	PROP_URL_STOP_DIR,

	PROP_REFERER,
	PROP_FOLLOW_LOCATION,
};

G_DEFINE_TYPE (VikRoutingWebEngine, vik_routing_web_engine, VIK_ROUTING_ENGINE_TYPE)

static void vik_routing_web_engine_set_property(GObject * object,
						unsigned int property_id,
						const GValue * value,
						GParamSpec   * pspec)
{
	VikRoutingWebEnginePrivate * priv = VIK_ROUTING_WEB_ENGINE_PRIVATE (object);

	switch (property_id) {
	case PROP_URL_BASE:
		free(priv->url_base);
		priv->url_base = g_strdup(g_value_get_string (value));
		break;

	case PROP_URL_START_LL:
		free(priv->url_start_ll_fmt);
		priv->url_start_ll_fmt = g_strdup(g_value_get_string (value));
		break;

	case PROP_URL_STOP_LL:
		free(priv->url_stop_ll_fmt);
		priv->url_stop_ll_fmt = g_strdup(g_value_get_string (value));
		break;

	case PROP_URL_VIA_LL:
		free(priv->url_via_ll_fmt);
		priv->url_via_ll_fmt = g_strdup(g_value_get_string (value));
		break;

	case PROP_URL_START_DIR:
		free(priv->url_start_dir_fmt);
		priv->url_start_dir_fmt = g_strdup(g_value_get_string (value));
		break;

	case PROP_URL_STOP_DIR:
		free(priv->url_stop_dir_fmt);
		priv->url_stop_dir_fmt = g_strdup(g_value_get_string (value));
		break;

	case PROP_REFERER:
		free(priv->options.referer);
		priv->options.referer = g_value_dup_string (value);
		break;

	case PROP_FOLLOW_LOCATION:
		priv->options.follow_location = g_value_get_long (value);
		break;

	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}




static void vik_routing_web_engine_get_property(GObject    * object,
						unsigned int property_id,
						GValue     * value,
						GParamSpec * pspec)
{
	VikRoutingWebEnginePrivate * priv = VIK_ROUTING_WEB_ENGINE_PRIVATE (object);

	switch (property_id) {
	case PROP_URL_BASE:
		g_value_set_string(value, priv->url_base);
		break;

	case PROP_URL_START_LL:
		g_value_set_string(value, priv->url_start_ll_fmt);
		break;

	case PROP_URL_STOP_LL:
		g_value_set_string(value, priv->url_stop_ll_fmt);
		break;

	case PROP_URL_VIA_LL:
		g_value_set_string(value, priv->url_via_ll_fmt);
		break;

	case PROP_URL_START_DIR:
		g_value_set_string(value, priv->url_start_dir_fmt);
		break;

	case PROP_URL_STOP_DIR:
		g_value_set_string(value, priv->url_stop_dir_fmt);
		break;

	case PROP_REFERER:
		g_value_set_string(value, priv->options.referer);
		break;

	case PROP_FOLLOW_LOCATION:
		g_value_set_long(value, priv->options.follow_location);
		break;

	default:
		/* We don't have any other property... */
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}




static void vik_routing_web_engine_class_init(VikRoutingWebEngineClass * klass)
{
	GObjectClass * object_class;
	VikRoutingEngineClass * parent_class;
	GParamSpec * pspec = NULL;

	object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = vik_routing_web_engine_set_property;
	object_class->get_property = vik_routing_web_engine_get_property;
	object_class->finalize = vik_routing_web_engine_finalize;

	parent_class = VIK_ROUTING_ENGINE_CLASS (klass);

	parent_class->find = vik_routing_web_engine_find;
	parent_class->supports_direction = vik_routing_web_engine_supports_direction;
	parent_class->get_url_from_directions = vik_routing_web_engine_get_url_from_directions;
	parent_class->refine = vik_routing_web_engine_refine;
	parent_class->supports_refine = vik_routing_web_engine_supports_refine;

	/**
	 * VikRoutingWebEngine:url-base:
	 *
	 * The base URL of the routing engine.
	 */
	pspec = g_param_spec_string("url-base",
				    "URL's base",
				    "The base URL of the routing engine",
				    "<no-set>", /* Default value. */
				    (GParamFlags) (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_URL_BASE, pspec);


	/**
	 * VikRoutingWebEngine:url-start-ll:
	 *
	 * The part of the request hosting the end point.
	 */
	pspec = g_param_spec_string("url-start-ll",
				     "Start part of the URL",
				    "The part of the request hosting the start point",
				    "<no-set>", /* default value */
				    (GParamFlags) (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_URL_START_LL, pspec);


	/**
	 * VikRoutingWebEngine:url-stop-ll:
	 *
	 * The part of the request hosting the end point.
	 */
	pspec = g_param_spec_string("url-stop-ll",
				    "Stop part of the URL",
				    "The part of the request hosting the end point",
				    "<no-set>", /* Default value */
				    (GParamFlags) (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_URL_STOP_LL, pspec);


	/**
	 * VikRoutingWebEngine:url-via-ll:
	 *
	 * The param of the request for setting a via point.
	 */
	pspec = g_param_spec_string("url-via-ll",
				    "Via part of the URL",
				    "The param of the request for setting a via point",
				    NULL, /* Default value. */
				    (GParamFlags) (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_URL_VIA_LL, pspec);


	/**
	 * VikRoutingWebEngine:url-start-dir:
	 *
	 * The part of the request hosting the end point.
	 */
	pspec = g_param_spec_string("url-start-dir",
				    "Start part of the URL",
				    "The part of the request hosting the start point",
				    NULL, /* Default value. */
				    (GParamFlags) (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_URL_START_DIR, pspec);


	/**
	 * VikRoutingWebEngine:url-stop-dir:
	 *
	 * The part of the request hosting the end point.
	 */
	pspec = g_param_spec_string("url-stop-dir",
				    "Stop part of the URL",
				    "The part of the request hosting the end point",
				    NULL, /* Default value. */
				    (GParamFlags) (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_URL_STOP_DIR, pspec);


	/**
	 * VikRoutingWebEngine:referer:
	 *
	 * The REFERER string to use in HTTP request.
	 */
	pspec = g_param_spec_string("referer",
				    "Referer",
				    "The REFERER string to use in HTTP request",
				    NULL, /* Default value. */
				    (GParamFlags) (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_REFERER, pspec);


	/**
	 * VikRoutingWebEngine:follow-location:
	 *
	 * Specifies the number of retries to follow a redirect while downloading a page.
	 */
	pspec = g_param_spec_long("follow-location",
				  "Follow location",
				  "Specifies the number of retries to follow a redirect while downloading a page",
				  0,  /* Minimum value. */
				  G_MAXLONG, /* Maximum value. */
				  2,  /* Default value. */
				  (GParamFlags) (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_FOLLOW_LOCATION, pspec);

	g_type_class_add_private(klass, sizeof (VikRoutingWebEnginePrivate));
}




static void vik_routing_web_engine_init(VikRoutingWebEngine * self)
{
	VikRoutingWebEnginePrivate * priv = VIK_ROUTING_WEB_ENGINE_PRIVATE (self);

	priv->url_base = NULL;

	/* LatLon. */
	priv->url_start_ll_fmt = NULL;
	priv->url_stop_ll_fmt = NULL;
	priv->url_via_ll_fmt = NULL;

	/* Directions. */
	priv->url_start_dir_fmt = NULL;
	priv->url_stop_dir_fmt = NULL;

	priv->options.referer = NULL;
	priv->options.follow_location = 0;
	priv->options.check_file = NULL;
	priv->options.check_file_server_time = false;
	priv->options.use_etag = false;
}




static void vik_routing_web_engine_finalize(GObject * gob)
{
	VikRoutingWebEnginePrivate * priv = VIK_ROUTING_WEB_ENGINE_PRIVATE (gob);

	free(priv->url_base);
	priv->url_base = NULL;

	/* LatLon. */
	free(priv->url_start_ll_fmt);
	priv->url_start_ll_fmt = NULL;
	free(priv->url_stop_ll_fmt);
	priv->url_stop_ll_fmt = NULL;
	free(priv->url_via_ll_fmt);
	priv->url_via_ll_fmt = NULL;

	/* Directions. */
	free(priv->url_start_dir_fmt);
	priv->url_start_dir_fmt = NULL;
	free(priv->url_stop_dir_fmt);
	priv->url_stop_dir_fmt = NULL;

	free(priv->options.referer);
	priv->options.referer = NULL;

	G_OBJECT_CLASS (vik_routing_web_engine_parent_class)->finalize(gob);
}




static DownloadFileOptions * vik_routing_web_engine_get_download_options(VikRoutingEngine * self)
{
	if (!VIK_IS_ROUTING_WEB_ENGINE (self)) {
		return NULL;
	}

	VikRoutingWebEnginePrivate * priv = VIK_ROUTING_WEB_ENGINE_PRIVATE(self);

	return &(priv->options);
}




static char * substitute_latlon(const char * fmt, struct LatLon ll)
{
	char lat[G_ASCII_DTOSTR_BUF_SIZE], lon[G_ASCII_DTOSTR_BUF_SIZE];
	char * substituted = g_strdup_printf(fmt,
					     g_ascii_dtostr(lat, G_ASCII_DTOSTR_BUF_SIZE, (double) ll.lat),
					     g_ascii_dtostr(lon, G_ASCII_DTOSTR_BUF_SIZE, (double) ll.lon));
	return substituted;
}




static char * vik_routing_web_engine_get_url_for_coords(VikRoutingEngine * self, struct LatLon start, struct LatLon end)
{
	if (!VIK_IS_ROUTING_WEB_ENGINE (self)) {
		return NULL;
	}

	VikRoutingWebEnginePrivate * priv = VIK_ROUTING_WEB_ENGINE_PRIVATE (self);

	if (!priv->url_base || !priv->url_start_ll_fmt || !priv->url_stop_ll_fmt) {
		return NULL;
	}

	char * startURL = substitute_latlon(priv->url_start_ll_fmt, start);
	char * endURL = substitute_latlon(priv->url_stop_ll_fmt, end);
	char * url = g_strconcat(priv->url_base, startURL, endURL, NULL);

	/* Free memory. */
	free(startURL);
	free(endURL);

	return url;
}




static bool vik_routing_web_engine_find(VikRoutingEngine * self, VikLayer * vtl, struct LatLon start, struct LatLon end)
{
	char * uri = vik_routing_web_engine_get_url_for_coords(self, start, end);

	DownloadFileOptions *options = vik_routing_web_engine_get_download_options(self);

	char * format = vik_routing_engine_get_format (self);
	ProcessOptions po = { NULL, NULL, format, uri, NULL, NULL };
	bool ret = a_babel_convert_from((LayerTRW *) vtl->layer, &po, NULL, NULL, options);

	free(uri);

	return ret;
}




static char * vik_routing_web_engine_get_url_from_directions(VikRoutingEngine * self, const char * start, const char * end)
{
	if (!VIK_IS_ROUTING_WEB_ENGINE (self)) {
		return NULL;
	}

	VikRoutingWebEnginePrivate * priv = VIK_ROUTING_WEB_ENGINE_PRIVATE (self);

	if (!priv->url_base || !priv->url_start_dir_fmt || !priv->url_stop_dir_fmt) {
		return NULL;
	}

	char *from_quoted, *to_quoted;
	char **from_split, **to_split;
	from_quoted = g_shell_quote(start);
	to_quoted = g_shell_quote(end);

	from_split = g_strsplit(from_quoted, " ", 0);
	to_split = g_strsplit(to_quoted, " ", 0);

	from_quoted = g_strjoinv("%20", from_split);
	to_quoted = g_strjoinv("%20", to_split);

	char * url_fmt = g_strconcat(priv->url_base, priv->url_start_dir_fmt, priv->url_stop_dir_fmt, NULL);
	char * url = g_strdup_printf(url_fmt, from_quoted, to_quoted);

	free(url_fmt);

	free(from_quoted);
	free(to_quoted);
	g_strfreev(from_split);
	g_strfreev(to_split);

	return url;
}




static bool vik_routing_web_engine_supports_direction(VikRoutingEngine * self)
{
	if (!VIK_IS_ROUTING_WEB_ENGINE (self)) {
		return false;
	}

	VikRoutingWebEnginePrivate * priv = VIK_ROUTING_WEB_ENGINE_PRIVATE (self);

	return (priv->url_start_dir_fmt) != NULL;
}




struct _append_ctx {
	VikRoutingWebEnginePrivate * priv;
	char ** urlParts;
	int nb;
};




static void _append_stringified_coords(void * data, void * user_data)
{
	Trackpoint * tp = (Trackpoint *) data;
	struct _append_ctx *ctx = (struct _append_ctx*)user_data;

	/* Stringify coordinate. */
	struct LatLon position;
	vik_coord_to_latlon(&(tp->coord), &position);
	char * string = substitute_latlon(ctx->priv->url_via_ll_fmt, position);

	/* Append. */
	ctx->urlParts[ctx->nb] = string;
	ctx->nb++;
}




static char * vik_routing_web_engine_get_url_for_track(VikRoutingEngine * self, Track * trk)
{
	char * *urlParts;
	char * url;

	VikRoutingWebEnginePrivate * priv = VIK_ROUTING_WEB_ENGINE_PRIVATE (self);

	if (!priv->url_base || !priv->url_start_ll_fmt || !priv->url_stop_ll_fmt || !priv->url_via_ll_fmt) {
		return NULL;
	}

	/* Init temporary storage. */
	size_t len = 1 + trk->trackpointsB->size() + 1; /* Base + trackpoints + NULL. */
	urlParts = (char **) malloc(sizeof(char*)*len);
	urlParts[0] = g_strdup(priv->url_base);
	urlParts[len-1] = NULL;

	struct _append_ctx ctx;
	ctx.priv = priv;
	ctx.urlParts = urlParts;
	ctx.nb = 1; /* First cell available, previous used for base URL. */

	/* Append all trackpoints to URL. */
	for (auto iter = trk->trackpointsB->begin(); iter != trk->trackpointsB->end(); iter++) {
		_append_stringified_coords(*iter, &ctx);
	}

	/* Override first and last positions with associated formats. */
	struct LatLon position;
	free(urlParts[1]);

	Trackpoint * tp = *trk->trackpointsB->begin();
	vik_coord_to_latlon(&tp->coord, &position);
	urlParts[1] = substitute_latlon(priv->url_start_ll_fmt, position);

	free(urlParts[len-2]);

	tp = *std::prev(trk->trackpointsB->end());
	vik_coord_to_latlon(&tp->coord, &position);
	urlParts[len-2] = substitute_latlon(priv->url_stop_ll_fmt, position);

	/* Concat. */
	url = g_strjoinv(NULL, urlParts);
	fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, url);

	/* Free. */
	g_strfreev(urlParts);

	return url;
}




static bool vik_routing_web_engine_refine(VikRoutingEngine * self, VikLayer * vtl, Track * trk)
{
	/* Compute URL. */
	char * uri = vik_routing_web_engine_get_url_for_track(self, trk);

	/* Download data. */
	DownloadFileOptions *options = vik_routing_web_engine_get_download_options(self);

	/* Convert and insert data in model. */
	char * format = vik_routing_engine_get_format(self);
	ProcessOptions po = { NULL, NULL, format, uri, NULL, NULL };
	bool ret = a_babel_convert_from((LayerTRW *) vtl->layer, &po, NULL, NULL, options);

	free(uri);

	return ret;
}




static bool vik_routing_web_engine_supports_refine(VikRoutingEngine * self)
{
	if (!VIK_IS_ROUTING_WEB_ENGINE (self)) {
		return false;
	}

	VikRoutingWebEnginePrivate * priv = VIK_ROUTING_WEB_ENGINE_PRIVATE (self);

	return priv->url_via_ll_fmt != NULL;
}
