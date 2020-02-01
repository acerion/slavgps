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
#include <QXmlDefaultHandler>




#include "vikutils.h"
#include "map_source_bing.h"
#include "map_utils.h"
#include "bbox.h"
#include "background.h"
#include "viewport_internal.h"
#include "tree_view.h"
#include "layers_panel.h"
#include "window.h"




using namespace SlavGPS;




#define SG_MODULE "MapSource Bing"




/* Format for URL. */
#define URL_ATTR_FMT "http://dev.virtualearth.net/REST/v1/Imagery/Metadata/Aerial/0,0?zl=1&mapVersion=v1&key=%1&include=ImageryProviders&output=xml"




extern bool vik_debug;




class XMLHandlerBing : public QXmlDefaultHandler {

public:
	XMLHandlerBing(MapSourceBing * new_map_source) : map_source(new_map_source) {};

	bool startDocument(void);
	bool endDocument(void);
	bool startElement(const QString &namespaceURI, const QString &localName, const QString &qName, const QXmlAttributes &atts);
	bool endElement(const QString &namespaceURI, const QString &localName, const QString &qName);
	bool characters(const QString & ch);

	bool fatalError(const QXmlParseException & exception);
private:
	MapSourceBing * map_source = NULL;
	BingImageryProvider * current_provider = NULL;
	QStringList stack;
};




class MapSourceBingProviders : public BackgroundJob {
public:
	MapSourceBingProviders(MapSourceBing * new_map_source) : map_source(new_map_source) {};
	~MapSourceBingProviders() {};

	void run(void);
private:
	MapSourceBing * map_source = NULL;
};




MapSourceBing::MapSourceBing()
{
	this->bing_api_key = "<no-set>";
	this->logo.logo_pixmap = QPixmap(":/icons/bing_maps.png");
	this->logo.logo_id = "Bing Maps"; /* TODO_LATER: verify this label, whether it is unique for this map source. */
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
	const QString quadtree = compute_quad_tree(src.scale.get_osm_tile_zoom_level(), src.x, src.y);
	const QString uri = QString(this->server_path_format).arg(quadtree);

	return uri;
}




void MapSourceBing::add_copyright(GisViewport * gisview, const LatLonBBox & bbox, const VikingScale & viking_scale)
{
	qDebug() << SG_PREFIX_D << "Looking for" << bbox << "at scale" << viking_scale.get_x();

	const TileScale tile_scale = viking_scale.to_tile_scale();

	/* Load imagery providers. */
	if (0 == this->providers.size() && "<no-set>" != this->bing_api_key) { /* TODO_LATER: also check this->bing_api_key.isEmpty()? */
		if (this->loading_providers) {
			/* Wait until providers are loaded before processing them. */
			return;
		} else {
			this->async_load_providers();
		}
	}

	/* Loop over all known providers. */
	for (auto iter = this->providers.begin(); iter != this->providers.end(); iter++) {
		const BingImageryProvider * current = *iter;
		/* fprintf(stderr, "DEBUG: %s %g %g %g %g %d %d\n", __FUNCTION__, current->bbox.south, current->bbox.north, current->bbox.east, current->bbox.west, current->zoom_min, current->zoom_max); */
		if (bbox.intersects_with(current->bbox) &&
		    (tile_scale.get_osm_tile_zoom_level()) > current->zoom_min &&
		    (tile_scale.get_osm_tile_zoom_level()) < current->zoom_max) {

			gisview->add_attribution(current->attribution);
			qDebug() << SG_PREFIX_D << "Found match:" << current->attribution;
		}
	}
}




bool XMLHandlerBing::startDocument(void)
{
	qDebug() << SG_PREFIX_I;
	return true;
}




bool XMLHandlerBing::endDocument(void)
{
	qDebug() << SG_PREFIX_I;
	return true;
}




bool XMLHandlerBing::startElement(__attribute__((unused)) const QString &namespaceURI, const QString &localName, __attribute__((unused)) const QString &qName, __attribute__((unused)) const QXmlAttributes &atts)
{
	qDebug() << SG_PREFIX_I << "Opening tag for" << localName;
	this->stack.push_back(localName);

	if (localName == "ImageryProvider") {
		this->current_provider = new BingImageryProvider;
	}
	return true;
}




bool XMLHandlerBing::endElement(__attribute__((unused)) const QString &namespaceURI, const QString &localName, __attribute__((unused)) const QString &qName)
{
	qDebug() << SG_PREFIX_I << "Closing tag for" << localName;
	this->stack.pop_back();

	if (localName == "ImageryProvider") {
		if (this->current_provider) {
			this->map_source->providers.push_back(this->current_provider);
			this->current_provider = NULL;
		} else {
			qDebug() << SG_PREFIX_E << "No 'current provider' object found when handling closing ImageryProvider tag";
		}
	}

	return true;
}




