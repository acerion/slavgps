/*
 * viking
 * Copyright (C) 2011, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *
 * viking is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * viking is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

 /**
  * SECTION:bingmapsource
  * @short_description: the class for Bing Maps
  *
  * The #BingMapSource class handles Bing map source.
  *
  * License and term of use are available here:
  * http://wiki.openstreetmap.org/wiki/File:Bing_license.pdf
  *
  * Technical details are available here:
  * http://msdn.microsoft.com/en-us/library/dd877180.aspx
  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#include "globals.h"
#include "bingmapsource.h"
#include "maputils.h"
#include "bbox.h"
#include "background.h"
#include "icons/icons.h"


using namespace SlavGPS;


/* Format for URL */
#define URL_ATTR_FMT "http://dev.virtualearth.net/REST/v1/Imagery/Metadata/Aerial/0,0?zl=1&mapVersion=v1&key=%s&include=ImageryProviders&output=xml"


MapSourceBing::MapSourceBing()
{
	priv->bing_api_key = NULL;
	priv->attributions = NULL;
	priv->attribution = NULL;
	priv->loading_attributions = false;

#if 0
	pspec = g_param_spec_string ("api-key",
                                     "API key",
                                     "The API key to access Bing",
                                     "<no-set>" /* default value */,
				     (GParamFlags) (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
#endif
	logo = gdk_pixbuf_from_pixdata(&bing_maps_pixbuf, true, NULL);
}

/**
 * @id: internal identifier.
 * @label: the label to display in map provider selector.
 * @key: the API key to access Bing's services.
 *
 * Constructor for Bing map source.
 *
 * Returns: a newly allocated MapSourceBing object
 */
MapSourceBing::MapSourceBing(uint16_t id_, const char * label_, const char * key_)
{
	uniq_id = id_;
	label = g_strdup(label_);
	name = strdup("Bing-Aerial");
	server_hostname = strdup("ecn.t2.tiles.virtualearth.net");
	server_path_format = strdup("/tiles/a%s.jpeg?g=587");
	bing_api_key = g_strdup(key_);
	download_options.check_file_server_time = true;
	zoom_min = 0;
	zoom_max = 19; /* NB: Might be regionally different rather than the same across the world. */
	copyright = strdup("© 2011 Microsoft Corporation and/or its suppliers");
	license = strdup("Microsoft Bing Maps Specific");
	license_url = "http://www.microsoft.com/maps/assets/docs/terms.aspx");
}

MapSourceBing::~MapSourceBing()
{
	free(priv->api_key);
	priv->api_key = NULL;
}


char * MapSourceBing::compute_quad_tree(int zoom, int tilex, int tiley)
{
	/* Picked from http://trac.openstreetmap.org/browser/applications/editors/josm/plugins/slippymap/src/org/openstreetmap/josm/plugins/slippymap/SlippyMapPreferences.java?rev=24486 */
	char k[20];
	int ik = 0;
	int i = 0;
	for(i = zoom; i > 0; i--)
	{
		char digit = 48;
		int mask = 1 << (i - 1);
		if ((tilex & mask) != 0) {
			digit += 1;
		}
		if ((tiley & mask) != 0) {
			digit += 2;
		}
		k[ik++] = digit;
	}
	k[ik] = '\0';
	return g_strdup(k);
}

char * MapSourceBing::get_server_path(MapCoord *src)
{
	char * quadtree = compute_quad_tree(17 - src->scale, src->x, src->y);
	char * path = g_strdup_printf(server_path_format, quadtree);
	free(quadtree);
	return path;
}

void MapSourceBing::get_copyright(LatLonBBox bbox, double zoom, void (*fct)(VikViewport*,const char*), void *data)
{
	fprintf(stderr, "DEBUG: %s: looking for %g %g %g %g at %g\n", __FUNCTION__, bbox.south, bbox.north, bbox.east, bbox.west, zoom);

	int level = map_utils_mpp_to_scale (zoom);

	/* Loop over all known attributions */
	GList * attribution = priv->attributions;
	if (attribution == NULL && g_strcmp0 ("<no-set>", priv->api_key)) {
		if ( ! priv->loading_attributions )
			_async_load_attributions (BING_MAP_SOURCE (self));
		else
			// Wait until attributions loaded before processing them
			return;
	}
	while (attribution != NULL) {
		struct _Attribution *current = (struct _Attribution*)attribution->data;
		/* fprintf(stderr, "DEBUG: %s %g %g %g %g %d %d\n", __FUNCTION__, current->bounds.south, current->bounds.north, current->bounds.east, current->bounds.west, current->minZoom, current->maxZoom); */
		if (BBOX_INTERSECT(bbox, current->bounds) &&
		    (17 - level) > current->minZoom &&
		    (17 - level) < current->maxZoom) {
			(*fct)((VikViewport *) data, current->attribution);
			fprintf(stderr, "DEBUG: %s: found match %s\n", __FUNCTION__, current->attribution);
		}
		attribution = attribution->next;
	}
}

/* Called for open tags <foo bar="baz"> */
void MapSourceBing::bstart_element(GMarkupParseContext *context,
					  const char         *element_name,
					  const char        **attribute_names,
					  const char        **attribute_values,
					  void *             user_data,
					  GError             **error)
{
	BingMapSource *self = BING_MAP_SOURCE (user_data);
	BingMapSourcePrivate *priv = BING_MAP_SOURCE_GET_PRIVATE (self);
	const char *element = g_markup_parse_context_get_element (context);
	if (strcmp (element, "CoverageArea") == 0) {
		/* New Attribution */
		struct _Attribution * attribution = (struct _Attribution *) malloc(sizeof (struct _Attribution));
		memset(attribution, 0, sizeof (struct _Attribution));

		priv->attributions = g_list_append (priv->attributions, attribution);
		attribution->attribution = g_strdup(priv->attribution);
	}
}

/* Called for character data */
/* text is not nul-terminated */
void MapSourceBing::btext(GMarkupParseContext * context,
			  const char          * text,
			  size_t                text_len,
			  void                * user_data,
			  GError             ** error)
{
	BingMapSource *self = BING_MAP_SOURCE (user_data);
	BingMapSourcePrivate *priv = BING_MAP_SOURCE_GET_PRIVATE (self);

	struct _Attribution *attribution = (_Attribution *) (priv->attributions == NULL ? NULL : g_list_last (priv->attributions)->data);
	const char *element = g_markup_parse_context_get_element (context);
	char *textl = g_strndup (text, text_len);
	const GSList *stack = g_markup_parse_context_get_element_stack (context);
	int len = g_slist_length ((GSList *)stack);

	const char *parent = len > 1 ? (const char *) g_slist_nth_data ((GSList *)stack, 1) : (const char *) NULL;
	if (strcmp (element, "Attribution") == 0) {
		free(priv->attribution);
		priv->attribution = g_strdup(textl);
	}
	else {
		if ( attribution ) {
			if (parent != NULL && strcmp (parent, "CoverageArea") == 0) {
				if (strcmp (element, "ZoomMin") == 0) {
					attribution->minZoom = atoi (textl);
				} else if (strcmp (element, "ZoomMax") == 0) {
					attribution->maxZoom = atoi (textl);
				}
			} else if (parent != NULL && strcmp (parent, "BoundingBox") == 0) {
				if (strcmp (element, "SouthLatitude") == 0) {
					attribution->bounds.south = g_ascii_strtod (textl, NULL);
				} else if (strcmp (element, "WestLongitude") == 0) {
					attribution->bounds.west = g_ascii_strtod (textl, NULL);
				} else if (strcmp (element, "NorthLatitude") == 0) {
					attribution->bounds.north = g_ascii_strtod (textl, NULL);
				} else if (strcmp (element, "EastLongitude") == 0) {
					attribution->bounds.east = g_ascii_strtod (textl, NULL);
				}
			}
		}
	}
	free(textl);
}

bool MapSourceBing::parse_file_for_attributions(char *filename)
{
	GMarkupParser xml_parser;
	GMarkupParseContext *xml_context = NULL;
	GError *error = NULL;
	BingMapSourcePrivate *priv = BING_MAP_SOURCE_GET_PRIVATE (self);
	g_return_val_if_fail(priv != NULL, false);

	FILE *file = g_fopen (filename, "r");
	if (file == NULL)
		/* TODO emit warning */
		return false;

	/* setup context parse (ie callbacks) */
	xml_parser.start_element = &bstart_element;
	xml_parser.end_element = NULL;
	xml_parser.text = &btext;
	xml_parser.passthrough = NULL;
	xml_parser.error = NULL;

	xml_context = g_markup_parse_context_new(&xml_parser, (GMarkupParseFlags) 0, self, NULL);

	char buff[BUFSIZ];
	size_t nb;
	size_t offset = -1;
	while (xml_context &&
	       (nb = fread (buff, sizeof(char), BUFSIZ, file)) > 0)
	{
		if (offset == -1)
			/* first run */
			/* Avoid possible BOM at begining of the file */
			offset = buff[0] == '<' ? 0 : 3;
		else
			/* reset offset */
			offset = 0;
		if (!g_markup_parse_context_parse(xml_context, buff+offset, nb-offset, &error))
		{
			fprintf(stderr, "%s: parsing error: %s.\n",
				__FUNCTION__, error->message);
			g_markup_parse_context_free(xml_context);
			xml_context = NULL;
		}
		g_clear_error (&error);
	}
	/* cleanup */
	if (xml_context &&
	    !g_markup_parse_context_end_parse(xml_context, &error))
		fprintf(stderr, "%s: errors occurred while reading file: %s.\n",
			__FUNCTION__, error->message);
	g_clear_error (&error);

	if (xml_context)
		g_markup_parse_context_free(xml_context);
	xml_context = NULL;
	fclose (file);

	if (vik_debug) {
		GList *attribution = priv->attributions;
		while (attribution != NULL) {
			struct _Attribution *aa = (struct _Attribution*)attribution->data;
			fprintf(stderr, "DEBUG: Bing Attribution: %s from %d to %d %g %g %g %g\n", aa->attribution, aa->minZoom, aa->maxZoom, aa->bounds.south, aa->bounds.north, aa->bounds.east, aa->bounds.west);
			attribution = attribution->next;
		}
	}

	return true;
}

int MapSourceBing::load_attributions()
{
	int ret = 0;  /* OK */

	BingMapSourcePrivate *priv = BING_MAP_SOURCE_GET_PRIVATE (self);
	priv->loading_attributions = true;
	char *uri = g_strdup_printf(URL_ATTR_FMT, priv->api_key);

	char *tmpname = a_download_uri_to_tmp_file ( uri, vik_map_source_default_get_download_options(VIK_MAP_SOURCE_DEFAULT(self)) );
	if ( !tmpname ) {
		ret = -1;
		goto done;
	}

	fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, tmpname);
	if (!_parse_file_for_attributions(self, tmpname)) {
		ret = -1;
	}

	(void)g_remove(tmpname);
	free(tmpname);
done:
	priv->loading_attributions = false;
	free(uri);
	return ret;
}

int MapSourceBing::emit_update(void * data)
{
	gdk_threads_enter();
	/* TODO
	vik_layers_panel_emit_update ( VIK_LAYERS_PANEL (data) );
	*/
	gdk_threads_leave();
	return 0;
}

int MapSourceBing::load_attributions_thread(void * threaddata)
{
	_load_attributions ( self );
	int result = a_background_thread_progress ( threaddata, 1.0 );
	if ( result != 0 )
		return -1; /* Abort thread */

	/* Emit update */
	/* As we are on a download thread,
	 * it's better to fire the update from the main loop.
	 */
	g_idle_add ( (GSourceFunc)_emit_update, NULL /* FIXME */ );

	return 0;
}

void MapSourceBing::async_load_attributions()
{
	a_background_thread ( BACKGROUND_POOL_REMOTE,
	                      /*VIK_GTK_WINDOW_FROM_WIDGET(vp)*/NULL,
	                      _("Bing attribution Loading"),
	                      (vik_thr_func) _load_attributions_thread,
	                      self,
	                      NULL,
	                      NULL,
	                      1);
}
