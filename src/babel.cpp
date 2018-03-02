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

#include <cstdio>
#include <cstring>
#include <cstdlib>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <QTemporaryFile>
#include <QDebug>
#include <QStandardPaths>

#include <glib.h>
#include <glib/gstdio.h>

#include "gpx.h"
#include "babel.h"
#include "file.h"
#include "util.h"
#include "vikutils.h"
#include "preferences.h"
#include "globals.h"




using namespace SlavGPS;




#define PREFIX ": Babel:" << __FUNCTION__ << __LINE__ << ">"




/* TODO in the future we could have support for other shells (change command strings), or not use a shell at all. */
#define BASH_LOCATION "/bin/bash"




Babel babel;

/* Definitions of static members from Babel class. */
std::map<int, BabelFileType *> Babel::file_types;
std::vector<BabelDevice *> Babel::devices;

static int file_type_id = 0;




static ParameterSpecification prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_IO, "gpsbabel", SGVariantType::String, PARAMETER_GROUP_GENERIC, QObject::tr("GPSBabel:"), WidgetType::FILEENTRY, NULL, NULL, NULL, N_("Allow setting the specific instance of GPSBabel. You must restart Viking for this value to take effect.") },
};




/* Path set here may be overwritten by path from preferences. */
void Babel::get_gpsbabel_path_from_system(void)
{
	/* The path may be empty string. */
	this->gpsbabel_path = QStandardPaths::findExecutable("gpsbabel");

	Preferences::register_parameter(prefs[0], SGVariant(this->gpsbabel_path, prefs[0].type_id));

	if (this->gpsbabel_path.isEmpty()) {
		qDebug() << "WW: Babel: gpsbabel not found in PATH";
	} else {
		qDebug() << "II: Babel: path to gpsbabel initialized as" << this->gpsbabel_path;
	}
}




/* Path set here may be overwritten by path from preferences. */
void Babel::get_unbuffer_path_from_system(void)
{
	this->unbuffer_path = QStandardPaths::findExecutable("unbuffer");

	if (this->unbuffer_path.isEmpty()) {
		qDebug() << "WW: Babel: unbuffer not found in PATH";
	} else {
		qDebug() << "II: Babel: path to unbuffer initialized as" << this->unbuffer_path;
	}
}




void Babel::get_gpsbabel_path_from_preferences(void)
{
	const QString gpsbabel_path_prefs = Preferences::get_param_value(PREFERENCES_NAMESPACE_IO ".gpsbabel").val_string;
	if (!gpsbabel_path_prefs.isEmpty()) {

		/* If setting is still the UNIX default then lookup in the path - otherwise attempt to use the specified value directly. */
		if (gpsbabel_path_prefs == "gpsbabel") {
			this->gpsbabel_path = QStandardPaths::findExecutable("gpsbabel");
		} else {
			this->gpsbabel_path = gpsbabel_path_prefs;
		}
		qDebug() << "II: Babel: path to gpsbabel set from preferences as" << this->gpsbabel_path;
	}

	if (!this->gpsbabel_path.isEmpty()) {
		this->is_detected = true;
		qDebug() << "II: Babel: gpsbabel detected as" << this->gpsbabel_path;
	} else {
		qDebug() << "WW: Babel: gpsbabel not detected";
	}
}




/*
  Call the function when you need to decide if the main program should
  be 'unbuffer' or 'gpsbabel'.
*/
bool Babel::set_program_name(QString & program, QStringList & args)
{
	if (this->unbuffer_path.isEmpty()) {
		program = this->gpsbabel_path;
	} else {
		program = this->unbuffer_path;
		args << this->gpsbabel_path;
	}

	return true;
}



#if 0
/**
 * @trw:        The TRW layer to modify. All data will be deleted, and replaced by what gpsbabel outputs.
 * @babel_args: A string containing gpsbabel command line filter options. No file types or names should
 *             be specified.
 * @cb:        A callback function.
 * @cb_data: passed along to cb
 * @not_used:  Must use NULL
 *
 * This function modifies data in a trw layer using gpsbabel filters.  This routine is synchronous;
 * that is, it will block the calling program until the conversion is done. To avoid blocking, call
 * this routine from a worker thread.
 *
 * Returns: %true on success.
 */
