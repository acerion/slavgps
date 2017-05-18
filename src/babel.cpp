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

#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

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
 * List of file formats supported by gpsbabel.
 */
std::vector<BabelFile *> a_babel_file_list;

/**
 * List of device supported by gpsbabel.
 */
extern std::vector<BabelDevice *> a_babel_device_list;




/**
 * Run a function on all file formats supporting a given mode.
 */
void a_babel_foreach_file_with_mode(BabelMode mode, GFunc func, void * user_data)
{
	for (auto iter = a_babel_file_list.begin(); iter != a_babel_file_list.end(); iter++) {
		BabelFile * currentFile = *iter;
		/* Check compatibility of modes. */
		bool compat = true;
		if (mode.waypointsRead  && ! currentFile->mode.waypointsRead)  compat = false;
		if (mode.waypointsWrite && ! currentFile->mode.waypointsWrite) compat = false;
		if (mode.tracksRead     && ! currentFile->mode.tracksRead)     compat = false;
		if (mode.tracksWrite    && ! currentFile->mode.tracksWrite)    compat = false;
		if (mode.routesRead     && ! currentFile->mode.routesRead)     compat = false;
		if (mode.routesWrite    && ! currentFile->mode.routesWrite)    compat = false;
		/* Do call. */
		if (compat) {
			func(currentFile, user_data);
		}
	}
}




/**
 * @func:      The function to be called on any file format with a read method
 * @user_data: Data passed into the function
 *
 * Run a function on all file formats with any kind of read method
 * (which is almost all but not quite - e.g. with GPSBabel v1.4.4 - PalmDoc is write only waypoints).
 */
