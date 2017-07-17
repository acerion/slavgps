/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2006, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
 *
 */

/**
 * SECTION:babel
 * @short_description: running external programs and redirecting to TRWLayers.
 *
 * GPSBabel may not be necessary for everything,
 *  one can use shell_command option but this will be OS platform specific
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <map>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <QTemporaryFile>
#include <QDebug>

#include <glib.h>
#include <glib/gstdio.h>

#include "gpx.h"
#include "babel.h"
#include "file.h"
#include "util.h"
#include "preferences.h"
#include "globals.h"




using namespace SlavGPS;




/* TODO in the future we could have support for other shells (change command strings), or not use a shell at all. */
#define BASH_LOCATION "/bin/bash"

/**
 * Path to gpsbabel.
 */
static char *gpsbabel_loc = NULL;

/**
 * Path to unbuffer.
 */
static char *unbuffer_loc = NULL;

/**
   Collection of file types supported by gpsbabel.
*/
std::map<int, BabelFileType *> a_babel_file_types;

/**
 * List of device supported by gpsbabel.
 */
extern std::vector<BabelDevice *> a_babel_device_list;




/**
 * @trw:        The TRW layer to modify. All data will be deleted, and replaced by what gpsbabel outputs.
 * @babelargs: A string containing gpsbabel command line filter options. No file types or names should
 *             be specified.
 * @cb:        A callback function.
 * @user_data: passed along to cb
 * @not_used:  Must use NULL
 *
 * This function modifies data in a trw layer using gpsbabel filters.  This routine is synchronous;
 * that is, it will block the calling program until the conversion is done. To avoid blocking, call
 * this routine from a worker thread.
 *
 * Returns: %true on success.
 */
bool a_babel_convert(LayerTRW * trw, const char * babelargs, BabelStatusFunc cb, void * user_data, void * unused)
{
	bool ret = false;
	char *bargs = g_strconcat(babelargs, " -i gpx", NULL);
	char *name_src = a_gpx_write_tmp_file(trw, NULL);

	if (name_src) {
		ProcessOptions po(bargs, name_src, NULL, NULL); /* kamil FIXME: memory leak through these pointers? */
		ret = a_babel_convert_from(trw, &po, cb, user_data, (DownloadOptions *) unused);
		(void) remove(name_src);
		free(name_src);
	}

	free(bargs);
	return ret;
}




/**
 * Perform any cleanup actions once GPSBabel has completed running.
 */
static void babel_watch(GPid pid, int status, void * user_data)
{
	g_spawn_close_pid(pid);
}




/**
 * @args: The command line arguments passed to GPSBabel
 * @cb: callback that is run for each line of GPSBabel output and at completion of the run
 *      callback may be NULL
 * @user_data: passed along to cb
 *
 * The function to actually invoke the GPSBabel external command.
 *
 * Returns: %true on successful invocation of GPSBabel command.
 */
static bool babel_general_convert(BabelStatusFunc cb, char **args, void * user_data)
{
	bool ret = false;
	GPid pid;
	GError *error = NULL;
	int babel_stdout;

	if (vik_debug) {
		for (unsigned int i=0; args[i]; i++) {
			fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, args[i]);
		}
	}

	if (!g_spawn_async_with_pipes(NULL, args, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL, &babel_stdout, NULL, &error)) {
		fprintf(stderr, "WARNING: Async command failed: %s\n", error->message);
		g_error_free(error);
		ret = false;
	} else {

		char line[512];
		FILE * diag = fdopen(babel_stdout, "r");
		setvbuf(diag, NULL, _IONBF, 0);

		while (fgets(line, sizeof(line), diag)) {
			if (cb) {
				cb(BABEL_DIAG_OUTPUT, line, user_data);
			}
		}
		if (cb) {
			cb(BABEL_DONE, NULL, user_data);
		}
		fclose(diag);
		diag = NULL;

		g_child_watch_add(pid, (GChildWatchFunc) babel_watch, NULL);

		ret = true;
	}

	return ret;
}



