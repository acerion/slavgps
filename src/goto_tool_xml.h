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
		MyHandler(const QString & new_lat_path, const QString & new_lat_attr, const QString & new_lon_path, const QString & new_lon_attr);

		bool startDocument(void);
		bool endDocument(void);
		bool startElement(const QString &namespaceURI, const QString &localName, const QString &qName, const QXmlAttributes &atts);
		bool endElement(const QString &namespaceURI, const QString &localName, const QString &qName);
		bool characters(const QString & ch);
		bool fatalError(const QXmlParseException & exception);

		LatLon ll;

	private:
		/* Stack to which all currently opened xml tags are
		   pushed.  I'm using QStringList to implement stack
		   because lat_path_ and lon_path_ are also
		   QStringList. */
		QStringList stack;

		QStringList lat_path; /* XPath of the latitude. */
		QStringList lon_path; /* XPath of the longitude. */

		QString lat_attr; /* XML attribute of the latitude. */
		QString lon_attr; /* XML attribute of the longitude. */

		bool use_attributes = false;
	};




	class GotoToolXML : public GotoTool {
	public:
		GotoToolXML();
		GotoToolXML(const QString & new_label, char const * new_url_format, const QString & new_lan_path, const QString & new_lon_path);
		GotoToolXML(const QString & new_label, char const * new_url_format, const QString & new_lat_path, const QString & new_lat_attr, const QString & new_lon_path, const QString & new_lon_attr);
		~GotoToolXML();

		char * get_url_format(void) const;
		void set_url_format(char const * new_format);

		bool parse_file_for_latlon(const QString & file_full_path, LatLon & lat_lon);

		/* This should be private. */
		char * url_format = NULL; /* The format of the URL */

		MyHandler * xml_handler = NULL;

	}; /* class GotoToolXML */





} /* namespace SlavGPS */





#endif /* #ifndef _SG_GOTO_TOOL_XML_H_ */