void a_babel_foreach_file_read_any(GFunc func, void * user_data)
{
	for (auto iter = a_babel_file_list.begin(); iter != a_babel_file_list.end(); iter++) {
		BabelFile *currentFile = *iter;
		/* Call function when any read mode found. */
		if (currentFile->mode.waypointsRead
		    || currentFile->mode.tracksRead
		    || currentFile->mode.routesRead) {

			func(currentFile, user_data);
		}
	}
}




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
bool a_babel_convert(LayerTRW * trw, const char * babelargs, BabelStatusFunc cb, void * user_data, void * not_used)
{
	bool ret = false;
	char *bargs = g_strconcat(babelargs, " -i gpx", NULL);
	char *name_src = a_gpx_write_tmp_file(trw, NULL);

	if (name_src) {
		ProcessOptions po(bargs, name_src, NULL, NULL); /* kamil FIXME: memory leak through these pointers? */
		ret = a_babel_convert_from(trw, &po, cb, user_data, (DownloadFileOptions *) not_used);
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
 * @trw: The TrackWaypoint Layer to save the data into.
 *   If it is null it signifies that no data is to be processed,
 *   however the gpsbabel command is still ran as it can be for non-data related options eg:
 *   for use with the power off command - 'command_off'
 * @cb: callback that is run upon new data from STDOUT (?).
 *   (TODO: STDERR would be nice since we usually redirect STDOUT)
 * @user_data: passed along to cb
 *
 * Runs args[0] with the arguments and uses the GPX module
 * to import the GPX data into layer vt. Assumes that upon
 * running the command, the data will appear in the (usually
 * temporary) file name_dst.
 *
 * Returns: %true on success.
 */
static bool babel_general_convert_from(LayerTRW * trw, BabelStatusFunc cb, char **args, const char *name_dst, void * user_data)
{
	bool ret = false;

	if (babel_general_convert(cb, args, user_data)) {

		/* No data actually required but still need to have run gpsbabel anyway
		   - eg using the device power command_off. */
		if (trw == NULL) {
			return true;
		}

		FILE * f = fopen(name_dst, "r");
		if (f) {
			ret = a_gpx_read_file(trw, f);
			fclose(f);
			f = NULL;
		}
	}

	return ret;
}




/**
 * @trw:           The TRW layer to place data into. Duplicate items will be overwritten.
 * @babelargs:    A string containing gpsbabel command line options. This string
 *                must include the input file type (-i) option.
 * @from          the file name to convert from
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
bool a_babel_convert_from_filter(LayerTRW * trw, const char *babelargs, const char *from, const char *babelfilters, BabelStatusFunc cb, void * user_data, void * not_used)
{
	int i,j;
	int fd_dst;
	char *name_dst = NULL;
	bool ret = false;
	char *args[64];

	if ((fd_dst = g_file_open_tmp("tmp-viking.XXXXXX", &name_dst, NULL)) >= 0) {
		fprintf(stderr, "DEBUG: %s: temporary file: %s\n", __FUNCTION__, name_dst);
		close(fd_dst);

		if (gpsbabel_loc) {
			char **sub_args = g_strsplit(babelargs, " ", 0);
			char **sub_filters = NULL;

			i = 0;
			if (unbuffer_loc) {
				args[i++] = unbuffer_loc;
			}
			args[i++] = gpsbabel_loc;
			for (j = 0; sub_args[j]; j++) {
				/* Some version of gpsbabel can not take extra blank arg. */
				if (sub_args[j][0] != '\0') {
					args[i++] = sub_args[j];
				}
			}
			args[i++] = (char *) "-f";
			args[i++] = (char *)from;
			if (babelfilters) {
				sub_filters = g_strsplit(babelfilters, " ", 0);
				for (j = 0; sub_filters[j]; j++) {
					/* Some version of gpsbabel can not take extra blank arg. */
					if (sub_filters[j][0] != '\0') {
						args[i++] = sub_filters[j];
					}
				}
			}
			args[i++] = (char *) "-o";
			args[i++] = (char *) "gpx";
			args[i++] = (char *) "-F";
			args[i++] = name_dst;
			args[i] = NULL;

			ret = babel_general_convert_from(trw, cb, args, name_dst, user_data);

			g_strfreev(sub_args);
			if (sub_filters) {
				g_strfreev(sub_filters);
			}
		} else {
			fprintf(stderr, "CRITICAL: gpsbabel not found in PATH\n");
		}
		(void) remove(name_dst);
		free(name_dst);
	}

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
bool a_babel_convert_from_url_filter(LayerTRW * trw, const char *url, const char *input_type, const char *babelfilters, BabelStatusFunc cb, void * user_data, DownloadFileOptions *options)
{
	/* If no download options specified, use defaults: */
	DownloadFileOptions myoptions = { false, false, NULL, 2, NULL, NULL, NULL };
	if (options) {
		myoptions = *options;
	}
	int fd_src;
	int fetch_ret;
	bool ret = false;
	char *name_src = NULL;
	char *babelargs = NULL;

	fprintf(stderr, "DEBUG: %s: input_type=%s url=%s\n", __FUNCTION__, input_type, url);

	if ((fd_src = g_file_open_tmp("tmp-viking.XXXXXX", &name_src, NULL)) >= 0) {
		fprintf(stderr, "DEBUG: %s: temporary file: %s\n", __FUNCTION__, name_src);
		close(fd_src);
		(void) remove(name_src);

		fetch_ret = a_http_download_get_url(url, "", name_src, &myoptions, NULL);
		if (fetch_ret == DOWNLOAD_SUCCESS) {
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
 * @download_options: If downloading from a URL use these options (may be NULL)
 *
 * Loads data into a trw layer from a file, using gpsbabel.  This routine is synchronous;
 * that is, it will block the calling program until the conversion is done. To avoid blocking, call
 * this routine from a worker thread.
 *
 * Returns: %true on success.
 */
bool a_babel_convert_from(LayerTRW * trw, ProcessOptions *process_options, BabelStatusFunc cb, void * user_data, DownloadFileOptions *download_options)
{
	if (!process_options) {
		qDebug() << "EE: Babel: no process options";
		return false;
	}

	if (process_options->url) {
		qDebug() << "II: Babel: ->url";
		return a_babel_convert_from_url_filter(trw, process_options->url, process_options->input_file_type, process_options->babel_filters, cb, user_data, download_options);
	}

	if (process_options->babelargs) {
		qDebug() << "II: Babel: ->babelargs";
		return a_babel_convert_from_filter(trw, process_options->babelargs, process_options->filename, process_options->babel_filters, cb, user_data, download_options);
	}

	if (process_options->shell_command) {
		qDebug() << "II: Babel: ->shell_command";
		return a_babel_convert_from_shellcommand(trw, process_options->shell_command, process_options->filename, cb, user_data, download_options);
	}
	qDebug() << "II: Babel: convert_from returns false";
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
bool a_babel_convert_to(LayerTRW * trw, Track * trk, const char *babelargs, const char *to, BabelStatusFunc cb, void * user_data)
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
	mode->waypointsRead  = smode[0] == 'r';
	mode->waypointsWrite = smode[1] == 'w';
	mode->tracksRead     = smode[2] == 'r';
	mode->tracksWrite    = smode[3] == 'w';
	mode->routesRead     = smode[4] == 'r';
	mode->routesWrite    = smode[5] == 'w';
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
				fprintf(stderr, "DEBUG: New gpsbabel device: %s, %d%d%d%d%d%d(%s)\n",
					device->name,
					device->mode.waypointsRead, device->mode.waypointsWrite,
					device->mode.tracksRead, device->mode.tracksWrite,
					device->mode.routesRead, device->mode.routesWrite,
					tokens[1]);
			} else {
				fprintf(stderr, "WARNING: Unexpected gpsbabel format string: %s\n", line);
			}
		} else if (strcmp("file", tokens[0]) == 0) {
			if (tokens[1] != NULL
			    && tokens[2] != NULL
			    && tokens[3] != NULL
			    && tokens[4] != NULL) {

				BabelFile * file = (BabelFile *) malloc(sizeof (BabelFile));
				set_mode (&(file->mode), tokens[1]);
				file->name = g_strdup(tokens[2]);
				file->ext = g_strdup(tokens[3]);
				file->label = g_strdup(tokens[4]);
				a_babel_file_list.push_back(file);
				fprintf(stderr, "DEBUG: New gpsbabel file: %s, %d%d%d%d%d%d(%s)\n",
					file->name,
					file->mode.waypointsRead, file->mode.waypointsWrite,
					file->mode.tracksRead, file->mode.tracksWrite,
					file->mode.routesRead, file->mode.routesWrite,
					tokens[1]);
			} else {
				fprintf(stderr, "WARNING: Unexpected gpsbabel format string: %s\n", line);
			}
		} /* else: ignore. */
	} else {
		fprintf(stderr, "WARNING: Unexpected gpsbabel format string: %s\n", line);
	}
	g_strfreev (tokens);
}




