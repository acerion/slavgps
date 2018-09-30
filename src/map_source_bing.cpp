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




#include <list>
#include <cmath>
#include <cstdlib>




#include <QDebug>
#include <QDir>




#include <glib.h>
#include <glib/gstdio.h>




#include "vikutils.h"
#include "map_source_bing.h"
#include "map_utils.h"
#include "bbox.h"
#include "background.h"
#include "viewport_internal.h"




using namespace SlavGPS;




#define PREFIX " MapSourceBing:" << __FUNCTION__ << __LINE__ << ">"




/* Format for URL. */
#define URL_ATTR_FMT "http://dev.virtualearth.net/REST/v1/Imagery/Metadata/Aerial/0,0?zl=1&mapVersion=v1&key=%1&include=ImageryProviders&output=xml"




extern bool vik_debug;




class MapSourceBingAttributions : public BackgroundJob {
public:
	MapSourceBingAttributions(MapSourceBing * new_map_source) : map_source(new_map_source) {};
	~MapSourceBingAttributions() {};

	void run(void);
private:
	MapSourceBing * map_source = NULL;
};




MapSourceBing::MapSourceBing()
{
#ifdef K_FIXME_RESTORE
	pspec = g_param_spec_string("api-key",
				    "API key",
				    "The API key to access Bing",
                                     "<no-set>" /* default value */,
				    (GParamFlags) (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
#endif
	this->logo.logo_pixmap = QPixmap(":/icons/bing_maps.png");
	this->logo.logo_id = "Bing Maps"; /* TODO_2_LATER: verify this label, whether it is unique for this map source. */
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
MapSourceBing::MapSourceBing(MapTypeID new_map_type_id, const QString & new_label, const QString & new_key)
{
	this->map_type_id = new_map_type_id;
	this->label = new_label;
	this->map_type_string = "Bing-Aerial"; /* Non-translatable. */
	this->server_hostname = "ecn.t2.tiles.virtualearth.net";
	this->server_path_format = "/tiles/a%1.jpeg?g=587";
	this->bing_api_key = new_key;
	this->dl_options.check_file_server_time = true;
	this->set_supported_tile_zoom_level_range(0, 19); /* Maximum zoom level may be regionally different rather than the same across the world. */
	this->copyright = "© 2011 Microsoft Corporation and/or its suppliers";
	this->license = "Microsoft Bing Maps Specific";
	this->license_url = "http://www.microsoft.com/maps/assets/docs/terms.aspx";
}




MapSourceBing::~MapSourceBing()
{
}




QString MapSourceBing::compute_quad_tree(int zoom, int tilex, int tiley) const
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
	return QString(k);
}



const QString MapSourceBing::get_server_path(const TileInfo & src) const
{
	const QString quadtree = compute_quad_tree(src.scale.get_tile_zoom_level(), src.x, src.y);
	const QString uri = QString(this->server_path_format).arg(quadtree);

	return uri;
}




void MapSourceBing::add_copyright(Viewport * viewport, const LatLonBBox & bbox, const VikingZoomLevel & viking_zoom_level)
{
	qDebug() << "DD" PREFIX << "looking for" << bbox << "at zoom" << viking_zoom_level.get_x();

	const TileScale tile_scale = viking_zoom_level.to_tile_scale();

	/* Load attributions. */
	if (0 == this->attributions.size() && "<no-set>" != this->bing_api_key) { /* TODO_2_LATER: also check this->bing_api_key.isEmpty()? */
		if (!this->loading_attributions) {
			this->async_load_attributions();
		} else {
			/* Wait until attributions loaded before processing them. */
			return;
		}
	}

	/* Loop over all known attributions. */
	for (auto iter = this->attributions.begin(); iter != this->attributions.end(); iter++) {
		const Attribution * current = *iter;
		/* fprintf(stderr, "DEBUG: %s %g %g %g %g %d %d\n", __FUNCTION__, current->bounds.south, current->bounds.north, current->bounds.east, current->bounds.west, current->minZoom, current->maxZoom); */
		if (BBOX_INTERSECT(bbox, current->bounds) &&
		    (tile_scale.get_tile_zoom_level()) > current->minZoom &&
		    (tile_scale.get_tile_zoom_level()) < current->maxZoom) {

			viewport->add_copyright(current->attribution);
			qDebug() << "DD: Map Source Bind: get copyright: found match:" << current->attribution;
		}
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
		Attribution * attribution = new Attribution;

		self->attributions.push_back(attribution);
		attribution->attribution = self->attribution;
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

	Attribution * attr = self->attributions.size() == 0  ? NULL : self->attributions.back();
	const char * element = g_markup_parse_context_get_element(context);
	const QString textl = QString(text).left(text_len);
	const GSList *stack = g_markup_parse_context_get_element_stack(context);
	int len = g_slist_length((GSList *)stack);

	const char *parent = len > 1 ? (const char *) g_slist_nth_data((GSList *)stack, 1) : (const char *) NULL;
	if (strcmp(element, "Attribution") == 0) {
		self->attribution = textl;
	} else {
		if (attr) {
			if (parent != NULL && strcmp(parent, "CoverageArea") == 0) {
				if (strcmp (element, "ZoomMin") == 0) {
					attr->minZoom = atoi(textl.toUtf8().constData());
				} else if (strcmp (element, "ZoomMax") == 0) {
					attr->maxZoom = atoi(textl.toUtf8().constData());
				}
			} else if (parent != NULL && strcmp(parent, "BoundingBox") == 0) {
				if (strcmp(element, "SouthLatitude") == 0) {
					attr->bounds.south = SGUtils::c_to_double(textl);
				} else if (strcmp(element, "WestLongitude") == 0) {
					attr->bounds.west = SGUtils::c_to_double(textl);
				} else if (strcmp(element, "NorthLatitude") == 0) {
					attr->bounds.north = SGUtils::c_to_double(textl);
				} else if (strcmp(element, "EastLongitude") == 0) {
					attr->bounds.east = SGUtils::c_to_double(textl);
				}
			}
		}
	}
}




/*
  @file passed to this function is an opened QTemporaryFile.
*/
bool MapSourceBing::parse_file_for_attributions(QFile & tmp_file)
{
	GMarkupParser xml_parser;
	GMarkupParseContext *xml_context = NULL;
	GError *error = NULL;

	FILE *file = fopen(tmp_file.fileName().toUtf8().constData(), "r");
	if (file == NULL) {
		/* TODO_2_LATER emit warning. */
		return false;
	}
#ifdef K_FIXME_RESTORE
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
		if (offset == (size_t) -1) {
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
		for (auto iter = this->attributions.begin(); iter != this->attributions.end(); iter++) {
			const Attribution * aa = *iter;
			fprintf(stderr, "DD: Map Source Bing: Bing Attribution: %s from %d to %d %g %g %g %g\n", aa->attribution.toUtf8().constData(), aa->minZoom, aa->maxZoom, aa->bounds.south, aa->bounds.north, aa->bounds.east, aa->bounds.west);
		}
	}

	return true;
}




int MapSourceBing::load_attributions()
{
	int ret = 0;  /* OK. */

	this->loading_attributions = true;
	const QString uri = QString(URL_ATTR_FMT).arg(this->bing_api_key);

	DownloadHandle dl_handle(this->get_download_options());
	QTemporaryFile tmp_file;
	if (!dl_handle.download_to_tmp_file(tmp_file, uri)) {
		ret = -1;
		goto done;
	}

	qDebug() << "DD:" PREFIX << "load attributions from" << tmp_file.fileName();
	if (!this->parse_file_for_attributions(tmp_file)) {
		ret = -1;
	}

	tmp_file.remove();

done:
	this->loading_attributions = false;
	return ret;
}




int MapSourceBing::emit_update(void * data)
{
#ifdef K_FIXME_RESTORE
	gdk_threads_enter();
	/* TODO_2_LATER
	items_tree_emit_update(VIK_LAYERS_PANEL (data));
	*/
	gdk_threads_leave();
#endif
	return 0;
}



/* Function for loading attributions. */
void MapSourceBingAttributions::run(void)
{
	this->map_source->load_attributions();

	const bool end_job = this->set_progress_state(1);
	if (end_job) {
		return; /* Abort thread. */
	}
#ifdef K_FIXME_RESTORE
	/* Emit update. */
	/* As we are on a download thread, it's better to fire the update from the main loop. */
	g_idle_add((GSourceFunc)_emit_update, NULL /* FIXME */);
#endif

	return;
}




void MapSourceBing::async_load_attributions()
{
	MapSourceBingAttributions * bg_job = new MapSourceBingAttributions(this);
	bg_job->n_items = 1;
	bg_job->set_description(QObject::tr("Bing attribution Loading"));

	bg_job->run_in_background(ThreadPoolType::Remote);
}