bool a_babel_convert(LayerTRW * trw, const QString & babel_args, BabelSomething * babel_something)
{
	bool ret = false;
	const QString bargs = babel_args + " -i gpx";
	const QString name_src = GPX::write_tmp_file(trw, NULL);

	if (!name_src.isEmpty()) {
		ProcessOptions po(bargs, name_src, NULL, NULL); /* kamil FIXME: memory leak through these pointers? */
		ret = a_babel_convert_import(trw, &po, (DownloadOptions *) unused, babel_something);
		QDir::root().remove(name_src);
	}

	return ret;
}
#endif



/**
 * Perform any cleanup actions once GPSBabel has completed running.
 */
static void babel_watch(GPid pid, int status, void * cb_data)
{
	g_spawn_close_pid(pid);
}




/**
   Runs gpsbabel program with the \param args and uses the GPX module to
   import the GPX data into \param trw layer. Assumes that upon
   running the command, the data will appear in the (usually
   temporary) file \param intermediate_file_path.

   \param trw: The TrackWaypoint Layer to save the data into.  If it
   is null it signifies that no data is to be processed, however the
   gpsbabel command is still ran as it can be for non-data related
   options eg: for use with the power off command - 'command_off'

   \param program: program name

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
bool Babel::convert_through_intermediate_file(const QString & program, const QStringList & args, BabelSomething * babel_something, LayerTRW * trw, const QString & intermediate_file_path)
{
	qDebug().nospace() << "II: Babel: convert through intermediate file: will write to intermediate file '" << intermediate_file_path << "'";

	BabelConverter converter(program, args, babel_something);
	if (!converter.run_conversion(true)) {
		qDebug() << "EE: Babel: convert through intermediate file: conversion failed";
		return false;
	}

	if (trw == NULL) {
		/* No data actually required but still need to have run gpsbabel anyway
		   - eg using the device power command_off. */
		return true;
	}

	FILE * file = fopen(intermediate_file_path.toUtf8().constData(), "r");
	if (!file) {
		qDebug().nospace() << "EE: Babel: convert through intermediate file: can't open intermediate file '" << intermediate_file_path << "'";
		return false;
	}

	bool read_success = GPX::read_file(file, trw);
	if (!read_success) {
		qDebug() << "EE: Babel: convert through intermediate file: failed to read intermediate gpx file" << intermediate_file_path;
	}

	fclose(file);
	file = NULL;

	return read_success;
}




/**
 * @trw: The TRW layer to place data into. Duplicate items will be overwritten.
 * @babel_args: A string containing gpsbabel command line options. This string must include the input file type (-i) option.
 * @input_file_path: the file name to convert from
 * @babel_filters: A string containing gpsbabel filter command line options
 * @cb: Optional callback function.
 * @cb_data: Passed along to cb
 * @not_used: Must use NULL
 *
 * Loads data into a trw layer from a file, using gpsbabel.  This routine is synchronous;
 * that is, it will block the calling program until the conversion is done. To avoid blocking, call
 * this routine from a worker thread.
 *
 * Returns: %true on success.
 */