/**
   Runs \param program with the \param args and uses the GPX module to
   import the GPX data into \param trw layer. Assumes that upon
   running the command, the data will appear in the (usually
   temporary) file \param intermediate_file_path.

   \param trw: The TrackWaypoint Layer to save the data into.  If it
   is null it signifies that no data is to be processed, however the
   gpsbabel command is still ran as it can be for non-data related
   options eg: for use with the power off command - 'command_off'

   \param program: path to program to be executed

   \param args: list of program's arguments (program's name is not on that list)

   \param intermediate_file_path: path to temporary file, to which
   babel should save data; data from that file will be then read into
   supplied \param trw layer

   \param cb: callback that is run upon new data from STDOUT (?).
   (TODO: STDERR would be nice since we usually redirect STDOUT)

   \param cb_data: data passed along to \param cb

   \return true on success
   \return false otherwise
*/
static bool babel_convert_from(LayerTRW * trw, const QString & program, const QStringList & args, const QString & intermediate_file_path, BabelStatusFunc cb, void * cb_data)
{
	qDebug().nospace() << "II: Babel: convert from: will write to intermediate file '" << intermediate_file_path << "'";

	BabelConverter converter(program, args);
	if (!converter.run_conversion(cb, cb_data)) {
		qDebug() << "EE: Babel: convert from: conversion failed";
		return false;
	}

	if (trw == NULL) {
		/* No data actually required but still need to have run gpsbabel anyway
		   - eg using the device power command_off. */
		return true;
	}

	FILE * f = fopen(intermediate_file_path.toUtf8().constData(), "r");
	if (!f) {
		qDebug().nospace() << "EE: Babel: convert from: can't open intermediate file '" << intermediate_file_path << "'";
		return false;
	}

	bool read_success = a_gpx_read_file(trw, f);
	if (!read_success) {
		qDebug() << "EE: Babel: convert from: failed to read intermediate gpx file" << intermediate_file_path;
	}

	fclose(f);
	f = NULL;

	return read_success;
}




static bool babel_general_convert_from(LayerTRW * trw, BabelStatusFunc cb, char **args, const char * source_file_name, void * user_data)
{
	qDebug().nospace() << "II: Babel: general convert from: will convert file '" << source_file_name << "'";

	if (!babel_general_convert(cb, args, user_data)) {
		qDebug() << "EE: Babel: general convert from: conversion failed";
		return false;
	}

	if (trw == NULL) {
		/* No data actually required but still need to have run gpsbabel anyway
		   - eg using the device power command_off. */
		return true;
	}

	FILE * f = fopen(source_file_name, "r");
	if (!f) {
		qDebug() << "EE: Babel: general convert from: can't open source file '" << source_file_name << "'";
		return false;
	}

	bool read_success = a_gpx_read_file(trw, f);
	if (!read_success) {
		qDebug() << "EE: Babel: general convert from: failed to read result gpx file";
	}

	fclose(f);
	f = NULL;

	return read_success;
}




/**
 * @trw:           The TRW layer to place data into. Duplicate items will be overwritten.
 * @babelargs:    A string containing gpsbabel command line options. This string
 *                must include the input file type (-i) option.
 * @input_file_path:  the file name to convert from
 * @babelfilters: A string containing gpsbabel filter command line options
 * @cb:	          Optional callback function. Same usage as in a_babel_convert().
 * @user_data:    passed along to cb
 * @not_used:     Must use NULL
 *
 * Loads data into a trw layer from a file, using gpsbabel.  This routine is synchronous;
 * that is, it will block the calling program until the conversion is done. To avoid blocking, call
 * this routine from a worker thread.
 *
 * Returns: %true on success.
 */
