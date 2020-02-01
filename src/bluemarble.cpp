/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2008, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
#include "bluemarble.h"
#include "layer_map.h"
#include "map_source_slippy.h"




using namespace SlavGPS;




#ifdef VIK_CONFIG_BLUEMARBLE
MapSource * map_source_maker_blue_marble(void)
{
	MapSource * bluemarble_type = new MapSourceSlippy(MapTypeID::BlueMarble, "BlueMarble", "s3.amazonaws.com", "/com.modestmaps.bluemarble/%d-r%3$d-c%2$d.jpg");
	bluemarble_type->set_map_type_string("BlueMarble"); /* Non-translatable. */
	bluemarble_type->set_supported_tile_zoom_level_range(0, 9);
	bluemarble_type->set_copyright("© NASA's Earth Observatory");
	bluemarble_type->set_license("NASA Terms of Use");
	bluemarble_type->set_license_url("http://visibleearth.nasa.gov/useterms.php");

	/* Credit/Copyright from: http://earthobservatory.nasa.gov/Features/BlueMarble/ */
	/* BlueMarble image hosting is courtesy of the Modest Maps project: http://modestmaps.com/ */

	return bluemarble_type;
}
#endif




/* Initialization. */
void BlueMarble::init(void)
{
#ifdef VIK_CONFIG_BLUEMARBLE
	MapSources::register_map_source_maker(map_source_maker_blue_marble, MapTypeID::BlueMarble, "BlueMarble");
#endif
}