bool a_babel_convert_import_from_filter(LayerTRW * trw, const QString & babel_args, const QString & input_file_path, const QString & babel_filters, BabelSomething * babel_something)
{
	if (!babel.is_detected) {
		qDebug() << "EE: Babel: gpsbabel not found in PATH";
		return false;
	}

	QTemporaryFile intermediate_file;
	if (!SGUtils::create_temporary_file(intermediate_file, "tmp-viking.XXXXXX")) {
		return false;
	}
	const QString intermediate_file_path = intermediate_file.fileName();
	qDebug() << "DD: Babel: Convert from filter: temporary file:" << intermediate_file_path;


	QString program;
	QStringList args; /* Args list won't contain main application's name. */
	babel.set_program_name(program, args);


	const QStringList sub_args = babel_args.split(" ", QString::SkipEmptyParts); /* Some version of gpsbabel can not take extra blank arg. */
	for (int i = 0; i < sub_args.size(); i++) { /* TODO: simplify concatenation of args and sub_args. */
		args << sub_args.at(i);
	}


	args << QString("-f");
	args << QString(input_file_path);


	QStringList sub_filters;
	if (!babel_filters.isEmpty()) {
		sub_filters = babel_filters.split(" ", QString::SkipEmptyParts); /* Some version of gpsbabel can not take extra blank arg. */
		for (int i = 0; i < sub_filters.size(); i++) { /* TODO: simplify concatenation of args and sub_filters. */
			args << sub_filters.at(i);
		}
	}


	args << QString("-o");
	args << QString("gpx");


	args << QString("-F");
	args << intermediate_file_path;


	const bool ret = babel.convert_through_intermediate_file(program, args, babel_something, trw, intermediate_file_path);

	intermediate_file.remove(); /* Close and remove. */

	return ret;
}




/**
 * @trw: The #LayerTRW where to insert the collected data
 * @shell_command: the command to run
 * @input_file_type:
 * @cb:	Optional callback function.
 * @cb_data: Passed along to cb
 * @not_used: Must use NULL
 *
 * Runs the input command in a shell (bash) and optionally uses GPSBabel to convert from input_file_type.
 * If input_file_type is %NULL, doesn't use GPSBabel. Input must be GPX (or Geocaching *.loc)
 *
 * Uses Babel::convert_through_intermediate_file() to actually run the command. This function
 * prepares the command and temporary file, and sets up the arguments for bash.
 */
bool a_babel_convert_import_from_shell_command(LayerTRW * trw, const QString & shell_command, const QString & input_file_type, BabelSomething * babel_something)
{
	QTemporaryFile intermediate_file;
	if (!SGUtils::create_temporary_file(intermediate_file, "tmp-viking.XXXXXX")) {
		return false;
	}
	const QString intermediate_file_fullpath = intermediate_file.fileName();
	qDebug() << "DD: Babel: Convert from shell command: intermediate file" << intermediate_file_fullpath;


	QString full_shell_command;
	if (!input_file_type.isEmpty()) {
		full_shell_command = QString("%1 | %2 -i %3 -f - -o gpx -F %4").arg(shell_command).arg(babel.gpsbabel_path).arg(input_file_type).arg(intermediate_file_fullpath);
	} else {
		full_shell_command = QString("%s > %s").arg(shell_command).arg(intermediate_file_fullpath);
	}
	qDebug() << "DD: Babel: Convert from shell command: shell command" << full_shell_command;


	const QString program(BASH_LOCATION);
	const QStringList args(QStringList() << "-c" << full_shell_command);
	return babel.convert_through_intermediate_file(program, args, babel_something, trw, intermediate_file_fullpath);
}




/**
 * @trw: The #LayerTRW where to insert the collected data.
 * @url: The URL to fetch.
 * @input_file_type: If input_file_type is empty, input must be GPX.
 * @babel_filters: The filter arguments to pass to gpsbabel.
 * @cb: Optional callback function.
 * @cb_data: Passed along to cb.
 * @options: Download options. If %NULL then default download options will be used.
 *
 * Download the file pointed by the URL and optionally uses GPSBabel to convert from input_type.
 * If input_file_type and babel_filters are empty, gpsbabel is not used.
 *
 * Returns: %true on successful invocation of GPSBabel or read of the GPX.
 */
