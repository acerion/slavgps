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

#ifndef _VIKING_JPG_H
#define _VIKING_JPG_H

#include <stdbool.h>
#include <stdint.h>

//#include "viking.h"
#include "vikaggregatelayer.h"

#ifdef __cplusplus
extern "C" {
#endif


bool a_jpg_magic_check(char const * filename);
bool a_jpg_load_file(SlavGPS::LayerAggregate * top, char const * filename, SlavGPS::Viewport * viewport);

#ifdef __cplusplus
}
#endif

#endif