static void load_feature_cb (BabelProgressCode code, void * line, void * user_data)
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
	{ (param_id_t) LayerType::NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "gpsbabel", ParameterType::STRING, VIK_LAYER_GROUP_NONE, N_("GPSBabel:"), WidgetType::FILEENTRY, NULL, NULL, N_("Allow setting the specific instance of GPSBabel. You must restart Viking for this value to take effect."), NULL, NULL, NULL },
};




/**
 * Just setup preferences first.
 */
void a_babel_init()
{
	/* Set the defaults. */
	ParameterValue vlpd;
#ifdef WINDOWS
	/* Basic guesses - could use %ProgramFiles% but this is simpler: */
	if (g_file_test ("C:\\Program Files (x86)\\GPSBabel\\gpsbabel.exe", G_FILE_TEST_EXISTS)) {
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
void a_babel_post_init()
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
void a_babel_uninit ()
{
	free(gpsbabel_loc);
	free(unbuffer_loc);

	if (a_babel_file_list.size()) {
		for (auto iter = a_babel_file_list.begin(); iter != a_babel_file_list.end(); iter++) {
			BabelFile * file = *iter;
			fprintf(stderr, "%s:%d: freeing file '%s' / '%s'\n", __FUNCTION__, __LINE__, file->name, file->label);
			free(file->name);
			free(file->ext);
			free(file->label);

			/* kamilFIXME: how should we do this? How to destroy BabelFile? */
			// free(*iter);
			// a_babel_file_list.erase(iter);
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
bool a_babel_available()
{
	return true;
#ifdef K
	return !a_babel_device_list.empty();
#endif
}
