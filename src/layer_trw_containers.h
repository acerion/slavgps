/*
 * SlavGPS -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * A fork of viking, project started in 2016.
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2011-2013, Rob Norris <rw_norris@hotmail.com>
 * Copyright (c) 2016 Kamil Ignacak <acerion@wp.pl>
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

#ifndef _SG_LAYER_TRW_CONTAINERS_H_
#define _SG_LAYER_TRW_CONTAINERS_H_




#include <list>
#include <unordered_map>

#include "waypoint.h"
#include "layer.h"
#include "track.h"




namespace SlavGPS {




	/* Comment from viking, perhaps still applies:

	   It's not entirely clear the benefits of hash tables usage
	   here - possibly the simplicity of first implementation for
	   unique names.  Now with the name of the item stored as part
	   of the item - these tables are effectively straightforward
	   lists.

	   For this reworking I've choosen to keep the use of hash
	   tables since for the expected data sizes - even many hundreds
	   of waypoints and tracks is quite small in the grand scheme of
	   things, and with normal PC processing capabilities - it has
	   negligible performance impact.  This also minimized the
	   amount of rework - as the management of the hash tables
	   already exists.

	   The hash tables are indexed by simple integers acting as a
	   UUID hash, which again shouldn't affect performance much we
	   have to maintain a uniqueness (as before when multiple names
	   where not allowed), this is to ensure it refers to the same
	   item in the data structures used on the viewport and on the
	   layers panel.
	*/
	typedef std::unordered_map<sg_uid_t, Track *> Tracks;
	typedef std::unordered_map<sg_uid_t, Waypoint *> Waypoints;







	class LayerTRWc {
	public:
	};




} /* namespace SlavGSP */




#endif /* #ifndef _SG_LAYER_TRW_CONTAINERS_H_ */
