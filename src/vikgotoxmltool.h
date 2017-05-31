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
 *
 */
#ifndef __SG_GOTO_TOOL_XML_H
#define __SG_GOTO_TOOL_XML_H





#include "goto_tool.h"
#include "coords.h"




namespace SlavGPS {





	class GotoToolXML : public GotoTool {

	public:

		GotoToolXML();
		GotoToolXML(char const * new_label, char const * new_url_format, char const * new_lan_path, char const * new_lon_path);
		GotoToolXML(char const * new_label, char const * new_url_format, char const * new_lat_path, char const * new_lat_attr, char const * new_lon_path, char const * new_lon_attr);
		~GotoToolXML();

		char * get_url_format();
		bool parse_file_for_latlon(char * filename, struct LatLon * ll);


		void set_url_format(char const * new_format);
		void set_lat_path(char const * new_value);
		void set_lat_attr(char const * new_value);
		void set_lon_path(char const * new_value);
		void set_lon_attr(char const * new_value);


		/* This should be private. */
		char * url_format; /* The format of the URL */
		char * lat_path;   /* XPath of the latitude */
		char * lat_attr;   /* XML attribute of the latitude. */
		char * lon_path;   /* XPath of the longitude. */
		char * lon_attr;   /* XML attribute of the longitude. */

		struct LatLon ll;

	}; /* class GotoToolXML */





} /* namespace SlavGPS */





#endif /* #ifndef __SG_GOTO_TOOL_XML_H */