bool a_babel_convert_from_filter(LayerTRW * trw, const char *babelargs, const QString & input_file_path, const char *babelfilters, BabelStatusFunc cb, void * user_data, void * not_used)
{
	if (!gpsbabel_loc) {
		qDebug() << "EE: Babel: gpsbabel not found in PATH";
		return false;
	}

	QTemporaryFile intermediate_file("tmp-viking.XXXXXX");
	if (!intermediate_file.open()) {
		qDebug().nospace() << "EE: Babel: from filter: failed to open intermediate file";
		return false;
	}
	const QString intermediate_file_path = intermediate_file.fileName();
	qDebug().nospace() << "II: Babel: from filter: intermediate file: '" << intermediate_file.fileName() << "'";
	intermediate_file.close(); /* We have to close it here, otherwise gpsbabel won't be able to write to it. */


	QString program;
	QStringList args; /* Args list won't contain main application's name. */

	if (unbuffer_loc) {
		program = QString(unbuffer_loc);
		args << QString(gpsbabel_loc);
	} else {
		program = QString(gpsbabel_loc);
	}

	char **sub_args = g_strsplit(babelargs, " ", 0);
	char **sub_filters = NULL;
	for (int j = 0; sub_args[j]; j++) {
		/* Some version of gpsbabel can not take extra blank arg. */
		if (sub_args[j][0] != '\0') {
			args << QString(sub_args[j]);
		}
	}

	args << QString("-f");
	args << QString(input_file_path);

	if (babelfilters) {
		sub_filters = g_strsplit(babelfilters, " ", 0);
		for (int j = 0; sub_filters[j]; j++) {
			/* Some version of gpsbabel can not take extra blank arg. */
			if (sub_filters[j][0] != '\0') {
				args << QString(sub_filters[j]);
			}
		}
	}

	args << QString("-o");
	args << QString("gpx");

	args << QString("-F");
	args << intermediate_file_path;

	bool ret = babel_convert_from(trw, program, args, intermediate_file_path, cb, user_data);

	g_strfreev(sub_args);
	if (sub_filters) {
		g_strfreev(sub_filters);
	}

	intermediate_file.remove(); /* Close and remove. */

	return ret;
}




/**
 * @trw: The #LayerTRW where to insert the collected data
 * @input_cmd: the command to run
 * @input_file_type:
 * @cb:	       Optional callback function. Same usage as in a_babel_convert().
 * @user_data: passed along to cb
 * @not_used:  Must use NULL
 *
 * Runs the input command in a shell (bash) and optionally uses GPSBabel to convert from input_file_type.
 * If input_file_type is %NULL, doesn't use GPSBabel. Input must be GPX (or Geocaching *.loc)
 *
 * Uses babel_general_convert_from() to actually run the command. This function
 * prepares the command and temporary file, and sets up the arguments for bash.
 */
bool a_babel_convert_from_shellcommand(LayerTRW * trw, const char *input_cmd, const char *input_file_type, BabelStatusFunc cb, void * user_data, void * not_used)
{
	int fd_dst;
	char *name_dst = NULL;
	bool ret = false;
	char **args;

	if ((fd_dst = g_file_open_tmp("tmp-viking.XXXXXX", &name_dst, NULL)) >= 0) {
		fprintf(stderr, "DEBUG: %s: temporary file: %s\n", __FUNCTION__, name_dst);
		char *shell_command;
		if (input_file_type) {
			shell_command = g_strdup_printf("%s | %s -i %s -f - -o gpx -F %s",
							input_cmd, gpsbabel_loc, input_file_type, name_dst);
		} else {
			shell_command = g_strdup_printf("%s > %s", input_cmd, name_dst);
		}

		fprintf(stderr, "DEBUG: %s: %s\n", __FUNCTION__, shell_command);
		close(fd_dst);

		args = (char **) malloc(4 * sizeof (char *));
		args[0] = (char *) BASH_LOCATION;
		args[1] = (char *) "-c";
		args[2] = shell_command;
		args[3] = NULL;

		ret = babel_general_convert_from(trw, cb, args, name_dst, user_data);
		free(args);
		free(shell_command);
		(void) remove(name_dst);
		free(name_dst);
	}

	return ret;
}




/**
 * @trw: The #LayerTRW where to insert the collected data
 * @url: the URL to fetch
 * @input_type:   If input_type is %NULL, input must be GPX.
 * @babelfilters: The filter arguments to pass to gpsbabel
 * @cb:	          Optional callback function. Same usage as in a_babel_convert().
 * @user_data:    Passed along to cb
 * @options:      Download options. If %NULL then default download options will be used.
 *
 * Download the file pointed by the URL and optionally uses GPSBabel to convert from input_type.
 * If input_type and babelfilters are %NULL, gpsbabel is not used.
 *
 * Returns: %true on successful invocation of GPSBabel or read of the GPX.
 */
