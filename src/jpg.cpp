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
#include "file_utils.h"
#include "layer_trw.h"
#include "window.h"
#include "layer_aggregate.h"
#include "viewport_internal.h"
#include "layers_panel.h"
#include "globals.h"
#include "vikutils.h"
#ifdef VIK_CONFIG_GEOTAG
#include "geotag_exif.h"
#endif

#ifdef HAVE_MAGIC_H
#include <magic.h>
#endif

#include <cstdlib>
#include <cstring>




using namespace SlavGPS;




extern Tree * g_tree;




/**
   Returns: Whether the file is a JPG.
   Uses Magic library if available to determine the jpgness.
   Otherwise uses a rudimentary extension name check.
*/
bool SlavGPS::jpg_magic_check(const QString & file_full_path)
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
		char const * magic = magic_file(myt, file_full_path.toUtf8().constData());
		fprintf(stderr, "DEBUG: %s:%s\n", __FUNCTION__, magic);
		if (g_ascii_strncasecmp(magic, "image/jpeg", 10) == 0) {
			is_jpg = true;
		}

		magic_close(myt);
	} else
#endif
	{
		is_jpg = FileUtils::has_extension(file_full_path, ".jpg");
	}


	return is_jpg;
}




/**
   @brief Load a single JPG into a Trackwaypoint Layer as a waypoint.

   @parent_layer: The Aggregate layer that a new TRW layer may be created in
   @file_full_path: full path to JPG file
   @viewport: The viewport

   If the JPG has geotag information then the waypoint will be created with the appropriate position.
   Otherwise the waypoint will be positioned at the current screen center.
   If a TRW layer is already selected the waypoint will be created in that layer.

   Returns: Whether the loading was a success or not.
*/
bool SlavGPS::jpg_load_file(LayerAggregate * parent_layer, Viewport * viewport, const QString & file_full_path)
{
	bool auto_zoom = true;
	/* Auto load into TrackWaypoint layer if one is selected. */
	Layer * layer = g_tree->tree_get_items_tree()->get_selected_layer();
	LayerTRW * trw = NULL;

	bool create_layer = false;
	if (layer == NULL || layer->type != LayerType::TRW) {
		/* Create layer if necessary. */

		trw = (LayerTRW *) new LayerTRW();
		trw->set_coord_mode(viewport->get_coord_mode());
		trw->set_name(FileUtils::get_base_name(file_full_path));
		create_layer = true;
	} else {
		trw = (LayerTRW *) layer;
	}

	QString waypoint_name;
	Waypoint * wp = NULL;

#ifdef VIK_CONFIG_GEOTAG
	wp = a_geotag_create_waypoint_from_file(file_full_path, viewport->get_coord_mode(), waypoint_name);
#endif
	if (wp) {
		/* Create name if geotag method didn't return one. */
		if (waypoint_name.isEmpty()) {
			waypoint_name = FileUtils::get_base_name(file_full_path);
		}
		trw->add_waypoint_from_file(wp, waypoint_name);
	} else {
		wp = new Waypoint();
		wp->visible = true;
		trw->add_waypoint_from_file(wp, file_base_name(file_full_path));
		wp->set_image(file_full_path);
		/* Simply set position to the current center. */
		wp->coord = *(viewport->get_center());
		auto_zoom = false;
	}

	/* Complete the setup. */
	trw->post_read(viewport, true);
	if (create_layer) {
		parent_layer->add_layer(trw, false);
	}

	if (auto_zoom) {
		trw->auto_set_view(viewport);
	}

	/* ATM This routine can't fail. */
	return true;
}
