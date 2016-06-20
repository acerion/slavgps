/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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
#ifndef _VIKING_TRWLAYER_TRACKLIST_H
#define _VIKING_TRWLAYER_TRACKLIST_H

#include <stdbool.h>
#include <stdint.h>

#include "viktrack.h"
#include "viktrwlayer.h"

#ifdef __cplusplus
extern "C" {
#endif


void vik_trw_layer_track_list_show_dialog(char * title,
										  SlavGPS::Layer * vl,
                                            void * user_data,
                                            VikTrwlayerGetTracksAndLayersFunc get_tracks_and_layers_cb,
                                            bool is_aggregate );

#ifdef __cplusplus
}
#endif

#endif
