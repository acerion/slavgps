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
 * The #RoutingEngineWeb class handles WEB based
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
#include "routing_engine_web.h"
#include "layer_trw_track_internal.h"




using namespace SlavGPS;




#ifdef K_TODO
static void vik_routing_web_engine_set_property(void * object,
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




static void vik_routing_web_engine_get_property(void    * object,
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
#endif




RoutingEngineWeb::~RoutingEngineWeb()
{
	free(this->url_base);
	this->url_base = NULL;

	/* LatLon. */
	free(this->url_start_ll_fmt);
	this->url_start_ll_fmt = NULL;
	free(this->url_stop_ll_fmt);
	this->url_stop_ll_fmt = NULL;
	free(this->url_via_ll_fmt);
	this->url_via_ll_fmt = NULL;

	/* Directions. */
	free(this->url_start_dir_fmt);
	this->url_start_dir_fmt = NULL;
	free(this->url_stop_dir_fmt);
	this->url_stop_dir_fmt = NULL;

	free(this->dl_options.referer);
	this->dl_options.referer = NULL;
}




const DownloadOptions * RoutingEngineWeb::get_download_options(void) const
{
	return &this->dl_options;
}




static char * substitute_latlon(const char * fmt, const LatLon & lat_lon)
{
	QString string_lat;
	QString string_lon;
	CoordUtils::to_strings(string_lat, string_lon, lat_lon);

	char * substituted = g_strdup_printf(fmt, string_lat.toUtf8().constData(), string_lon.toUtf8().constData());
	return substituted;
}




char * RoutingEngineWeb::get_url_for_coords(const LatLon & start, const LatLon & end)
{
	if (!this->url_base || !this->url_start_ll_fmt || !this->url_stop_ll_fmt) {
		return NULL;
	}

	char * startURL = substitute_latlon(this->url_start_ll_fmt, start);
	char * endURL = substitute_latlon(this->url_stop_ll_fmt, end);
	char * url = g_strconcat(this->url_base, startURL, endURL, NULL);

	/* Free memory. */
	free(startURL);
	free(endURL);

	return url;
}




bool RoutingEngineWeb::find(LayerTRW * trw, const LatLon & start, const LatLon & end)
{
	char * uri = this->get_url_for_coords(start, end);

	char * format_ = this->get_format();
	ProcessOptions po(NULL, NULL, format_, uri); /* kamil FIXME: memory leak through these pointers? */
	bool ret = a_babel_convert_from(trw, &po, NULL, NULL, &this->dl_options);

	free(uri);

	return ret;
}




char * RoutingEngineWeb::get_url_from_directions(const char * start, const char * end)
{
	if (!this->url_base || !this->url_start_dir_fmt || !this->url_stop_dir_fmt) {
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

	char * url_fmt = g_strconcat(this->url_base, this->url_start_dir_fmt, this->url_stop_dir_fmt, NULL);
	char * url = g_strdup_printf(url_fmt, from_quoted, to_quoted);

	free(url_fmt);

	free(from_quoted);
	free(to_quoted);
	g_strfreev(from_split);
	g_strfreev(to_split);

	return url;
}




bool RoutingEngineWeb::supports_direction(void)
{
	return (this->url_start_dir_fmt) != NULL;
}




struct _append_ctx {
	RoutingEngineWeb * engine = NULL;
	char ** urlParts = NULL;
	int nb;
};




static void _append_stringified_coords(void * data, void * user_data)
{
	Trackpoint * tp = (Trackpoint *) data;
	struct _append_ctx *ctx = (struct _append_ctx*)user_data;

	/* Stringify coordinate. */
	char * string = substitute_latlon(ctx->engine->url_via_ll_fmt, tp->coord.get_latlon());

	/* Append. */
	ctx->urlParts[ctx->nb] = string;
	ctx->nb++;
}




char * RoutingEngineWeb::get_url_for_track(Track * trk)
{
	char ** urlParts = NULL;
	char * url = NULL;

	if (!this->url_base || !this->url_start_ll_fmt || !this->url_stop_ll_fmt || !this->url_via_ll_fmt) {
		return NULL;
	}

	/* Init temporary storage. */
	size_t len = 1 + trk->trackpoints.size() + 1; /* Base + trackpoints + NULL. */
	urlParts = (char **) malloc(sizeof(char*)*len);
	urlParts[0] = g_strdup(this->url_base);
	urlParts[len-1] = NULL;

	struct _append_ctx ctx;
	ctx.engine = this;
	ctx.urlParts = urlParts;
	ctx.nb = 1; /* First cell available, previous used for base URL. */

	/* Append all trackpoints to URL. */
	for (auto iter = trk->trackpoints.begin(); iter != trk->trackpoints.end(); iter++) {
		_append_stringified_coords(*iter, &ctx);
	}

	/* Override first and last positions with associated formats. */
	free(urlParts[1]);

	Trackpoint * tp = *trk->trackpoints.begin();
	urlParts[1] = substitute_latlon(this->url_start_ll_fmt, tp->coord.get_latlon());

	free(urlParts[len-2]);

	tp = *std::prev(trk->trackpoints.end());
	urlParts[len-2] = substitute_latlon(this->url_stop_ll_fmt, tp->coord.get_latlon());

	/* Concat. */
	url = g_strjoinv(NULL, urlParts);
	fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, url);

	/* Free. */
	g_strfreev(urlParts);

	return url;
}




bool RoutingEngineWeb::refine(LayerTRW * trw, Track * trk)
{
	/* Compute URL. */
	char * uri = this->get_url_for_track(trk);

	/* Convert and insert data in model. */
	char * format_ = this->get_format();
	ProcessOptions po(NULL, NULL, format_, uri); /* kamil FIXME: memory leak through these pointers? */
	bool ret = a_babel_convert_from(trw, &po, NULL, NULL, &this->dl_options);

	free(uri);

	return ret;
}




bool RoutingEngineWeb::supports_refine(void)
{
	return this->url_via_ll_fmt != NULL;
}