bool a_babel_convert_from_url_filter(LayerTRW * trw, const char *url, const char *input_type, const char *babelfilters, BabelStatusFunc cb, void * user_data, DownloadOptions * dl_options)
{
	/* If no download options specified, use defaults: */
	DownloadOptions babel_dl_options(2);
	if (dl_options) {
		babel_dl_options = *dl_options;
	}
	int fd_src;
	DownloadResult fetch_ret;
	bool ret = false;
	char *name_src = NULL;
	char *babelargs = NULL;

	fprintf(stderr, "DEBUG: %s: input_type=%s url=%s\n", __FUNCTION__, input_type, url);

	if ((fd_src = g_file_open_tmp("tmp-viking.XXXXXX", &name_src, NULL)) >= 0) {
		fprintf(stderr, "DEBUG: %s: temporary file: %s\n", __FUNCTION__, name_src);
		close(fd_src);
		(void) remove(name_src);

		fetch_ret = Download::get_url_http(url, "", name_src, &babel_dl_options, NULL);
		if (fetch_ret == DownloadResult::SUCCESS) {
			if (input_type != NULL || babelfilters != NULL) {
				babelargs = (input_type) ? g_strdup_printf(" -i %s", input_type) : g_strdup("");
				ret = a_babel_convert_from_filter(trw, babelargs, name_src, babelfilters, NULL, NULL, NULL);
			} else {
				/* Process directly the retrieved file. */
				fprintf(stderr, "DEBUG: %s: directly read GPX file %s\n", __FUNCTION__, name_src);
				FILE *f = fopen(name_src, "r");
				if (f) {
					ret = a_gpx_read_file(trw, f);
					fclose(f);
					f = NULL;
				}
			}
		}
		(void)util_remove(name_src);
		free(babelargs);
		free(name_src);
	}

	return ret;
}



/**
 * @vt:               The TRW layer to place data into. Duplicate items will be overwritten.
 * @process_options:  The options to control the appropriate processing function. See #ProcessOptions for more detail
 * @cb:               Optional callback function. Same usage as in a_babel_convert().
 * @user_data:        passed along to cb
 * @dl_options:       If downloading from a URL use these options (may be NULL)
 *
 * Loads data into a trw layer from a file, using gpsbabel.  This routine is synchronous;
 * that is, it will block the calling program until the conversion is done. To avoid blocking, call
 * this routine from a worker thread.
 *
 * Returns: %true on success.
 */
bool SlavGPS::a_babel_convert_from(LayerTRW * trw, ProcessOptions *process_options, BabelStatusFunc cb, void * user_data, DownloadOptions * dl_options)
{
	if (!process_options) {
		qDebug() << "EE: Babel: convert from: no process options";
		return false;
	}

	if (process_options->url) {
		qDebug() << "II: Babel: convert from: url";
		return a_babel_convert_from_url_filter(trw, process_options->url, process_options->input_file_type, process_options->babel_filters, cb, user_data, dl_options);
	}

	if (process_options->babelargs) {
		qDebug() << "II: Babel: convert from: babel args";
		return a_babel_convert_from_filter(trw, process_options->babelargs, QString(process_options->filename), process_options->babel_filters, cb, user_data, dl_options);
	}

	if (process_options->shell_command) {
		qDebug() << "II: Babel: convert from: shell command";
		return a_babel_convert_from_shellcommand(trw, process_options->shell_command, process_options->filename, cb, user_data, dl_options);
	}

	qDebug() << "II: Babel: convert from: no process option found";
	return false;
}




static bool babel_general_convert_to(LayerTRW * trw, Track * trk, BabelStatusFunc cb, char **args, const char *name_src, void * user_data)
{
	/* Now strips out invisible tracks and waypoints. */
	if (!a_file_export(trw, name_src, FILE_TYPE_GPX, trk, false)) {
		fprintf(stderr, "CRITICAL: Error exporting to %s\n", name_src);
		return false;
	}

	return babel_general_convert(cb, args, user_data);
}




