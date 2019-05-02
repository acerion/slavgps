/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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




#include <cmath>
#include <limits>




#include <QDebug>
#include <QXmlSimpleReader>
#include <QXmlInputSource>




#include "goto_tool_xml.h"
#include "vikutils.h"
#include "globals.h"




/* TODO_2_LATER:

   1. the implementation captures only the first result returned by
      server.  If the server returns more than one location of a name,
      they are ignored. Perhaps we could present them to user so that
      he knows about them and can select from them.

   2. improve error handling by MyHandler class.

   3. improve error handling on failed connections/downloads/queries.
*/




using namespace SlavGPS;




#define SG_MODULE "GoTo XML"




MyHandler::MyHandler(const QString & new_lat_path, const QString & new_lon_path)
{
	/* We will know that we found lat/lon values when, during
	   traversal of xml tree, we descend into place with given
	   "path", i.e. list of nested xml tags. */
	this->lat_path = QString(new_lat_path).split('/');
	this->lon_path = QString(new_lon_path).split('/');

	qDebug() << SG_PREFIX_I << "latitude path =" << this->lat_path;
	qDebug() << SG_PREFIX_I << "longitude path =" << this->lon_path;

	this->use_attributes = false;

	this->ll.lat = std::numeric_limits<double>::quiet_NaN();
	this->ll.lon = std::numeric_limits<double>::quiet_NaN();
}




MyHandler::MyHandler(const QString & new_lat_path, const QString & new_lat_attr, const QString & new_lon_path, const QString & new_lon_attr)
{
	/* We will know that we found lat/lon values when, during
	   traversal of xml tree, we descend into place with given
	   "path", i.e. list of nested xml tags. */
	this->lat_path = QString(new_lat_path).split('/');
	this->lon_path = QString(new_lon_path).split('/');

	qDebug() << SG_PREFIX_I << "latitude path =" << this->lat_path;
	qDebug() << SG_PREFIX_I << "longitude path =" << this->lon_path;

	this->lat_attr = new_lat_attr;
	this->lon_attr = new_lon_attr;
	this->use_attributes = true;

	this->ll.lat = std::numeric_limits<double>::quiet_NaN();
	this->ll.lon = std::numeric_limits<double>::quiet_NaN();
}




bool MyHandler::startDocument(void)
{
	qDebug() << SG_PREFIX_I;
	return true;
}




bool MyHandler::endDocument(void)
{
	qDebug() << SG_PREFIX_I;
	return true;
}




/*
  Updates element stack.
  Extracts lat/lon from element attributes.
*/
bool MyHandler::startElement(const QString &namespaceURI, const QString &localName, const QString &qName, const QXmlAttributes &atts)
{
	this->stack.push_back(localName);

	qDebug() << SG_PREFIX_I << "localName =" << localName << "qName =" << qName;
	qDebug() << SG_PREFIX_I << "stack after pushing =" << this->stack;

	if (this->use_attributes) {
#if 1 /* Debug. */
		if (this->stack == this->lat_path || this->stack == this->lon_path) {
			for (int i = 0; i < atts.count(); i++) {
				qDebug() << SG_PREFIX_I << "         attribute" << i << atts.localName(i) << atts.value(i);
			}
		}
#endif

		/*
		  In case of Nominatim server the path to Latitude and
		  Longitude is exactly the same. In fact both lat and
		  lon are attributes of the same element
		  entry. Remember about this when parsing data.

		  Below we use std::isnan() to see whether some
		  location/result hasn't been captured before, and we
		  reject 2nd, 3rd, n-th result.
		*/

		if (this->stack == this->lat_path) {
			if (std::isnan(this->ll.lat)) {
				this->ll.lat = SGUtils::c_to_double(atts.value("", this->lat_attr));
			}
			qDebug() << SG_PREFIX_I << "---- found latitude =" << this->ll.lat;

		}
		if (this->stack == this->lon_path) {
			if (std::isnan(this->ll.lon)) {
				this->ll.lon = SGUtils::c_to_double(atts.value("", this->lon_attr));
			}
			qDebug() << SG_PREFIX_I << "---- found longitude =" << this->ll.lon;
		}
	}

	return true;
}




bool MyHandler::endElement(const QString &namespaceURI, const QString &localName, const QString &qName)
{
	qDebug() << SG_PREFIX_I << "localName =" << localName << "qName =" << qName;
	this->stack.pop_back();
	qDebug() << SG_PREFIX_D << "Stack after popping =" << this->stack;
	return true;
}




/* Extracts lat/lon from character data. */
bool MyHandler::characters(const QString & ch)
{
	if (this->use_attributes) {
		return true;
	}

	/*
	  Below we use std::isnan() to see whether some
	  location/result hasn't been captured before, and we reject
	  2nd, 3rd, n-th result.
	*/

	if (this->stack == this->lat_path) {
		if (std::isnan(this->ll.lat)) {
			this->ll.lat = SGUtils::c_to_double(ch);
		}
		qDebug() << SG_PREFIX_I << "---- found latitude =" << this->ll.lat;

	} else if (this->stack == this->lon_path) {
		if (std::isnan(this->ll.lon)) {
			this->ll.lon = SGUtils::c_to_double(ch);
		}
		qDebug() << SG_PREFIX_I << "---- found longitude =" << this->ll.lat;
	} else {
		qDebug() << SG_PREFIX_I << "found other characters string =" << ch;
	}

	return true;
}




bool MyHandler::fatalError(const QXmlParseException & exception)
{
	qDebug() << SG_PREFIX_E << exception.message() << exception.lineNumber() << exception.columnNumber();
	return true;
}




GotoToolXML::GotoToolXML()
{
	this->url_format = strdup("<no-set>");
}




GotoToolXML::GotoToolXML(const QString & new_label, const QString & new_url_format, const QString & new_lat_path, const QString & new_lon_path)
{
	this->label = new_label;
	this->set_url_format(new_url_format);

	this->xml_handler = new MyHandler(new_lat_path, new_lon_path);
}




GotoToolXML::GotoToolXML(const QString & new_label, const QString & new_url_format, const QString & new_lat_path, const QString & new_lat_attr, const QString & new_lon_path, const QString & new_lon_attr)
{
	this->label = new_label;
	this->set_url_format(new_url_format);

	this->xml_handler = new MyHandler(new_lat_path, new_lat_attr, new_lon_path, new_lon_attr);
}




GotoToolXML::~GotoToolXML()
{
	delete this->xml_handler;
}




void GotoToolXML::set_url_format(const QString & new_url_format)
{
	this->url_format = new_url_format;
}




QString GotoToolXML::get_url_format(void) const
{
	return this->url_format;
}




bool GotoToolXML::parse_file_for_latlon(QFile & file, LatLon & lat_lon)
{
	QXmlSimpleReader xml_reader;
	QXmlInputSource source(&file);
	xml_reader.setContentHandler(this->xml_handler);
	xml_reader.setErrorHandler(this->xml_handler);

	/* We don't re-create xml handler on every query, so clear old found location. */
	this->xml_handler->ll.lat = std::numeric_limits<double>::quiet_NaN();
	this->xml_handler->ll.lon = std::numeric_limits<double>::quiet_NaN();


	if (!xml_reader.parse(&source)) {
		qDebug() << SG_PREFIX_E << "Failed to parse xml file";
		return false;
	}

	if (std::isnan(this->xml_handler->ll.lat) || std::isnan(this->xml_handler->ll.lat)) {
		/* At least one coordinate not found */
		return false;
	}

	lat_lon = this->xml_handler->ll;

	return true;
}
