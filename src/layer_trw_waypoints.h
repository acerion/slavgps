/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2011-2013, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_LAYER_WAYPOINTS_H_
#define _SG_LAYER_WAYPOINTS_H_




#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

//#include <cstdint>
//#include <list>
//#include <unordered_map>

//#include <QStandardItem>
//#include <QDialog>

//#include "layer.h"
//#include "layer_tool.h"
//#include "layer_interface.h"
//#include "layer_trw_containers.h"
//#include "layer_trw_dialogs.h"
//#include "viewport.h"
//#include "waypoint.h"
//#include "trackpoint_properties.h"
//#include "file.h"
#include "tree_view.h"
#include "layer_trw_containers.h"




namespace SlavGPS {




	class LayerTRWWaypoints : public TreeItem {
		Q_OBJECT
	public:
		LayerTRWWaypoints();
		~LayerTRWWaypoints();

		Waypoints waypoints;

	};




}




#endif /* #ifndef _SG_LAYER_WAYPOINTS_H_ */