bool a_babel_convert_import_from_url_filter(LayerTRW * trw, const QString & url, const QString & input_file_type, const QString & babel_filters, DownloadOptions * dl_options)
{
	/* If no download options specified, use defaults: */
	DownloadOptions babel_dl_options(2);
	if (dl_options) {
		babel_dl_options = *dl_options;
	}

	bool ret = false;

	qDebug() << "DD: Babel: input_file_type =" << input_file_type << ", url =" << url;


	QTemporaryFile tmp_file;
	if (!SGUtils::create_temporary_file(tmp_file, "tmp-viking.XXXXXX")) {
		return false;
	}
	const QString name_src = tmp_file.fileName();
	qDebug() << "DD: Babel: Convert from url filter: temporary file:" << name_src;
	tmp_file.remove(); /* Because we only needed to confirm that a path to temporary file is "available"? */

	DownloadHandle dl_handle(&babel_dl_options);

	if (DownloadResult::SUCCESS == dl_handle.get_url_http(url, "", name_src)) {
		if (!input_file_type.isEmpty() || !babel_filters.isEmpty()) {
			const QString babel_args = (!input_file_type.isEmpty()) ? QString(" -i %1").arg(input_file_type) : "";
			ret = a_babel_convert_import_from_filter(trw, babel_args, name_src, babel_filters, NULL);
		} else {
			/* Process directly the retrieved file. */
			qDebug() << "DD: Babel: directly read GPX file" << name_src;
			FILE * file = fopen(name_src.toUtf8().constData(), "r");
			if (file) {
				ret = GPX::read_file(file, trw);
				fclose(file);
				file = NULL;
			}
		}
	}
	Util::remove(name_src);


	return ret;
}




/**
 * @trw: The TRW layer to place data into. Duplicate items will be overwritten.
 * @process_options: The options to control the appropriate processing function. See #ProcessOptions for more detail.
 * @cb: Optional callback function.
 * @cb_data: Passed along to cb.
 * @dl_options: If downloading from a URL use these options (may be NULL).
 *
 * Loads data into a trw layer from a file, using gpsbabel.  This routine is synchronous;
 * that is, it will block the calling program until the conversion is done. To avoid blocking, call
 * this routine from a worker thread.
 *
 * Returns: %true on success.
 */
bool SlavGPS::a_babel_convert_import(LayerTRW * trw, ProcessOptions * process_options, DownloadOptions * dl_options, BabelSomething * babel_something)
{
	if (!process_options) {
		qDebug() << "EE: Babel: convert from: no process options";
		return false;
	}

	if (!process_options->url.isEmpty()) {
		qDebug() << "II: Babel: convert from: url";
		return a_babel_convert_import_from_url_filter(trw, process_options->url, process_options->input_file_type, process_options->babel_filters, dl_options);
	}

	if (!process_options->babel_args.isEmpty()) {
		qDebug() << "II: Babel: convert from: babel args";
		return a_babel_convert_import_from_filter(trw, process_options->babel_args, process_options->input_file_name, process_options->babel_filters, babel_something);
	}

	if (!process_options->shell_command.isEmpty()) {
		qDebug() << "II: Babel: convert from: shell command";
		return a_babel_convert_import_from_shell_command(trw, process_options->shell_command, process_options->input_file_type, babel_something);
	}

	qDebug() << "II: Babel: convert from: no process option found";
	return false;
}




/**
 * @trw: The TRW layer from which data is taken.
 * @track: Operate on the individual track if specified. Use NULL when operating on a TRW layer.
 * @babel_args: A string containing gpsbabel command line options.  In addition to any filters, this string must include the input file type (-i) option.
 * @target_file_full_path: Filename or device the data is written to.
 * @cb:	Optional callback function.
 * @cb_data: passed along to cb.
 *
 * Exports data using gpsbabel.  This routine is synchronous;
 * that is, it will block the calling program until the conversion is done. To avoid blocking, call
 * this routine from a worker thread.
 *
 * Returns: %true on successful invocation of GPSBabel command.
 */
bool SlavGPS::a_babel_convert_export(LayerTRW * trw, Track * trk, const QString & babel_args, const QString & target_file_full_path, BabelSomething * babel_something)
{
	if (!babel.is_detected) {
		qDebug() << "EE: Babel: gpsbabel not found in PATH";
		return false;
	}


	QTemporaryFile tmp_file;
	if (!SGUtils::create_temporary_file(tmp_file, "tmp-viking.XXXXXX")) {
		return false;
	}
	const QString tmp_file_full_path = tmp_file.fileName();
	qDebug() << "DD: Babel: Convert to: temporary file:" << tmp_file_full_path;


	QString program;
	QStringList args;
	babel.set_program_name(program, args);


	args << "-i";
	args << "gpx";


	const QStringList sub_args = babel_args.split(" ", QString::SkipEmptyParts); /* Some version of gpsbabel can not take extra blank arg. */
	for (int i = 0; i < sub_args.size(); i++) { /* TODO: simplify concatenation of args and sub_args. */
		args << sub_args.at(i);
	}


	args << "-f";
	args << tmp_file_full_path;
	args << "-F";
	args << target_file_full_path;


	/* Now strips out invisible tracks and waypoints. */
	if (!VikFile::export_(trw, tmp_file_full_path, SGFileType::GPX, trk, false)) {
		qDebug() << "EE: Babel: Convert to: Error exporting to" << tmp_file_full_path;
		return false;
	}

	BabelConverter converter(program, args, babel_something);
	return converter.run_conversion(false);
}