/* Called for character data. */
/* Text is not nul-terminated. */
bool XMLHandlerBing::characters(const QString & ch)
{
	const int stack_size = this->stack.size();
	if (0 == stack_size) {
		qDebug() << SG_PREFIX_E << "Characters outside of xml tree:" << ch;
		return false;
	}

	if (stack_size < 2) {
		/* We aren't interested in xml tags at this low level of nesting. */
		return true;
	}

	QString current = this->stack.at(stack_size - 1);
	QString parent = this->stack.at(stack_size - 2);


	if (parent == "ImageryProvider" && current == "Attribution") {
		this->current_provider->attribution = ch;
		qDebug() << SG_PREFIX_I << "Attribution =" << this->current_provider->attribution;

	} else if (parent == "CoverageArea") {
		if (current == "ZoomMin") {
			this->current_provider->zoom_min = ch.toInt();
			qDebug() << SG_PREFIX_I << "Zoom Min =" << this->current_provider->zoom_min;
		} else if (current == "ZoomMax") {
			this->current_provider->zoom_max = ch.toInt();
			qDebug() << SG_PREFIX_I << "Zoom Max =" << this->current_provider->zoom_max;
		} else {
			qDebug() << SG_PREFIX_W << "Unexpected tag inside CoverageArea:" << current;
		}
	} else if (parent == "BoundingBox") {
		if (current == "SouthLatitude") {
			this->current_provider->bbox.south = SGUtils::c_to_double(ch);
			qDebug() << SG_PREFIX_I << "South =" << this->current_provider->bbox.south;

		} else if (current == "WestLongitude") {
			this->current_provider->bbox.west = SGUtils::c_to_double(ch);
			qDebug() << SG_PREFIX_I << "West =" << this->current_provider->bbox.west;

		} else if (current == "NorthLatitude") {
			this->current_provider->bbox.north = SGUtils::c_to_double(ch);
			qDebug() << SG_PREFIX_I << "North =" << this->current_provider->bbox.north;

		} else if (current == "EastLongitude") {
			this->current_provider->bbox.east = SGUtils::c_to_double(ch);
			qDebug() << SG_PREFIX_I << "East =" << this->current_provider->bbox.east;

		} else {
			qDebug() << SG_PREFIX_W << "Unexpected tag inside BoundingBox:" << current;
		}
	} else {
		;
	}

	return true;
}




bool XMLHandlerBing::fatalError(const QXmlParseException & exception)
{
	qDebug() << SG_PREFIX_E << exception.message() << exception.lineNumber() << exception.columnNumber();
	return true;
}




/*
  @file passed to this function is an opened QTemporaryFile.
*/
sg_ret MapSourceBing::parse_file_for_providers(QFile & tmp_file)
{
	XMLHandlerBing handler(this);
	QXmlSimpleReader xml_reader;
	QXmlInputSource source(&tmp_file);
	xml_reader.setContentHandler(&handler);
	xml_reader.setErrorHandler(&handler);
	if (!xml_reader.parse(&source)) {
		qDebug() << SG_PREFIX_E << "Failed to parse xml file";
		return sg_ret::err;
	}

	if (true && vik_debug) {
		for (auto iter = this->providers.begin(); iter != this->providers.end(); iter++) {
			const BingImageryProvider * p = *iter;
			qDebug() << SG_PREFIX_D << "Bing Imagery Provider" << p->attribution
				 << ", zoom from" << p->zoom_min << "to" << p->zoom_max
				 << p->bbox;
		}
	}

	return sg_ret::ok;
}




sg_ret MapSourceBing::load_providers(void)
{
	sg_ret ret = sg_ret::ok;

	this->loading_providers = true;
	const QString uri = QString(URL_ATTR_FMT).arg(this->bing_api_key);

	DownloadHandle dl_handle(this->get_download_options());
	QTemporaryFile tmp_file;
	if (!dl_handle.download_to_tmp_file(tmp_file, uri)) {
		ret = sg_ret::err;
		goto done;
	}

	qDebug() << SG_PREFIX_D << "Load imagery providers from" << tmp_file.fileName();
	if (sg_ret::ok != this->parse_file_for_providers(tmp_file)) {
		ret = sg_ret::err;
	}

	tmp_file.remove();

done:
	this->loading_providers = false;
	return ret;
}




/* Function for loading imagery providers. */
void MapSourceBingProviders::run(void)
{
	this->map_source->load_providers();

	const bool end_job = this->set_progress_state(1);
	if (end_job) {
		return; /* Abort thread. */
	}

	/* Emit update. As we are on a download thread, it's better to
	   fire the update from the main loop. */
	emit ThisApp::get_layers_panel()->items_tree_updated();

	return;
}




void MapSourceBing::async_load_providers(void)
{
	MapSourceBingProviders * bg_job = new MapSourceBingProviders(this);
	bg_job->n_items = 1;
	bg_job->set_description(QObject::tr("Bing Image Providers Loading"));

	bg_job->run_in_background(ThreadPoolType::Remote);
}
