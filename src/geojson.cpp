/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (c) 2014, Rob Norris <rw_norris@hotmail.com>
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
 * See http://geojson.org/ for the specification
 *
 */

#include <cstdlib>

#include "geojson.h"
#include "gpx.h"
#include "globals.h"
#include "window.h"
#include "util.h"
#include "layer_trw.h"

#include <glib.h>
#include <glib/gstdio.h>




using namespace SlavGPS;




/**
 * Perform any cleanup actions once program has completed running.
 */
static void my_watch(GPid pid, int status, void * user_data)
{
	g_spawn_close_pid(pid);
}




/**
 * Returns true if successfully written.
 */
bool SlavGPS::geojson_write_file(FILE * file, LayerTRW * trw)
{
	bool result = false;

	char *tmp_filename = a_gpx_write_tmp_file(trw, NULL);
	if (!tmp_filename) {
		return result;
	}

	GPid pid;
	int mystdout;

	/* geojson program should be on the $PATH. */
	char ** argv;
	argv = (char **) malloc(5 * sizeof (char *));
	argv[0] = g_strdup(geojson_program_export());
	argv[1] = strdup("-f");
	argv[2] = strdup("gpx");
	argv[3] = g_strdup(tmp_filename);
	argv[4] = NULL;

	GError * error = NULL;
	/* TODO: monitor stderr? */
	if (!g_spawn_async_with_pipes(NULL, argv, NULL, (GSpawnFlags) (G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD), NULL, NULL, &pid, NULL, &mystdout, NULL, &error)) {

		Window * w = trw->get_window();
		if (w) {
			w->statusbar_update(StatusBarField::INFO, QString("%s command failed: %1").arg(error->message));
		} else {
			fprintf(stderr, "WARNING: Async command failed: %s\n", error->message);
		}

		g_error_free(error);
	} else {
		/* Probably should use GIOChannels... */
		char line[512];
		FILE * fout = fdopen(mystdout, "r");
		setvbuf(fout, NULL, _IONBF, 0);

		while (fgets(line, sizeof(line), fout)) {
			fprintf(file, "%s", line);
		}

		fclose(fout);

		g_child_watch_add(pid, (GChildWatchFunc) my_watch, NULL);
		result = true;
	}

	g_strfreev(argv);

	/* Delete the temporary file. */
	(void) remove(tmp_filename);
	free(tmp_filename);

	return result;
}




/*
   https://github.com/mapbox/togeojson

   https://www.npmjs.org/package/togeojson

   Tested with version 0.7.0.
*/
const char * SlavGPS::geojson_program_export(void)
{
	return "togeojson";
}



/*
  https://github.com/tyrasd/togpx

  https://www.npmjs.org/package/togpx

  Tested with version 0.3.1.
*/
const char * SlavGPS::geojson_program_import(void)
{
	return "togpx";
}




/**
 * @filename: The source GeoJSON file
 *
 * Returns: The name of newly created temporary GPX file.
 *          This file should be removed once used and the string freed.
 *          If NULL then the process failed.
 */
char * SlavGPS::geojson_import_to_gpx(const char * filename)
{
	char * gpx_filename = NULL;
	GError * error = NULL;
	/* Opening temporary file. */
	int fd = g_file_open_tmp("vik_geojson_XXXXXX.gpx", &gpx_filename, &error);
	if (fd < 0) {
		fprintf(stderr, _("WARNING: failed to open temporary file: %s\n"), error->message);
		g_clear_error(&error);
		return NULL;
	}
	fprintf(stderr, "DEBUG: %s: temporary file = %s\n", __FUNCTION__, gpx_filename);

	GPid pid;
	int mystdout;

	/* geojson program should be on the $PATH. */
	char ** argv;
	argv = (char **) malloc(3 * sizeof (char *));
	argv[0] = g_strdup(geojson_program_import());
	argv[1] = g_strdup(filename);
	argv[2] = NULL;

	FILE * gpxfile = fdopen(fd, "w");

	/* TODO: monitor stderr? */
	if (!g_spawn_async_with_pipes(NULL, argv, NULL, (GSpawnFlags) (G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD), NULL, NULL, &pid, NULL, &mystdout, NULL, &error)) {
		fprintf(stderr, "WARNING: Async command failed: %s\n", error->message);
		g_error_free(error);
	} else {
		/* Probably should use GIOChannels... */
		char line[512];
		FILE * fout = fdopen(mystdout, "r");
		setvbuf(fout, NULL, _IONBF, 0);

		while (fgets(line, sizeof(line), fout)) {
			fprintf(gpxfile, "%s", line);
		}

		fclose(fout);
		g_child_watch_add(pid, (GChildWatchFunc) my_watch, NULL);
	}

	fclose(gpxfile);

	g_strfreev(argv);

	return gpx_filename;
}
