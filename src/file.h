/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2012, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifndef _VIKING_FILE_H
#define _VIKING_FILE_H

#include <glib.h>
#include <stdbool.h>
#include <stdint.h>


#include "vikaggregatelayer.h"
#include "viktrwlayer.h"
#include "vikviewport.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
	FILE_TYPE_GPSPOINT=1,
	FILE_TYPE_GPSMAPPER=2,
	FILE_TYPE_GPX=3,
	FILE_TYPE_KML=4,
	FILE_TYPE_GEOJSON=5,
} VikFileType_t;

bool a_file_check_ext(const char *filename, const char *fileext);

	/*
 * Function to determine if a filename is a 'viking' type file
 */
bool check_file_magic_vik(const char *filename);

typedef enum {
	LOAD_TYPE_READ_FAILURE,
	LOAD_TYPE_GPSBABEL_FAILURE,
	LOAD_TYPE_GPX_FAILURE,
	LOAD_TYPE_UNSUPPORTED_FAILURE,
	LOAD_TYPE_VIK_FAILURE_NON_FATAL,
	LOAD_TYPE_VIK_SUCCESS,
	LOAD_TYPE_OTHER_SUCCESS,
} VikLoadType_t;

char *append_file_ext(const char *filename, VikFileType_t type);

VikLoadType_t a_file_load(VikAggregateLayer *top, VikViewport *vp, const char *filename);
bool a_file_save(VikAggregateLayer *top, void * vp, const char *filename);
/* Only need to define Track if the file type is FILE_TYPE_GPX_TRACK */
bool a_file_export(LayerTRW * trw, const char *filename, VikFileType_t file_type, Track * trk, bool write_hidden);
bool a_file_export_babel(LayerTRW * trw, const char *filename, const char *format,
			 bool tracks, bool routes, bool waypoints);

void file_write_layer_param(FILE *f, const char *name, VikLayerParamType type, VikLayerParamData data);

char *file_realpath(const char *path, char *real);

char *file_realpath_dup(const char *path);

const char *file_GetRelativeFilename(char *currentDirectory, char *absoluteFilename);

#ifdef __cplusplus
}
#endif

#endif
