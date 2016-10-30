/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (c) 2014, Rob Norris <rw_norris@hotmail.com>
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
#ifndef _SG_WAYPOINT_PARAMETERS_H_
#define _SG_WAYPOINT_PARAMETERS_H_



#include <QWidget>

#include "layer_trw.h"
#include "waypoint.h"




namespace SlavGPS {




	enum {
		SG_WP_PARAM_NAME,
		SG_WP_PARAM_LAT,
		SG_WP_PARAM_LON,
		SG_WP_PARAM_TIME,
		SG_WP_PARAM_ALT,
		SG_WP_PARAM_COMMENT,
		SG_WP_PARAM_DESC,
		SG_WP_PARAM_IMAGE,
		SG_WP_PARAM_SYMBOL
	};




	char * waypoint_properties_dialog(QWidget * parent, char * default_name, LayerTRW * trw, Waypoint * wp, VikCoordMode coord_mode, bool is_new, bool * updated);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_WAYPOINT_PARAMETERS_H_ */
