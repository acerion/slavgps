/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
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
#ifndef _SG_PREFERENCES_H_
#define _SG_PREFERENCES_H_




#include <cstdint>

#include <QWindow>

#include "uibuilder.h"



/* TODO IMPORTANT!!!! add REGISTER_GROUP !!! OR SOMETHING!!! CURRENTLY GROUPLESS!!! */




void a_preferences_init();
void a_preferences_uninit();

/* Pref should be persistent thruout the life of the preference. */


/* Must call FIRST. */
void a_preferences_register_group(const char * key, const char * name);

/* Nothing in pref is copied neither but pref itself is copied. (TODO: COPY EVERYTHING IN PREF WE NEED, IF ANYTHING),
   so pref key is not copied. default param data IS copied. */
/* Group field (integer) will be overwritten. */
void a_preferences_register(LayerParam * pref, LayerParamData defaultval, const char * group_key);

void a_preferences_show_window(QWindow * parent);

LayerParamData * a_preferences_get(const char * key);

/* Allow preferences to be manipulated externally. */
void a_preferences_run_setparam(LayerParamData data, LayerParam * vlparams);

bool a_preferences_save_to_file();




#endif
