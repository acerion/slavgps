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

#ifndef _SG_GPSMAPPER_H_
#define _SG_GPSMAPPER_H_




#include <list>




#include <QFile>




#include "globals.h"




namespace SlavGPS {




	class LayerTRW;
	class Track;
	class Waypoint;




	class GPSMapper {
	public:
		static SaveStatus write_layer_to_file(FILE * file, LayerTRW * trw);

	private:
		static SaveStatus write_tracks_to_file(FILE * file, const std::list<Track *> & tracks);
		static SaveStatus write_waypoints_to_file(FILE * file, const std::list<Waypoint *> & waypoints);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_GPSMAPPER_H_ */
