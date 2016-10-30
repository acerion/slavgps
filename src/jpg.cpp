/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
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

#include "jpg.h"
#include "file.h"
#include "fileutils.h"
#include "layer_trw.h"
#include "window.h"
#include "globals.h"
#ifdef VIK_CONFIG_GEOTAG
#include "geotag_exif.h"
#endif

#ifdef HAVE_MAGIC_H
#include <magic.h>
#endif

#include <cstdlib>
#include <cstring>




using namespace SlavGPS;




/**
 * @filename: The file
 *
 * Returns: Whether the file is a JPG.
 * Uses Magic library if available to determine the jpgness.
 * Otherwise uses a rudimentary extension name check.
 */
bool SlavGPS::jpg_magic_check(char const * filename)
{
	bool is_jpg = false;
#ifdef HAVE_MAGIC_H
	magic_t myt = magic_open(MAGIC_CONTINUE|MAGIC_ERROR|MAGIC_MIME);
	if (myt) {
#ifdef WINDOWS
		/* We have to 'package' the magic database ourselves :(
		   --> %PROGRAM FILES%\Viking\magic.mgc */
		magic_load(myt, "magic.mgc");
#else
		/* Use system default. */
		magic_load(myt, NULL);
#endif
		char const * magic = magic_file(myt, filename);
		fprintf(stderr, "DEBUG: %s:%s\n", __FUNCTION__, magic);
		if (g_ascii_strncasecmp(magic, "image/jpeg", 10) == 0) {
			is_jpg = true;
		}

		magic_close(myt);
	} else
#endif
	{
		is_jpg = a_file_check_ext(filename, ".jpg");
	}


	return is_jpg;
}




/**
 * Load a single JPG into a Trackwaypoint Layer as a waypoint.
 *
 * @top:      The Aggregate layer that a new TRW layer may be created in
 * @filename: The JPG filename
 * @viewport: The viewport
 *
 * Returns: Whether the loading was a success or not.
 *
 * If the JPG has geotag information then the waypoint will be created with the appropriate position.
 * Otherwise the waypoint will be positioned at the current screen center.
 * If a TRW layer is already selected the waypoint will be created in that layer.
 */
bool SlavGPS::jpg_load_file(LayerAggregate * top, char const * filename, Viewport * viewport)
{
	bool auto_zoom = true;
	/* Auto load into TrackWaypoint layer if one is selected. */
	Layer * trw = top->get_window()->get_layers_panel()->get_selected_layer();

	bool create_layer = false;
	if (trw == NULL || trw->type != LayerType::TRW) {
		/* Create layer if necessary. */

		trw = (LayerTRW *) new LayerTRW(viewport);
		trw->rename(file_basename(filename));
		create_layer = true;
	}

	char * name = NULL;
	Waypoint * wp = NULL;
#ifdef VIK_CONFIG_GEOTAG
	wp = a_geotag_create_waypoint_from_file(filename, viewport->get_coord_mode(), &name);
#endif
	if (wp) {
		/* Create name if geotag method didn't return one. */
		if (!name) {
			name = strdup(file_basename(filename));
		}
		((LayerTRW *) trw)->filein_add_waypoint(name, wp);
		free(name);
	} else {
		wp = new Waypoint();
		wp->visible = true;
		((LayerTRW *) trw)->filein_add_waypoint((char *) file_basename(filename), wp);
		wp->set_image(filename);
		/* Simply set position to the current center. */
		wp->coord = *(viewport->get_center());
		auto_zoom = false;
	}

	/* Complete the setup. */
	trw->post_read(viewport, true);
	if (create_layer) {
		top->add_layer(trw, false);
	}

	if (auto_zoom) {
		((LayerTRW *) trw)->auto_set_view(viewport);
	}

	/* ATM This routine can't fail. */
	return true;
}