/**
 * @vt:             The TRW layer from which data is taken.
 * @track:          Operate on the individual track if specified. Use NULL when operating on a TRW layer
 * @babelargs:      A string containing gpsbabel command line options.  In addition to any filters, this string
 *                 must include the input file type (-i) option.
 * @to:             Filename or device the data is written to.
 * @cb:		   Optional callback function. Same usage as in a_babel_convert.
 * @user_data: passed along to cb
 *
 * Exports data using gpsbabel.  This routine is synchronous;
 * that is, it will block the calling program until the conversion is done. To avoid blocking, call
 * this routine from a worker thread.
 *
 * Returns: %true on successful invocation of GPSBabel command.
 */
bool SlavGPS::a_babel_convert_to(LayerTRW * trw, Track * trk, const char *babelargs, const char *to, BabelStatusFunc cb, void * user_data)
{
	int i,j;
	int fd_src;
	char *name_src = NULL;
	bool ret = false;
	char *args[64];

	if ((fd_src = g_file_open_tmp("tmp-viking.XXXXXX", &name_src, NULL)) >= 0) {
		fprintf(stderr, "DEBUG: %s: temporary file: %s\n", __FUNCTION__, name_src);
		close(fd_src);

		if (gpsbabel_loc) {
			char **sub_args = g_strsplit(babelargs, " ", 0);

			i = 0;
			if (unbuffer_loc) {
				args[i++] = unbuffer_loc;
			}
			args[i++] = gpsbabel_loc;
			args[i++] = (char *) "-i";
			args[i++] = (char *) "gpx";
			for (j = 0; sub_args[j]; j++) {
				/* Some version of gpsbabel can not take extra blank arg. */
				if (sub_args[j][0] != '\0') {
					args[i++] = sub_args[j];
				}
			}
			args[i++] = (char *) "-f";
			args[i++] = name_src;
			args[i++] = (char *) "-F";
			args[i++] = (char *)to;
			args[i] = NULL;

			ret = babel_general_convert_to(trw, trk, cb, args, name_src, user_data);

			g_strfreev(sub_args);
		} else {
			fprintf(stderr, "CRITICAL: gpsbabel not found in PATH\n");
		}
		(void) remove(name_src);
		free(name_src);
	}

	return ret;
}




static void set_mode(BabelMode *mode, char *smode)
{
	mode->waypoints_read  = smode[0] == 'r';
	mode->waypoints_write = smode[1] == 'w';
	mode->tracks_read     = smode[2] == 'r';
	mode->tracks_write    = smode[3] == 'w';
	mode->routes_read     = smode[4] == 'r';
	mode->routes_write    = smode[5] == 'w';
}




/**
 * Load a single feature stored in the given line.
 */
static void load_feature_parse_line (char *line)
{
	char **tokens = g_strsplit (line, "\t", 0);
	if (tokens != NULL
	    && tokens[0] != NULL) {
		if (strcmp("serial", tokens[0]) == 0) {
			if (tokens[1] != NULL
			    && tokens[2] != NULL
			    && tokens[3] != NULL
			    && tokens[4] != NULL) {

				BabelDevice * device = (BabelDevice *) malloc(sizeof (BabelDevice));
				set_mode (&(device->mode), tokens[1]);
				device->name = g_strdup(tokens[2]);
				device->label = g_strndup (tokens[4], 50); /* Limit really long label text. */
#ifdef K
				a_babel_device_list.push_back(device);
#endif
#if 0
				qDebug() << "DD: Babel: new gpsbabel device:"
					 << device->name
					 << device->mode.waypoints_read
					 << device->mode.waypoints_write
					 << device->mode.tracks_read
					 << device->mode.tracks_write
					 << device->mode.routes_read
					 << device->mode.routes_write
					 << tokens[1];
#endif
			} else {
				fprintf(stderr, "WARNING: Unexpected gpsbabel format string: %s\n", line);
			}
		} else if (strcmp("file", tokens[0]) == 0) {
			if (tokens[1] != NULL
			    && tokens[2] != NULL
			    && tokens[3] != NULL
			    && tokens[4] != NULL) {

				static int type_id = 0;

				BabelFileType * file_type = (BabelFileType *) malloc(sizeof (BabelFileType));
				set_mode (&(file_type->mode), tokens[1]);
				file_type->name = g_strdup(tokens[2]);
				file_type->ext = g_strdup(tokens[3]);
				file_type->label = g_strdup(tokens[4]);
				a_babel_file_types.insert({{ type_id, file_type }});
#if 0
				qDebug() << "II: Babel: gpsbabel file type #"
					 << type_id
					 << ": "
					 << file_type->name
					 << " "
					 << file_type->mode.waypoints_read
					 << file_type->mode.waypoints_write
					 << file_type->mode.tracks_read
					 << file_type->mode.tracks_write
					 << file_type->mode.routes_read
					 << file_type->mode.routes_write
					 << " "
					 << tokens[1];
#endif
				type_id++;
			} else {
				fprintf(stderr, "WARNING: Unexpected gpsbabel format string: %s\n", line);
			}
		} /* else: ignore. */
	} else {
		fprintf(stderr, "WARNING: Unexpected gpsbabel format string: %s\n", line);
	}
	g_strfreev (tokens);
}




