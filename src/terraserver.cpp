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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "variant.h"
#include "terraserver.h"
#include "map_source_terraserver.h"
#include "layer_map.h"
#include "map_ids.h"




using namespace SlavGPS;




void SlavGPS::terraserver_init()
{
	MapSources::register_map_source(new MapSourceTerraserver(MapTypeID::TERRASERVER_TOPO, "Terraserver Topos"));
	MapSources::register_map_source(new MapSourceTerraserver(MapTypeID::TERRASERVER_AERIAL, "Terraserver Aerials"));
	MapSources::register_map_source(new MapSourceTerraserver(MapTypeID::TERRASERVER_URBAN, "Terraserver Urban Areas"));
}
