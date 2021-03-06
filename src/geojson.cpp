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
 */




#include <cstdlib>




#include <QDebug>
#include <QDir>




#include <glib.h>




#include "geojson.h"
#include "gpx.h"
#include "window.h"
#include "util.h"
#include "layer_trw.h"
#include "statusbar.h"




using namespace SlavGPS;




/**
 * Perform any cleanup actions once program has completed running.
 */
static void my_watch(GPid pid, __attribute__((unused)) int status, __attribute__((unused)) void * user_data)
{
	g_spawn_close_pid(pid);
}




/**
 * Returns true if successfully written.
 */
SaveStatus GeoJSON::write_layer_to_file(FILE * file, LayerTRW * trw)
{
	SaveStatus result = SaveStatus::Code::GenericError;

	QString tmp_file_full_path;
	SaveStatus intermediate_status = GPX::write_layer_to_tmp_file(tmp_file_full_path, trw, NULL);
	if (SaveStatus::Code::Success != intermediate_status) {
		intermediate_status = SaveStatus::Code::CantOpenIntermediateFileError;
		return intermediate_status;
	}

	GPid pid;
	int mystdout;

	/* geojson program should be on the $PATH. */
	char ** argv;
	argv = (char **) malloc(5 * sizeof (char *));
	argv[0] = g_strdup(geojson_program_export().toUtf8().constData());
	argv[1] = strdup("-f");
	argv[2] = strdup("gpx");
	argv[3] = g_strdup(tmp_file_full_path.toUtf8().constData());
	argv[4] = NULL;

	GError * error = NULL;
	/* TODO_MAYBE: monitor stderr? */
	if (!g_spawn_async_with_pipes(NULL, argv, NULL, (GSpawnFlags) (G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD), NULL, NULL, &pid, NULL, &mystdout, NULL, &error)) {

		Window * w = trw->get_window();
		if (w) {
			w->statusbar()->set_message(StatusBarField::Info, QObject::tr("%s command failed: %1").arg(error->message));
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
		result = SaveStatus::Code::Success;
	}

	g_strfreev(argv);

	/* Delete the temporary file. */
	QDir::root().remove(tmp_file_full_path);

	return result;
}




/*
   https://github.com/mapbox/togeojson

   https://www.npmjs.org/package/togeojson

   Tested with version 0.7.0.
*/
QString SlavGPS::geojson_program_export(void)
{
	return "togeojson";
}



/*
  https://github.com/tyrasd/togpx

  https://www.npmjs.org/package/togpx

  Tested with version 0.3.1.
*/
QString SlavGPS::geojson_program_import(void)
{
	return "togpx";
}




/**
   @file_full_path: The source GeoJSON file

   Returns: The name of newly created temporary GPX file.
            This file should be removed once used.
            If returned string is empty then the process failed.
*/
QString SlavGPS::geojson_import_to_gpx(const QString & file_full_path)
{
	QString result;

	char * gpx_filename = NULL;
	GError * error = NULL;
	/* Opening temporary file. */
	int fd = g_file_open_tmp("vik_geojson_XXXXXX.gpx", &gpx_filename, &error);
	if (fd < 0) {
		qDebug() << QObject::tr("WARNING: failed to open temporary file: %1").arg(error->message);
		g_clear_error(&error);
		return result;
	}
	fprintf(stderr, "DEBUG: %s: temporary file = %s\n", __FUNCTION__, gpx_filename);

	GPid pid;
	int mystdout;

	/* geojson program should be on the $PATH. */
	char ** argv;
	argv = (char **) malloc(3 * sizeof (char *));
	argv[0] = g_strdup(geojson_program_import().toUtf8().constData());
	argv[1] = g_strdup(file_full_path.toUtf8().constData());
	argv[2] = NULL;

	FILE * gpxfile = fdopen(fd, "w");

	/* TODO_MAYBE: monitor stderr? */
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

	result = QString(gpx_filename);

	return result;
}