static void load_feature_cb(BabelProgressCode code, void * line, void * user_data)
{
	if (line != NULL) {
		load_feature_parse_line((char *) line);
	}
}




static bool load_feature()
{
	bool ret = false;

	if (gpsbabel_loc) {
		int i = 0;
		char * args[4];

		if (unbuffer_loc) {
			args[i++] = unbuffer_loc;
		}
		args[i++] = gpsbabel_loc;
		args[i++] = (char *) "-^3";
		args[i] = NULL;

		ret = babel_general_convert(load_feature_cb, args, NULL);
	} else {
		fprintf(stderr, "CRITICAL: gpsbabel not found in PATH\n");
	}

	return ret;
}




static Parameter prefs[] = {
	{ (param_id_t) LayerType::NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "gpsbabel", SGVariantType::STRING, VIK_LAYER_GROUP_NONE, N_("GPSBabel:"), WidgetType::FILEENTRY, NULL, NULL, N_("Allow setting the specific instance of GPSBabel. You must restart Viking for this value to take effect."), NULL, NULL, NULL },
};




/**
 * Just setup preferences first.
 */
void SlavGPS::a_babel_init()
{
	/* Set the defaults. */
	SGVariant vlpd;
#ifdef WINDOWS
	/* Basic guesses - could use %ProgramFiles% but this is simpler: */
	if (0 == access("C:\\Program Files (x86)\\GPSBabel\\gpsbabel.exe", F_OK)) {
		/* 32 bit location on a 64 bit system. */
		vlpd.s = "C:\\Program Files (x86)\\GPSBabel\\gpsbabel.exe";
	} else {
		vlpd.s = "C:\\Program Files\\GPSBabel\\gpsbabel.exe";
	}
#else
	vlpd.s = "gpsbabel";
#endif
	a_preferences_register(&prefs[0], vlpd, VIKING_PREFERENCES_IO_GROUP_KEY);
}




/**
 * Initialises babel module.
 * Mainly check existence of gpsbabel progam and load all features available in that version.
 */
void SlavGPS::a_babel_post_init()
{
	/* Read the current preference. */
	const char *gpsbabel = a_preferences_get(VIKING_PREFERENCES_IO_NAMESPACE "gpsbabel")->s;
	/* If setting is still the UNIX default then lookup in the path - otherwise attempt to use the specified value directly. */
	if (strcmp(gpsbabel, "gpsbabel") == 0) {
		gpsbabel_loc = g_find_program_in_path("gpsbabel");
		if (!gpsbabel_loc) {
			fprintf(stderr, "CRITICAL: gpsbabel not found in PATH\n");
		}
	} else {
		gpsbabel_loc = (char*)gpsbabel;
	}

	/* Unlikely to package unbuffer on Windows so ATM don't even bother trying.
	   Highly unlikely unbuffer is available on a Windows system otherwise. */
#ifndef WINDOWS
	unbuffer_loc = g_find_program_in_path("unbuffer");
	if (!unbuffer_loc) {
		fprintf(stderr, "WARNING: unbuffer not found in PATH\n");
	}
#endif

	load_feature();
}




/**
 * Free resources acquired by a_babel_init.
 */
