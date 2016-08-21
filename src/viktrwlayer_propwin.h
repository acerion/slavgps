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
#ifndef _SG_LAYER_TRW_PROPWIN_H_
#define _SG_LAYER_TRW_PROPWIN_H_




#include <cstdint>

#include <glib.h>
#include <gtk/gtk.h>

#include "viktrack.h"




#define VIK_TRW_LAYER_PROPWIN_SPLIT 1
#define VIK_TRW_LAYER_PROPWIN_REVERSE 2
#define VIK_TRW_LAYER_PROPWIN_DEL_DUP 3
#define VIK_TRW_LAYER_PROPWIN_SPLIT_MARKER 4




namespace SlavGPS {




	void vik_trw_layer_propwin_run(GtkWindow * parent,
				       LayerTRW * layer,
				       Track * trk,
				       void * panel,
				       Viewport * viewport,
				       bool start_on_stats);

	/**
	 * Update this property dialog e.g. if the track has been renamed
	 */
	void vik_trw_layer_propwin_update(Track * trk);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_LAYER_TRW_PROPWIN_H_ */
