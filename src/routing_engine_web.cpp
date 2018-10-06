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




#include <cstring>
#include <cstdlib>




#include <QDebug>




#include <glib.h>




#include "babel.h"
#include "routing_engine_web.h"
#include "layer_trw_track_internal.h"




using namespace SlavGPS;




#define PREFIX ": Web Routing Engine:" << __FUNCTION__ << __LINE__ << ">"




#ifdef K_FIXME_RESTORE
static void vik_routing_web_engine_set_property(void * object,
						unsigned int property_id,
						const GValue * value,
						GParamSpec   * pspec)
{
	VikRoutingWebEnginePrivate * priv = VIK_ROUTING_WEB_ENGINE_PRIVATE (object);

	switch (property_id) {
	case PROP_URL_BASE:
		priv->url_base = g_value_get_string(value);
		break;

	case PROP_URL_START_LL:
		priv->url_start_ll_fmt = g_value_get_string(value);
		break;

	case PROP_URL_STOP_LL:
		priv->url_stop_ll_fmt = g_value_get_string(value);
		break;

	case PROP_URL_VIA_LL:
		priv->url_via_ll_fmt = g_value_get_string(value);
		break;

	case PROP_URL_START_DIR:
		priv->url_start_dir_fmt = g_value_get_string(value);
		break;

	case PROP_URL_STOP_DIR:
		priv->url_stop_dir_fmt = g_value_get_string(value);
		break;

	case PROP_REFERER:
		priv->options.referer = value;
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
		g_value_set_string(value, priv->url_base.toUtf8().constData());
		break;

	case PROP_URL_START_LL:
		g_value_set_string(value, priv->url_start_ll_fmt.toUf8.constData());
		break;

	case PROP_URL_STOP_LL:
		g_value_set_string(value, priv->url_stop_ll_fmt.toUtf8().constData());
		break;

	case PROP_URL_VIA_LL:
		g_value_set_string(value, priv->url_via_ll_fmt.toUtf8().constData());
		break;

	case PROP_URL_START_DIR:
		g_value_set_string(value, priv->url_start_dir_fmt.toUtf8().constData());
		break;

	case PROP_URL_STOP_DIR:
		g_value_set_string(value, priv->url_stop_dir_fmt.toUtf8().constData());
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
}




const DownloadOptions * RoutingEngineWeb::get_download_options(void) const
{
	return &this->dl_options;
}




static QString substitute_latlon(const QString & fmt, const LatLon & lat_lon)
{
	QString string_lat;
	QString string_lon;
	lat_lon.to_strings_raw(string_lat, string_lon);

	const QString result = QString(fmt).arg(string_lat).arg(string_lon);
	return result;
}




QString RoutingEngineWeb::get_url_for_coords(const LatLon & start, const LatLon & end)
{
	if (this->url_base.isEmpty() || this->url_start_ll_fmt.isEmpty() || this->url_stop_ll_fmt.isEmpty()) {
		return NULL;
	}

	const QString start_url = substitute_latlon(this->url_start_ll_fmt, start);
	const QString end_url = substitute_latlon(this->url_stop_ll_fmt, end);
	const QString url = this->url_base + start_url + end_url;

	return url;
}




bool RoutingEngineWeb::find(LayerTRW * trw, const LatLon & start, const LatLon & end)
{
	AcquireOptions babel_options(AcquireOptionsMode::FromURL);
	babel_options.source_url = this->get_url_for_coords(start, end);
	babel_options.input_data_format = this->get_format();

	bool ret = babel_options.import_from_url(trw, &this->dl_options, NULL);

	return ret;
}




QString RoutingEngineWeb::get_url_from_directions(const QString & start, const QString & end)
{
	if (this->url_base.isEmpty() || this->url_start_dir_fmt.isEmpty() || this->url_stop_dir_fmt.isEmpty()) {
		return "";
	}

	char * from_quoted = g_shell_quote(start.toUtf8().constData());
	char * to_quoted = g_shell_quote(end.toUtf8().constData());

	char ** from_split = g_strsplit(from_quoted, " ", 0);
	char ** to_split = g_strsplit(to_quoted, " ", 0);

	from_quoted = g_strjoinv("%20", from_split);
	to_quoted = g_strjoinv("%20", to_split);

	const QString url_fmt = this->url_base + this->url_start_dir_fmt + this->url_stop_dir_fmt;
	const QString url = QString(url_fmt).arg(from_quoted).arg(to_quoted);

	free(from_quoted);
	free(to_quoted);
	g_strfreev(from_split);
	g_strfreev(to_split);

	return url;
}




bool RoutingEngineWeb::supports_direction(void)
{
	return !this->url_start_dir_fmt.isEmpty();
}




class URLParts {
public:
	void append_tp_coords(const Trackpoint * tp);

	RoutingEngineWeb * engine = NULL;
	QStringList url_parts;
	int nb = 0;
};




void URLParts::append_tp_coords(const Trackpoint * tp)
{
	/* Stringify coordinate. */
	const QString string = substitute_latlon(this->engine->url_via_ll_fmt, tp->coord.get_latlon());

	/* Append. */
	this->url_parts[this->nb] = string;
	this->nb++;
}




QString RoutingEngineWeb::get_url_for_track(Track * trk)
{
	if (this->url_base.isEmpty() || this->url_start_ll_fmt.isEmpty() || this->url_stop_ll_fmt.isEmpty() || this->url_via_ll_fmt.isEmpty()) {
		return NULL;
	}

	/* Init temporary storage. */
	size_t len = 1 + trk->trackpoints.size() + 1; /* Base + trackpoints + NULL. */

	URLParts ctx;
	ctx.engine = this;
	ctx.url_parts << this->url_base;
	ctx.nb = 1; /* First cell available (free). Zero-th cell is used for base URL. */

	/* Append all trackpoints to URL. */
	for (auto iter = trk->trackpoints.begin(); iter != trk->trackpoints.end(); iter++) {
		ctx.append_tp_coords(*iter);
	}

	/* Override first and last positions with associated formats. */
	Trackpoint * tp = *trk->trackpoints.begin();
	ctx.url_parts[1] = substitute_latlon(this->url_start_ll_fmt, tp->coord.get_latlon()); /* TODO_2_LATER: have more control and checks for list index. */

	tp = *std::prev(trk->trackpoints.end());
	ctx.url_parts[len - 2] = substitute_latlon(this->url_stop_ll_fmt, tp->coord.get_latlon()); /* TODO_2_LATER: have more control and checks for list index. */

	/* Concat. */
	const QString url = ctx.url_parts.join("");
	qDebug() << "DD" PREFIX << "Final url is" << url;

	return url;
}




bool RoutingEngineWeb::refine(LayerTRW * trw, Track * trk)
{
	AcquireOptions babel_options(AcquireOptionsMode::FromURL);
	babel_options.source_url = this->get_url_for_track(trk);
	babel_options.input_data_format = this->get_format();

	/* Convert and insert data in model. */
	bool ret = babel_options.import_from_url(trw, &this->dl_options, NULL);

	return ret;
}




bool RoutingEngineWeb::supports_refine(void)
{
	return !this->url_via_ll_fmt.isEmpty();
}
