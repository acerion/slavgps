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

#ifndef _SG_LAYER_TRW_DEFINITIONS_H_
#define _SG_LAYER_TRW_DEFINITIONS_H_




/* Common file for some definitions to avoid dependencies between
   header files. */




/* Note that using LayerTRWTrackDrawingMode::BySpeed may be slow especially for vast
   numbers of trackpoints as we are (re)calculating the color for
   every point. */
enum class LayerTRWTrackDrawingMode {
	ByTrack,
	BySpeed,
	AllSameColor
};




/* These symbols are used for Waypoints, but they can be used elsewhere too. */
enum class GraphicMarker {
	/* There were four markers originally in Viking. */
	FilledSquare,
	Square,
	Circle,
	X,

	Max
};



/* See http://developer.gnome.org/pango/stable/PangoMarkupFormat.html */
typedef enum {
	FS_XX_SMALL = 0,
	FS_X_SMALL,
	FS_SMALL,
	FS_MEDIUM, // DEFAULT
	FS_LARGE,
	FS_X_LARGE,
	FS_XX_LARGE,
	FS_NUM_SIZES
} font_size_t;




#endif /* #ifndef _SG_LAYER_TRW_DEFINITIONS_H_ */