static void set_mode(BabelMode * mode, const QString & mode_string)
{
	mode->waypoints_read  = mode_string.at(0) == 'r';
	mode->waypoints_write = mode_string.at(1) == 'w';
	mode->tracks_read     = mode_string.at(2) == 'r';
	mode->tracks_write    = mode_string.at(3) == 'w';
	mode->routes_read     = mode_string.at(4) == 'r';
	mode->routes_write    = mode_string.at(5) == 'w';
}




/**
   Load a single feature stored in the given line.
*/
void BabelFeatureLoader::import_progress_cb(BabelProgressCode code, void * data)
{
	if (!data) {
		return;
	}
	const QString line = QString((char *) data);

	QStringList tokens = line.split('\t', QString::KeepEmptyParts);
	if (tokens.size() == 0) {
		qDebug() << "WW" PREFIX << "Unexpected gpsbabel feature string" << line;
		return;
	}

	if ("serial" == tokens.at(0)) {
		if (tokens.size() != 6) {
			qDebug() << "WW" PREFIX << "Unexpected gpsbabel feature string" << line;
		} else {
			BabelDevice * device = new BabelDevice(tokens.at(1), tokens.at(2), tokens.at(4));
			Babel::devices.push_back(device);
		}

	} else if ("file" == tokens.at(0)) {
		if (tokens.size() != 6) {
			qDebug() << "WW" PREFIX << "Unexpected gpsbabel format string" << line;
		} else {
			BabelFileType * file_type = new BabelFileType(tokens.at(1), tokens.at(2), tokens.at(3), tokens.at(4));
			Babel::file_types.insert({{ file_type_id, file_type }});

			file_type_id++;
		}
	} else {
		/* Ignore. */
	}

	return;
}




static bool load_babel_features(void)
{
	if (!babel.is_detected) {
		qDebug() << "EE" PREFIX << "gpsbabel not found in PATH";
		return false;
	}

	QString program;
	QStringList args;
	babel.set_program_name(program, args);

	args << QString("-^3");

	BabelFeatureLoader feature_loader;

	BabelConverter converter(program, args, &feature_loader);
	return converter.run_conversion(true);
}




/**
 * Just setup preferences first.
 */
void Babel::init()
{
	babel.get_gpsbabel_path_from_system();

	/* Unlikely to package unbuffer on Windows so ATM don't even bother trying.
	   Highly unlikely unbuffer is available on a Windows system otherwise. */
#ifndef WINDOWS
	babel.get_unbuffer_path_from_system();
#endif

}




/**
 * Initialises babel module.
 * Mainly check existence of gpsbabel progam and load all features available in that version.
 */
void Babel::post_init()
{
	babel.get_gpsbabel_path_from_preferences();

	load_babel_features();
}




/**
   \brief Free resources acquired by Babel::init()
*/
void Babel::uninit()
{
	if (Babel::file_types.size()) {
		for (auto iter = Babel::file_types.begin(); iter != Babel::file_types.end(); iter++) {
			/* kamilFIXME: how should we do this? How to destroy BabelFileType? */
			// delete (*iter);
			Babel::file_types.erase(iter);
		}
	}

	if (Babel::devices.size()) {
		for (auto iter = Babel::devices.begin(); iter != Babel::devices.end(); iter++) {
			/* kamilFIXME: how should we do this? How to destroy BabelDevice? */
			// delete (*iter);
			Babel::devices.erase(iter);
		}
	}
}




