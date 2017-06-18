/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_BABEL_H_
#define _SG_BABEL_H_



#include <cstdint>

#include <glib.h>

#include "layer_trw.h"
#include "download.h"





/**
 * BabelProgressCode:
 * @BABEL_DIAG_OUTPUT: a line of diagnostic output is available. The pointer is to a
 *                     NULL-terminated line of diagnostic output from gpsbabel.
 * @BABEL_DONE: gpsbabel finished, or %NULL if no callback is needed.
 *
 * Used when calling #BabelStatusFunc.
 */
typedef enum {
	BABEL_DIAG_OUTPUT,
	BABEL_DONE,
} BabelProgressCode;

/**
 * BabelStatusFunc:
 *
 * Callback function.
 */
typedef void (*BabelStatusFunc)(BabelProgressCode, void *, void *);

/**
 * ProcessOptions:
 *
 * All values are defaulted to NULL
 *
 * Need to specify at least one of babelargs, URL or shell_command.
 */
struct ProcessOptions {
public:
	ProcessOptions();
	ProcessOptions(const char * args, const char * filename, const char * input_file_type, const char * url);

	~ProcessOptions();

	char * babelargs = NULL;       /* The standard initial arguments to gpsbabel (if gpsbabel is to be used) - normally should include the input file type (-i) option. */
	char * filename = NULL;        /* Input filename (or device port e.g. /dev/ttyS0). */
	char * input_file_type = NULL; /* If NULL then uses internal file format handler (GPX only ATM), otherwise specify gpsbabel input type like "kml","tcx", etc... */
	char * url = NULL;             /* URL input rather than a filename. */
	char * babel_filters = NULL;   /* Optional filter arguments to gpsbabel. */
	char * shell_command = NULL;   /* Optional shell command to run instead of gpsbabel - but will be (Unix) platform specific. */
} ;

/**
 * BabelMode:
 *
 * Store the Read/Write support offered by gpsbabel for a given format.
 */
typedef struct {
	unsigned waypoints_read : 1;
	unsigned waypoints_write : 1;
	unsigned tracks_read : 1;
	unsigned tracks_write : 1;
	unsigned routes_read : 1;
	unsigned routes_write : 1;
} BabelMode;

/**
 * BabelDevice:
 * @name: gpsbabel's identifier of the device
 * @label: human readable label
 *
 * Representation of a supported device.
 */
typedef struct {
	BabelMode mode;
	char *name;
	char *label;
} BabelDevice;

/**
 * @name: gpsbabel's identifier of the format
 * @ext: file's extension for this format
 * @label: human readable label
 *
 * Representation of a supported file format.
 */
typedef struct {
	BabelMode mode;
	char *name;
	char *ext;
	char *label;
} BabelFileType;

/* NB needs to match typedef VikDataSourceProcessFunc in acquire.h. */
bool a_babel_convert_from(SlavGPS::LayerTRW * trw, ProcessOptions *process_options, BabelStatusFunc cb, void * user_data, DownloadFileOptions *download_options );

bool a_babel_convert_to(SlavGPS::LayerTRW * trw, SlavGPS::Track * trk, const char *babelargs, const char *file, BabelStatusFunc cb, void * user_data );

void a_babel_init();
void a_babel_post_init();
void a_babel_uninit();

bool a_babel_available();




#endif /* #ifndef _SG_BABEL_H_ */
