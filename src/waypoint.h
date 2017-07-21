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
 */

#ifndef _SG_WAYPOINT_H_
#define _SG_WAYPOINT_H_




#include <cstdint>

#include <time.h>

#include <QPixmap>

#include "coord.h"
#include "globals.h"
#include "tree_view.h"
#include "layer.h"




namespace SlavGPS {




	class Waypoint : public Sublayer {
		Q_OBJECT
	public:
		Waypoint();
		Waypoint(const Waypoint& other);
		~Waypoint();


		void set_name(const QString & new_name);
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

		void convert(CoordMode dest_mode);


		Coord coord;
		bool visible = true;
		bool has_timestamp = false;
		time_t timestamp = 0;
		double altitude = VIK_DEFAULT_ALTITUDE;
		QString name;
		char * comment = NULL;
		char * description = NULL;
		char * source = NULL;
		char * type = NULL;
		char * url = NULL;
		char * image = NULL;
		/* A rather misleading, ugly hack needed for trwlayer's click image.
		   These are the height at which the thumbnail is being drawn, not the
		   dimensions of the original image. */
		uint8_t image_width;
		uint8_t image_height;

		char * symbol = NULL;
		/* Only for GUI display. */
		QPixmap * symbol_pixmap = NULL;
	};




} /* namespace SlavGPS */




Q_DECLARE_METATYPE(SlavGPS::Waypoint*)




#endif /* #ifndef _SG_WAYPOINT_H_ */
