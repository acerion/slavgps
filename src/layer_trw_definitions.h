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



enum {
	LAYER_TRW_TRACK_GRAPHICS_BLACK,
	LAYER_TRW_TRACK_GRAPHICS_SLOW,
	LAYER_TRW_TRACK_GRAPHICS_AVER,
	LAYER_TRW_TRACK_GRAPHICS_FAST,
	LAYER_TRW_TRACK_GRAPHICS_STOP,
	LAYER_TRW_TRACK_GRAPHICS_SINGLE,

	LAYER_TRW_TRACK_GRAPHICS_MAX
};




/* This #define is not related to enum above. */
#define LAYER_TRW_TRACK_COLORS_MAX 10




/* Note that using LayerTRWTrackDrawingMode::BY_SPEED may be slow especially for vast
   numbers of trackpoints as we are (re)calculating the color for
   every point. */
enum class LayerTRWTrackDrawingMode {
	BY_TRACK,
	BY_SPEED,
	ALL_SAME_COLOR
};




/* These symbols are used for Waypoints, but they can be used elsewhere too. */
enum class GraphicMarker {
	/* There were four markers originally in Viking. */
	FILLED_SQUARE,
	SQUARE,
	CIRCLE,
	X,

	MAX
};




#endif /* #ifndef _SG_LAYER_TRW_DEFINITIONS_H_ */
