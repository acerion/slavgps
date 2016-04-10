/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _VIKING_WAYPOINT_H
#define _VIKING_WAYPOINT_H

#include <stdbool.h>
#include <stdint.h>

#include "vikcoord.h"

#include <gdk-pixbuf/gdk-pixdata.h>





namespace SlavGPS {

	class Waypoint {

	public:
		Waypoint();
		~Waypoint();

		Waypoint(const Waypoint& other);

		void set_name(char const * name);
		void set_comment(char const * comment);
		void set_description(char const * description);
		void set_source(char const * source);
		void set_type(char const * type);
		void set_url(char const * url);
		void set_image(char const * image);
		void set_symbol(char const * symname);

		void set_comment_no_copy(char * comment);
		bool apply_dem_data( bool skip_existing );


		void marshall(uint8_t ** data, size_t * len);
		static Waypoint * unmarshall(uint8_t * data, size_t datalen);

		static void delete_waypoint(Waypoint *);


		VikCoord coord;
		bool visible;
		bool has_timestamp;
		time_t timestamp;
		double altitude;
		char * name;
		char * comment;
		char * description;
		char * source;
		char * type;
		char * url;
		char * image;
		/* a rather misleading, ugly hack needed for trwlayer's click image.
		 * these are the height at which the thumbnail is being drawn, not the
		 * dimensions of the original image. */
		uint8_t image_width;
		uint8_t image_height;
		char * symbol;
		// Only for GUI display
		GdkPixbuf * symbol_pixbuf;
	};





} /* namespace */





#endif /* #ifndef _VIKING_WAYPOINT_H */
