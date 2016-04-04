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
#ifndef __VIKING_LAYER_DEFAULTS_H
#define __VIKING_LAYER_DEFAULTS_H

#include <stdbool.h>
#include <stdint.h>

#include "uibuilder.h"

#ifdef __cplusplus
extern "C" {
#endif


void a_layer_defaults_init();
void a_layer_defaults_uninit();

void a_layer_defaults_register ( VikLayerParam *vlp, VikLayerParamData defaultval, const char *layername );

bool a_layer_defaults_show_window ( GtkWindow *parent, const char *layername );

VikLayerParamData a_layer_defaults_get ( const char *layername, const char *param_name, VikLayerParamType param_type );

bool a_layer_defaults_save ();

#ifdef __cplusplus
}
#endif

#endif
