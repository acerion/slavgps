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




#include "variant.h"
#include "terraserver.h"
#include "layer_map_source_terraserver.h"
#include "layer_map.h"




using namespace SlavGPS;




#ifdef VIK_CONFIG_TERRASERVER
MapSource * map_source_maker_terraserver_topos(void)
{
	return new MapSourceTerraserver(MapTypeID::TerraserverTopo, "Terraserver Topo");
}
MapSource * map_source_maker_terraserver_aerials(void)
{
	return new MapSourceTerraserver(MapTypeID::TerraserverAerial, "Terraserver Aerials");
}
MapSource * map_source_maker_terraserver_urban(void)
{
	return new MapSourceTerraserver(MapTypeID::TerraserverUrban, "Terraserver Urban Areas");
}
#endif




void Terraserver::init(void)
{
#ifdef VIK_CONFIG_TERRASERVER
	MapSources::register_map_source_maker(map_source_maker_terraserver_topos, MapTypeID::TerraserverTopo, "Terraserver Topo");
	MapSources::register_map_source_maker(map_source_maker_terraserver_aerials, MapTypeID::TerraserverAerial, "Terraserver Aerial");
	MapSources::register_map_source_maker(map_source_maker_terraserver_urban, MapTypeID::TerraserverUrban, "Terraserver Urban Areas");
#endif
}
