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

#ifndef _SG_GOTO_TOOL_XML_H_
#define _SG_GOTO_TOOL_XML_H_




#include <QStringList>
#include <QXmlDefaultHandler>




#include "goto_tool.h"
#include "coords.h"




namespace SlavGPS {




	class MyHandler : public QXmlDefaultHandler {
	public:
		MyHandler(const QString & lat_path, const QString & lon_path);

		bool startDocument(void);
		bool endDocument(void);
		bool startElement(const QString &namespaceURI, const QString &localName, const QString &qName, const QXmlAttributes &atts);
		bool endElement(const QString &namespaceURI, const QString &localName, const QString &qName);
		bool characters(const QString & ch);
		bool fatalError(const QXmlParseException & exception);

		/* Stack to which all currently opened xml tags are
		   pushed.  I'm using QStringList to implement stack
		   because lat_path_ and lon_path_ are also
		   QStringList. */
		QStringList stack;

		QStringList lat_path;
		QStringList lon_path;
	};




	class GotoToolXML : public GotoTool {

	public:

		GotoToolXML();
		GotoToolXML(const QString & new_label, char const * new_url_format, const QString & new_lan_path, const QString & new_lon_path);
		GotoToolXML(const QString & new_label, char const * new_url_format, char const * new_lat_path, char const * new_lat_attr, char const * new_lon_path, char const * new_lon_attr);
		~GotoToolXML();

		char * get_url_format();
		bool parse_file_for_latlon(const QString & file_full_path, LatLon & lat_lon);


		void set_url_format(char const * new_format);
		void set_lat_path(char const * new_value);
		void set_lat_attr(char const * new_value);
		void set_lon_path(char const * new_value);
		void set_lon_attr(char const * new_value);


		/* This should be private. */
		char * url_format = NULL; /* The format of the URL */
		char * lat_path = NULL;   /* XPath of the latitude */
		char * lat_attr = NULL;   /* XML attribute of the latitude. */
		char * lon_path = NULL;   /* XPath of the longitude. */
		char * lon_attr = NULL;   /* XML attribute of the longitude. */

		LatLon ll;

		MyHandler * xml_handler = NULL;

	}; /* class GotoToolXML */





} /* namespace SlavGPS */





#endif /* #ifndef _SG_GOTO_TOOL_XML_H_ */
