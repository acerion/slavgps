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
 *
 */

#include "bluemarble.h"
#include "vikmapslayer.h"
#include "vikslippymapsource.h"
#include "map_ids.h"



using namespace SlavGPS;



/* Initialization. */
void SlavGPS::bluemarble_init()
{
	MapSource * bluemarble_type = new MapSourceSlippy(MAP_ID_BLUE_MARBLE, "BlueMarble", "s3.amazonaws.com", "/com.modestmaps.bluemarble/%d-r%3$d-c%2$d.jpg");
	bluemarble_type->set_name((char *) "BlueMarble");
	bluemarble_type->zoom_min = 0;
	bluemarble_type->zoom_max = 9;
	bluemarble_type->set_copyright((char *) "© NASA's Earth Observatory");
	bluemarble_type->set_license((char *) "NASA Terms of Use");
	bluemarble_type->set_license_url((char *) "http://visibleearth.nasa.gov/useterms.php");

	/* Credit/Copyright from: http://earthobservatory.nasa.gov/Features/BlueMarble/ */
	/* BlueMarble image hosting is courtesy of the Modest Maps project: http://modestmaps.com/ */

	maps_layer_register_map_source(bluemarble_type);
}