/**
   \brief Indicates if babel is available or not.

   TODO: what about Babel::is_detected?

   Returns: true if babel available.
*/
bool Babel::is_available(void)
{
	return !Babel::devices.empty();
}




BabelConverter::BabelConverter(const QString & program, const QStringList & args, BabelSomething * new_babel_something)
{
	this->process = new QProcess(this);

	this->process->setProgram(program);
	this->process->setArguments(args);

	this->babel_something = new_babel_something;

	QObject::connect(this->process, SIGNAL (started()), this, SLOT (started_cb()));
	QObject::connect(this->process, SIGNAL (finished(int, QProcess::ExitStatus)), this, SLOT (finished_cb(int, QProcess::ExitStatus)));
	QObject::connect(this->process, SIGNAL (errorOccurred(QProcess::ProcessError)), this, SLOT (error_occurred_cb(QProcess::ProcessError)));
	QObject::connect(this->process, SIGNAL (readyReadStandardOutput()), this, SLOT (read_stdout_cb()));

	if (true || vik_debug) {
		qDebug() << "DD: Babel: general convert: program" << program;
		for (int i = 0; i < args.size(); i++) {
			qDebug() << "DD: Babel Converter: arg no." << i << ": '" << args.at(i) << "'";
		}
	}
}




BabelConverter::~BabelConverter()
{
	delete this->process;
}




bool BabelConverter::run_conversion(bool do_import)
{
	bool success = true;
	this->process->start();
	//this->process->waitForStarted(-1);
	this->process->waitForFinished(-1);

	if (do_import) {
		this->babel_something->import_progress_cb(BabelProgressCode::Completed, NULL);
	} else {
		this->babel_something->export_progress_cb(BabelProgressCode::Completed, NULL);
	}

#ifdef K_OLD_IMPLEMENTATION
	/* Old implementation. New implementation uses QProcess and signal sent on new data appearing on stdout. */

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
				cb(BabelProgressCode::DiagOutput, line, cb_data);
			}
		}
		if (cb) {
			cb(BabelProgressCode::Completed, NULL, cb_data);
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




void BabelConverter::read_stdout_cb()
{
	char buffer[512];

	while (this->process->canReadLine()) {
		this->process->readLine(buffer, sizeof (buffer));
		//qDebug() << "DD: Babel: Converter: read stdout" << buffer;

		this->babel_something->import_progress_cb(BabelProgressCode::Completed, buffer);
	}
}




BabelFileType::BabelFileType(const QString & new_mode, const QString & new_identifier, const QString & new_extension, const QString & new_label)
{
	set_mode(&this->mode, new_mode);
	this->identifier = new_identifier;
	this->extension = new_extension;
	this->label = new_label;

#if 1
	qDebug() << "II: Babel: gpsbabel file type #"
		 << file_type_id
		 << ": "
		 << this->identifier
		 << " "
		 << this->mode.waypoints_read
		 << this->mode.waypoints_write
		 << this->mode.tracks_read
		 << this->mode.tracks_write
		 << this->mode.routes_read
		 << this->mode.routes_write
		 << " "
		 << this->label;
#endif
}




BabelFileType::~BabelFileType()
{
	qDebug() << "DD: Babel: delete BabelFileType" << this->identifier << "/" << this->label;
}




BabelDevice::BabelDevice(const QString & new_mode, const QString & new_identifier, const QString & new_label)
{
	set_mode(&this->mode, new_mode);
	this->identifier = new_identifier;
	this->label = new_label.left(50); /* Limit really long label text. */

#if 1
	qDebug() << "DD: Babel: new gpsbabel device:"
		 << this->identifier
		 << this->mode.waypoints_read
		 << this->mode.waypoints_write
		 << this->mode.tracks_read
		 << this->mode.tracks_write
		 << this->mode.routes_read
		 << this->mode.routes_write
		 << this->label;
#endif
}




BabelDevice::~BabelDevice()
{
	qDebug() << "DD: Babel: freeing device" << this->identifier << "/" << this->label;
}