void SlavGPS::a_babel_uninit()
{
	free(gpsbabel_loc);
	free(unbuffer_loc);

	if (a_babel_file_types.size()) {
		for (auto iter = a_babel_file_types.begin(); iter != a_babel_file_types.end(); iter++) {
			BabelFileType * file_type = iter->second;
			//fprintf(stderr, "%s:%d: freeing file '%s' / '%s'\n", __FUNCTION__, __LINE__, file_type->name, file_type->label);
			free(file_type->name);
			free(file_type->ext);
			free(file_type->label);

			/* kamilFIXME: how should we do this? How to destroy BabelFileType? */
			// free(*iter);
			// a_babel_file_types.erase(iter);
		}
	}

#ifdef K
	if (a_babel_device_list.size()) {
		for (auto iter = a_babel_device_list.begin(); iter != a_babel_device_list.end(); iter++) {
			BabelDevice * device = *iter;
			fprintf(stderr, "%s:%d: freeing device '%s' / '%s'\n", __FUNCTION__, __LINE__, device->name, device->label);
			free(device->name);
			free(device->label);

			/* kamilFIXME: how should we do this? How to destroy BabelDevice? */
			// free(*iter);
			// a_babel_device_list.erase(iter);
		}
	}
#endif

}




/**
 * Indicates if babel is available or not.
 *
 * Returns: true if babel available.
 */
bool SlavGPS::a_babel_available()
{
	return true;
#ifdef K
	return !a_babel_device_list.empty();
#endif
}




BabelConverter::BabelConverter(const QString & program, const QStringList & args)
{
	this->process = new QProcess(this);

	this->process->setProgram(program);
	this->process->setArguments(args);

	QObject::connect(this->process, SIGNAL (started()), this, SLOT (started_cb()));
	QObject::connect(this->process, SIGNAL (finished(int, QProcess::ExitStatus)), this, SLOT (finished_cb(int, QProcess::ExitStatus)));
	QObject::connect(this->process, SIGNAL (errorOccurred(QProcess::ProcessError)), this, SLOT (error_occurred_cb(QProcess::ProcessError)));

	if (true || vik_debug) {
		for (int i = 0; i < args.size(); i++) {
			qDebug().nospace() << "DD: Babel Converter: arg #" << i << ": '" << args.at(i) << "'";
		}
	}
}




BabelConverter::~BabelConverter()
{
	delete this->process;
}




bool BabelConverter::run_conversion(BabelStatusFunc cb, void * cb_data)
{
	bool success = true;
	this->process->start();
	//this->process->waitForStarted(-1);
	this->process->waitForFinished(-1);

#ifdef K
	GPid pid;
	GError *error = NULL;
	int babel_stdout;


	if (!g_spawn_async_with_pipes(NULL, args, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL, &babel_stdout, NULL, &error)) {
		fprintf(stderr, "WARNING: Async command failed: %s\n", error->message);
		g_error_free(error);
		ret = false;
	} else {

		char line[512];
		FILE * diag = fdopen(babel_stdout, "r");
		setvbuf(diag, NULL, _IONBF, 0);

		while (fgets(line, sizeof(line), diag)) {
			if (cb) {
				cb(BABEL_DIAG_OUTPUT, line, cb_data);
			}
		}
		if (cb) {
			cb(BABEL_DONE, NULL, cb_data);
		}
		fclose(diag);
		diag = NULL;

		g_child_watch_add(pid, (GChildWatchFunc) babel_watch, NULL);

		ret = true;
	}
#endif


	return success;
}




void BabelConverter::started_cb(void)
{
	qDebug() << "II: Babel Converter: process started";
}




void BabelConverter::error_occurred_cb(QProcess::ProcessError error)
{
	qDebug() << "II: Babel Converter: error occurred when running process:" << error;
}




void BabelConverter::finished_cb(int exit_code, QProcess::ExitStatus exitStatus)
{
	qDebug() << "II: Babel Converter: process finished with exit code" << exit_code << "and exit status" << exitStatus;

	QByteArray ba = this->process->readAllStandardOutput();
	qDebug() << "II: Babel Converter: stdout: '" << ba.data() << "'";

	ba = this->process->readAllStandardError();
	qDebug() << "II: Babel Converter: stderr: '" << ba.data() << "'";
}
