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
  * SECTION:map_source_bing
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
#include <cmath>
#endif

#ifdef HAVE_STDLIB_H
#include <cstdlib>
#endif

#include <glib.h>
#include <glib/gstdio.h>

#include "globals.h"
#include "map_source_bing.h"
#include "map_utils.h"
#include "bbox.h"
#include "background.h"
#include "slav_qt.h"
#include "icons/icons.h"




using namespace SlavGPS;




/* Format for URL. */
#define URL_ATTR_FMT "http://dev.virtualearth.net/REST/v1/Imagery/Metadata/Aerial/0,0?zl=1&mapVersion=v1&key=%s&include=ImageryProviders&output=xml"




MapSourceBing::MapSourceBing()
{
	this->bing_api_key = NULL;
	this->attributions = NULL;
	this->attribution = NULL;
	this->loading_attributions = false;

#if 0
	pspec = g_param_spec_string("api-key",
				    "API key",
				    "The API key to access Bing",
                                     "<no-set>" /* default value */,
				    (GParamFlags) (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
#endif
#ifdef K
	logo = gdk_pixbuf_from_pixdata(&bing_maps_pixbuf, true, NULL);
#endif
}




/**
 * @id: internal identifier.
 * @label: the label to display in map provider selector.
 * @key: the API key to access Bing's services.
 *
 * Constructor for Bing map source.
 *
 * Returns: a newly allocated MapSourceBing object.
 */
MapSourceBing::MapSourceBing(MapTypeID map_type_, const char * label_, const char * key_)
{
	map_type = map_type_;
	label = g_strdup(label_);
	name = strdup("Bing-Aerial");
	server_hostname = strdup("ecn.t2.tiles.virtualearth.net");
	server_path_format = strdup("/tiles/a%s.jpeg?g=587");
	bing_api_key = g_strdup(key_);
	this->dl_options.check_file_server_time = true;
	zoom_min = 0;
	zoom_max = 19; /* NB: Might be regionally different rather than the same across the world. */
	copyright = strdup("© 2011 Microsoft Corporation and/or its suppliers");
	license = strdup("Microsoft Bing Maps Specific");
	license_url = strdup("http://www.microsoft.com/maps/assets/docs/terms.aspx");
}




MapSourceBing::~MapSourceBing()
{
	free(this->bing_api_key);
	this->bing_api_key = NULL;
}




char * MapSourceBing::compute_quad_tree(int zoom, int tilex, int tiley)
{
	/* Picked from http://trac.openstreetmap.org/browser/applications/editors/josm/plugins/slippymap/src/org/openstreetmap/josm/plugins/slippymap/SlippyMapPreferences.java?rev=24486 */
	char k[20];
	int ik = 0;
	for (int i = zoom; i > 0; i--) {
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



char * MapSourceBing::get_server_path(TileInfo * src)
{
	char * quadtree = compute_quad_tree(17 - src->scale, src->x, src->y);
	char * path = g_strdup_printf(server_path_format, quadtree);
	free(quadtree);
	return path;
}




void MapSourceBing::get_copyright(LatLonBBox bbox, double zoom, void (*fct)(Viewport *, QString const &), void * data)
{
	fprintf(stderr, "DEBUG: %s: looking for %g %g %g %g at %g\n", __FUNCTION__, bbox.south, bbox.north, bbox.east, bbox.west, zoom);

	int level = map_utils_mpp_to_scale(zoom);

	/* Loop over all known attributions. */
	GList * attribution = this->attributions;
	if (attribution == NULL && strcmp("<no-set>", this->bing_api_key)) {
		if (! this->loading_attributions) {
			this->async_load_attributions();
		} else {
			/* Wait until attributions loaded before processing them. */
			return;
		}
	}
	while (attribution != NULL) {
		struct _Attribution *current = (struct _Attribution*)attribution->data;
		/* fprintf(stderr, "DEBUG: %s %g %g %g %g %d %d\n", __FUNCTION__, current->bounds.south, current->bounds.north, current->bounds.east, current->bounds.west, current->minZoom, current->maxZoom); */
		if (BBOX_INTERSECT(bbox, current->bounds) &&
		    (17 - level) > current->minZoom &&
		    (17 - level) < current->maxZoom) {

			(*fct)((Viewport *) data, current->attribution);
			fprintf(stderr, "DEBUG: %s: found match %s\n", __FUNCTION__, current->attribution);
		}
		attribution = attribution->next;
	}
}




/* Called for open tags <foo bar="baz">. */
void MapSourceBing::bstart_element(GMarkupParseContext * context,
				   const char          * element_name,
				   const char         ** attribute_names,
				   const char         ** attribute_values,
				   void                * user_data,
				   GError             ** error)
{
	MapSourceBing * self = (MapSourceBing *) user_data;

	const char *element = g_markup_parse_context_get_element(context);
	if (strcmp (element, "CoverageArea") == 0) {
		/* New Attribution. */
		struct _Attribution * attribution = (struct _Attribution *) malloc(sizeof (struct _Attribution));
		memset(attribution, 0, sizeof (struct _Attribution));

		self->attributions = g_list_append(self->attributions, attribution);
		attribution->attribution = g_strdup(self->attribution);
	}
}




/* Called for character data. */
/* Text is not nul-terminated. */
void MapSourceBing::btext(GMarkupParseContext * context,
			  const char          * text,
			  size_t                text_len,
			  void                * user_data,
			  GError             ** error)
{
	MapSourceBing * self = (MapSourceBing *) user_data;

	struct _Attribution *attribution = (_Attribution *) (self->attributions == NULL ? NULL : g_list_last (self->attributions)->data);
	const char *element = g_markup_parse_context_get_element(context);
	char *textl = g_strndup(text, text_len);
	const GSList *stack = g_markup_parse_context_get_element_stack(context);
	int len = g_slist_length((GSList *)stack);

	const char *parent = len > 1 ? (const char *) g_slist_nth_data((GSList *)stack, 1) : (const char *) NULL;
	if (strcmp(element, "Attribution") == 0) {
		free(self->attribution);
		self->attribution = g_strdup(textl);
	} else {
		if (attribution) {
			if (parent != NULL && strcmp(parent, "CoverageArea") == 0) {
				if (strcmp (element, "ZoomMin") == 0) {
					attribution->minZoom = atoi(textl);
				} else if (strcmp (element, "ZoomMax") == 0) {
					attribution->maxZoom = atoi(textl);
				}
			} else if (parent != NULL && strcmp(parent, "BoundingBox") == 0) {
				if (strcmp(element, "SouthLatitude") == 0) {
					attribution->bounds.south = g_ascii_strtod(textl, NULL);
				} else if (strcmp(element, "WestLongitude") == 0) {
					attribution->bounds.west = g_ascii_strtod(textl, NULL);
				} else if (strcmp(element, "NorthLatitude") == 0) {
					attribution->bounds.north = g_ascii_strtod(textl, NULL);
				} else if (strcmp(element, "EastLongitude") == 0) {
					attribution->bounds.east = g_ascii_strtod(textl, NULL);
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

	FILE *file = fopen(filename, "r");
	if (file == NULL) {
		/* TODO emit warning. */
		return false;
	}
#ifdef K
	/* Setup context parse (i.e. callbacks). */
	xml_parser.start_element = &bstart_element;
	xml_parser.end_element = NULL;
	xml_parser.text = &btext;
	xml_parser.passthrough = NULL;
	xml_parser.error = NULL;
#endif

	xml_context = g_markup_parse_context_new(&xml_parser, (GMarkupParseFlags) 0, this, NULL);

	char buff[BUFSIZ];
	size_t nb;
	size_t offset = -1;
	while (xml_context &&
	       (nb = fread(buff, sizeof(char), BUFSIZ, file)) > 0) {
		if (offset == -1) {
			/* First run. */
			/* Avoid possible BOM at begining of the file. */
			offset = buff[0] == '<' ? 0 : 3;
		} else {
			/* Reset offset. */
			offset = 0;
		}

		if (!g_markup_parse_context_parse(xml_context, buff+offset, nb-offset, &error)) {
			fprintf(stderr, "%s: parsing error: %s.\n",
				__FUNCTION__, error->message);
			g_markup_parse_context_free(xml_context);
			xml_context = NULL;
		}
		g_clear_error(&error);
	}
	/* Cleanup. */
	if (xml_context &&
	    !g_markup_parse_context_end_parse(xml_context, &error)) {
		fprintf(stderr, "%s: errors occurred while reading file: %s.\n", __FUNCTION__, error->message);
	}

	g_clear_error(&error);

	if (xml_context) {
		g_markup_parse_context_free(xml_context);
	}
	xml_context = NULL;
	fclose(file);

	if (vik_debug) {
		GList * attribution = this->attributions;
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
	int ret = 0;  /* OK. */

	this->loading_attributions = true;
	char * uri = g_strdup_printf(URL_ATTR_FMT, this->bing_api_key);

	char * tmpname = Download::get_uri_to_tmp_file(uri, this->get_download_options());
	if (!tmpname) {
		ret = -1;
		goto done;
	}

	fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, tmpname);
	if (!this->parse_file_for_attributions(tmpname)) {
		ret = -1;
	}

	(void) remove(tmpname);
	free(tmpname);
done:
	this->loading_attributions = false;
	free(uri);
	return ret;
}




int MapSourceBing::emit_update(void * data)
{
#ifdef K
	gdk_threads_enter();
	/* TODO
	vik_layers_panel_emit_update(VIK_LAYERS_PANEL (data));
	*/
	gdk_threads_leave();
#endif
	return 0;
}




static int load_attributions_thread(BackgroundJob * bg_job)
{
#ifdef K
	bg_job->load_attributions();
#endif
	int result = a_background_thread_progress(bg_job, 1);
	if (result != 0) {
		return -1; /* Abort thread. */
	}
#ifdef K
	/* Emit update. */
	/* As we are on a download thread, it's better to fire the update from the main loop. */
	g_idle_add((GSourceFunc)_emit_update, NULL /* FIXME */);
#endif

	return 0;
}




void MapSourceBing::async_load_attributions()
{
#ifdef K
	/* kamilFIXME: since object passed to a_background_thread() is of type BackgroundJob,
	   and MapSourceBing does not inherit from BackgroundJob, we have a type error here. */
	this->thread_fn = load_attributions_thread;
	this->n_items = 1;

	a_background_thread(this, ThreadPoolType::REMOTE, QString(QObject::tr("Bing attribution Loading")));
#endif
}
