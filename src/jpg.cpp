/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
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
#include "viktrwlayer.h"
#include "viklayerspanel.h"
#include "globals.h"
#ifdef VIK_CONFIG_GEOTAG
#include "geotag_exif.h"
#endif
#ifdef HAVE_MAGIC_H
#include <magic.h>
#endif

#include <stdlib.h>

using namespace SlavGPS;

/**
 * a_jpg_magic_check:
 * @filename: The file
 *
 * Returns: Whether the file is a JPG.
 *  Uses Magic library if available to determine the jpgness.
 *  Otherwise uses a rudimentary extension name check.
 */
bool a_jpg_magic_check ( const char *filename )
{
	bool is_jpg = false;
#ifdef HAVE_MAGIC_H
	magic_t myt = magic_open ( MAGIC_CONTINUE|MAGIC_ERROR|MAGIC_MIME );
	if ( myt ) {
#ifdef WINDOWS
		// We have to 'package' the magic database ourselves :(
		//  --> %PROGRAM FILES%\Viking\magic.mgc
		magic_load ( myt, "magic.mgc" );
#else
		// Use system default
		magic_load ( myt, NULL );
#endif
		const char* magic = magic_file (myt, filename);
		fprintf(stderr, "DEBUG: %s:%s\n", __FUNCTION__, magic );
		if ( g_ascii_strncasecmp (magic, "image/jpeg", 10) == 0 )
			is_jpg = true;

		magic_close ( myt );
	}
	else
#endif
		is_jpg = a_file_check_ext ( filename, ".jpg" );

	return is_jpg;
}

/**
 * Load a single JPG into a Trackwaypoint Layer as a waypoint
 *
 * @top:      The Aggregate layer that a new TRW layer may be created in
 * @filename: The JPG filename
 * @vvp:      The viewport
 *
 * Returns: Whether the loading was a success or not
 *
 * If the JPG has geotag information then the waypoint will be created with the appropriate position.
 *  Otherwise the waypoint will be positioned at the current screen center.
 * If a TRW layer is already selected the waypoint will be created in that layer.
 */
bool a_jpg_load_file ( VikAggregateLayer *top, const char *filename, VikViewport *vvp )
{
	bool auto_zoom = true;
	VikWindow *vw = (VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(VIK_LAYER(top)));
	VikLayersPanel *vlp = vik_window_layers_panel ( vw );
	// Auto load into TrackWaypoint layer if one is selected
	VikLayer *vtl = vlp->panel_ref->get_selected();

	bool create_layer = false;
	if ( vtl == NULL || ((Layer *) vtl->layer)->type != VIK_LAYER_TRW ) {
		// Create layer if necessary

		vtl = vik_layer_create ( VIK_LAYER_TRW, &vvp->port, false );
		((VikTrwLayer *) vtl)->trw->rename(a_file_basename ( filename ) );
		create_layer = true;
	}

	char *name = NULL;
	Waypoint * wp = NULL;
#ifdef VIK_CONFIG_GEOTAG
	wp = a_geotag_create_waypoint_from_file ( filename, vvp->port.get_coord_mode(), &name );
#endif
	if ( wp ) {
		// Create name if geotag method didn't return one
		if ( !name )
			name = g_strdup( a_file_basename ( filename ) );
		(VIK_TRW_LAYER(vtl))->trw->filein_add_waypoint(name, wp);
		free( name );
	}
	else {
		wp = new Waypoint();
		wp->visible = true;
		(VIK_TRW_LAYER(vtl))->trw->filein_add_waypoint((char *) a_file_basename(filename), wp);
		wp->set_image(filename);
		// Simply set position to the current center
		wp->coord = *(vvp->port.get_center());
		auto_zoom = false;
	}

	// Complete the setup
	vik_layer_post_read ( vtl, &vvp->port, true );
	if ( create_layer ) {
		Layer * layer = (Layer *) vtl->layer;
		((LayerAggregate *) ((VikLayer *) top)->layer)->add_layer(layer, false);
	}

	if ( auto_zoom )
		(VIK_TRW_LAYER(vtl))->trw->auto_set_view(&vvp->port);

	// ATM This routine can't fail
	return true;
}
