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




#include <cstdio>
#include <cstring>
#include <cstdlib>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif




#include <QTemporaryFile>
#include <QDebug>
#include <QStandardPaths>




#include "gpx.h"
#include "babel.h"
#include "file.h"
#include "util.h"
#include "vikutils.h"
#include "preferences.h"




using namespace SlavGPS;




extern bool vik_debug;




#define SG_MODULE "Babel"
#define PREFIX ": Babel:" << __FUNCTION__ << __LINE__ << ">"




/* TODO_MAYBE in the future we could have support for other shells (change command strings), or not use a shell at all. */
#define BASH_LOCATION "/bin/bash"




Babel babel;

/* Definitions of static members from Babel class. */
std::map<int, BabelFileType *> Babel::file_types;
std::vector<BabelDevice *> Babel::devices;

static int file_type_id = 0;




static ParameterSpecification prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_IO "gpsbabel", SGVariantType::String, PARAMETER_GROUP_GENERIC, QObject::tr("GPSBabel:"), WidgetType::FileSelector, NULL, NULL, NULL, N_("Allow setting the specific instance of GPSBabel. You must restart Viking for this value to take effect.") },
};




/* Path set here may be overwritten by path from preferences. */
void Babel::get_gpsbabel_path_from_system(void)
{
	/* The path may be empty string. */
	this->gpsbabel_path = QStandardPaths::findExecutable("gpsbabel");

	Preferences::register_parameter_instance(prefs[0], SGVariant(this->gpsbabel_path, prefs[0].type_id));

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
	const QString gpsbabel_path_prefs = Preferences::get_param_value(PREFERENCES_NAMESPACE_IO "gpsbabel").val_string;
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




BabelOptions::BabelOptions(const BabelOptions & other)
{
	this->input             = other.input;
	this->input_data_format = other.input_data_format;
	this->output            = other.output;
	this->babel_args        = other.babel_args;
	this->babel_filters     = other.babel_filters;
	this->shell_command     = other.shell_command;
	this->mode              = other.mode;
	/* Ignore babel_process field. */
}




/**
   Runs gpsbabel program with the \param args and uses the GPX module to
   import the GPX data into \param trw layer.

   \param trw: The TrackWaypoint Layer to save the data into.  If it
   is null it signifies that no data is to be processed, however the
   gpsbabel command is still ran as it can be for non-data related
   options eg: for use with the power off command - 'command_off'

   \param program: program name

   \param args: list of program's arguments (program's name is not on that list)

   \param cb: callback that is run upon new data from STDOUT (?).
   (TODO_2_LATER: STDERR would be nice since we usually redirect STDOUT)

   \param cb_data: data passed along to \param cb

   \return true on success
   \return false otherwise
*/
bool BabelProcess::convert_through_gpx(LayerTRW * trw)
{
	qDebug() << SG_PREFIX_I;

	this->importer = new GPXImporter(trw);


	if (!this->run_process()) {
		qDebug() << SG_PREFIX_E << "Conversion failed";
		return false;
	}
	this->importer->write("", 0); /* Just to ensure proper termination by GPX parser. */
	sleep(3);

	if (this->babel_progr_indicator) {
		this->babel_progr_indicator->set_headline(tr("Import completed"));
		this->babel_progr_indicator->set_current_status("");
	}

	if (trw == NULL) {
		/* No data actually required but still need to have run gpsbabel anyway
		   - eg using the device power command_off. */
		return true;
	}

	bool read_success = this->importer->status != XML_STATUS_ERROR;

	delete this->importer;

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
bool BabelOptions::import_from_local_file(LayerTRW * trw, AcquireContext & acquire_context, DataProgressDialog * progr_dialog)
{
	qDebug() << SG_PREFIX_I << "Importing from local file" << this->input;

	if (!babel.is_detected) {
		qDebug() << SG_PREFIX_E << "gpsbabel not found in PATH";
		return false;
	}


	QStringList args; /* Args list won't contain main application's name. */


	const QStringList sub_args = this->babel_args.split(" ", QString::SkipEmptyParts); /* Some version of gpsbabel can not take extra blank arg. */
	if (sub_args.size()) {
		args << sub_args;
	}


	args << "-f";
	args << this->input;


	if (!this->babel_filters.isEmpty()) {
		const QStringList sub_filters = this->babel_filters.split(" ", QString::SkipEmptyParts); /* Some version of gpsbabel can not take extra blank arg. */
		if (sub_filters.size()) {
			args << sub_filters;
		}
	}


	args << QString("-o");
	args << QString("gpx");


	args << QString("-F");
	args << "-"; /* Output data appearing on stdout of gpsbabel will be redirected to input of GPX importer. */


	this->babel_process = new BabelProcess();
	this->babel_process->set_args(args);
	this->babel_process->set_auxiliary_parameters(acquire_context, progr_dialog);
	const bool ret = this->babel_process->convert_through_gpx(trw);
	delete this->babel_process;


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
 * Uses Babel::convert_through_gpx() to actually run the command. This function
 * prepares the command and temporary file, and sets up the arguments for bash.
 */
bool BabelOptions::import_with_shell_command(LayerTRW * trw, AcquireContext & acquire_context, DataProgressDialog * progr_dialog)
{
	qDebug() << SG_PREFIX_I << "Initial form of shell command" << this->shell_command;

	QString full_shell_command;
	if (this->input_data_format.isEmpty()) {
		/* Output of command will be redirected to GPX importer through read_stdout_cb(). */
		full_shell_command = this->shell_command;
	} else {
		/* "-" indicates output to stdout; stdout will be redirected to GPX importer through read_stdout_cb(). */
		full_shell_command = QString("%1 | %2 -i %3 -f - -o gpx -F -").arg(this->shell_command).arg(babel.gpsbabel_path).arg(this->input_data_format);

	}
	qDebug() << SG_PREFIX_I << "Final form of shell command" << full_shell_command;

#ifdef K_TODO
	const QString program(BASH_LOCATION);
	const QStringList args(QStringList() << "-c" << full_shell_command);

	this->babel_process = new BabelProcess();
	this->babel_process->set_args(program, args);
	this->babel_process->set_auxiliary_parameters(acquire_context, progr_dialog);
	bool rv = this->babel_process->convert_through_gpx(trw);
	delete this->babel_process;
	return rv;
#else
	return true;
#endif
}




int BabelOptions::kill(const QString & status)
{
	if (this->babel_process && QProcess::NotRunning != this->babel_process->process->state()) {
		return this->babel_process->kill(status);
	} else {
		return -3;
	}
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
bool BabelOptions::import_from_url(LayerTRW * trw, DownloadOptions * dl_options, DataProgressDialog * progr_dialog)
{
	qDebug() << "II" PREFIX << "importing from URL" << this->input;

	/* If no download options specified, use defaults: */
	DownloadOptions babel_dl_options(2);
	if (dl_options) {
		babel_dl_options = *dl_options;
	}

	bool ret = false;

	qDebug() << "DD" PREFIX << "input data format =" << this->input_data_format << ", url =" << this->input;


	QTemporaryFile tmp_file;
	if (!SGUtils::create_temporary_file(tmp_file, "tmp-viking.XXXXXX")) {
		return false;
	}
	const QString target_file_full_path = tmp_file.fileName();
	qDebug() << SG_PREFIX_D << "Temporary file:" << target_file_full_path;
	tmp_file.remove(); /* Because we only needed to confirm that a path to temporary file is "available"? */

	DownloadHandle dl_handle(&babel_dl_options);
	const DownloadStatus download_status = dl_handle.perform_download(this->input, target_file_full_path);

	if (DownloadStatus::Success == download_status) {
		if (!this->input_data_format.isEmpty() || !this->babel_filters.isEmpty()) {

			BabelOptions opts_local_file(BabelOptionsMode::FromFile); /* TODO_MAYBE: maybe we could reuse 'this->' ? */
			opts_local_file.input = target_file_full_path;
			opts_local_file.babel_args = (!this->input_data_format.isEmpty()) ? QString(" -i %1").arg(this->input_data_format) : "";
			opts_local_file.babel_filters = this->babel_filters;

			AcquireContext acquire_context;
			ret = opts_local_file.import_from_local_file(trw, acquire_context, NULL);
		} else {
			/* Process directly the retrieved file. */
			qDebug() << SG_PREFIX_D << "Directly read GPX file" << target_file_full_path;
			FILE * file = fopen(target_file_full_path.toUtf8().constData(), "r");
			if (file) {
				ret = GPX::read_file(file, trw);
				fclose(file);
				file = NULL;
			}
		}
	}
	Util::remove(target_file_full_path);


	return ret;
}




/**
 * @trw: The TRW layer to place data into. Duplicate items will be overwritten.
 * @process_options: The options to control the appropriate processing function. See #BabelOptions for more detail.
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
bool BabelOptions::universal_import_fn(LayerTRW * trw, DownloadOptions * dl_options, AcquireContext & acquire_context, DataProgressDialog * progr_dialog)
{
	if (this->importer) {
		BabelLocalFileImporter * importer2 = new BabelLocalFileImporter();
		importer2->program_name = importer->program_name;
		importer2->args = importer->args;

		QStringList args;

#if 0
		const QStringList sub_args = this->babel_args.split(" ", QString::SkipEmptyParts); /* Some version of gpsbabel can not take extra blank arg. */
		if (sub_args.size()) {
			args << sub_args;
		}
#endif


		if (!this->babel_filters.isEmpty()) {
			const QStringList sub_filters = this->babel_filters.split(" ", QString::SkipEmptyParts); /* Some version of gpsbabel can not take extra blank arg. */
			if (sub_filters.size()) {
				args << sub_filters;
			}
		}

		args << QString("-o");
		args << QString("gpx");

		args << QString("-F");
		args << "-"; /* Output data appearing on stdout of gpsbabel will be redirected to input of GPX importer. */

		importer2->set_args(args);
		importer2->set_auxiliary_parameters(acquire_context, progr_dialog);
		const bool result = importer2->convert_through_gpx(trw);

		delete importer2;

		return result;
	}


	switch (this->mode) {
	case BabelOptionsMode::FromURL:
		return this->import_from_url(trw, dl_options, progr_dialog);

	case BabelOptionsMode::FromFile:
		return this->import_from_local_file(trw, acquire_context, progr_dialog);

	case BabelOptionsMode::FromShellCommand:
		return this->import_with_shell_command(trw, acquire_context, progr_dialog);

	default:
		qDebug() << "EE" PREFIX << "unexpected babel options mode" << (int) this->mode;
		return false;
	}
}




bool BabelOptions::turn_off_device()
{
	AcquireContext acquire_context;
	return this->universal_import_fn(NULL, NULL, acquire_context, NULL);
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
bool BabelOptions::universal_export_fn(LayerTRW * trw, Track * trk, AcquireContext & acquire_context, DataProgressDialog * progr_dialog)
{
	if (!babel.is_detected) {
		qDebug() << SG_PREFIX_E << "gpsbabel not found in PATH";
		return false;
	}


	QTemporaryFile tmp_file;
	if (!SGUtils::create_temporary_file(tmp_file, "tmp-viking.XXXXXX")) {
		return false;
	}
	const QString tmp_file_full_path = tmp_file.fileName();
	qDebug() << SG_PREFIX_D << "Temporary file:" << tmp_file_full_path;


	QStringList args;


	args << "-i";
	args << "gpx";


	const QStringList sub_args = this->babel_args.split(" ", QString::SkipEmptyParts); /* Some version of gpsbabel can not take extra blank arg. */
	if (sub_args.size()) {
		args << sub_args;
	}


	args << "-f";
	args << tmp_file_full_path;
	args << "-F";
	args << this->output;


	/* Now strips out invisible tracks and waypoints. */
	if (!VikFile::export_trw(trw, tmp_file_full_path, SGFileType::GPX, trk, false)) {
		qDebug() << SG_PREFIX_E << "Error exporting to" << tmp_file_full_path;
		return false;
	}


	BabelProcess converter;
	converter.set_args(args);
	converter.set_auxiliary_parameters(acquire_context, progr_dialog);
	return converter.run_export();
}




bool BabelOptions::is_valid(void) const
{
	return !this->babel_args.isEmpty()
		|| !this->input.isEmpty()
		|| !this->shell_command.isEmpty();
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
   Load a single feature stored in the given line
*/
size_t BabelFeatureParser::write(const char * data, size_t size)
{
	if (!data) {
		qDebug() << SG_PREFIX_E << "NULL argument pointer";
		return 0;
	}
	const QString line = QString(data);

	QStringList tokens = line.split('\t', QString::KeepEmptyParts);
	if (tokens.size() == 0) {
		qDebug() << SG_PREFIX_W << "Unexpected gpsbabel feature string" << line;
		return 0;
	}

	if ("serial" == tokens.at(0)) {
		if (tokens.size() != 6) {
			qDebug() << SG_PREFIX_W << "Unexpected gpsbabel feature string" << line;
		} else {
			BabelDevice * device = new BabelDevice(tokens.at(1), tokens.at(2), tokens.at(4));
			Babel::devices.push_back(device);
		}

	} else if ("file" == tokens.at(0)) {
		if (tokens.size() != 6) {
			qDebug() << SG_PREFIX_W << "Unexpected gpsbabel format string" << line;
		} else {
			BabelFileType * file_type = new BabelFileType(tokens.at(1), tokens.at(2), tokens.at(3), tokens.at(4));
			Babel::file_types.insert({{ file_type_id, file_type }});

			file_type_id++;
		}
	} else {
		/* Ignore. */
	}

	return size;
}




BabelFeatureLoader::BabelFeatureLoader()
{
	this->feature_parser = new BabelFeatureParser();
}




BabelFeatureLoader::~BabelFeatureLoader()
{
	delete this->feature_parser;
}




static bool load_babel_features(void)
{
	if (!babel.is_detected) {
		qDebug() << SG_PREFIX_E << "gpsbabel not found in PATH";
		return false;
	}

	QStringList args = { QString("-^3") };

	BabelFeatureLoader feature_loader;
	feature_loader.set_args(args);
	return feature_loader.run_process();
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
			delete (*iter).second;
			Babel::file_types.erase(iter);
		}
	}

	if (Babel::devices.size()) {
		for (auto iter = Babel::devices.begin(); iter != Babel::devices.end(); iter++) {
			delete *iter;
			Babel::devices.erase(iter);
		}
	}
}




/**
   \brief Indicates if babel is available or not.

   FIXME: what about Babel::is_detected?

   Returns: true if babel available.
*/
bool Babel::is_available(void)
{
	return !Babel::devices.empty();
}




BabelProcess::BabelProcess()
{
	this->process = new QProcess();

	QObject::connect(this->process, SIGNAL (started()), this, SLOT (started_cb()));
	QObject::connect(this->process, SIGNAL (finished(int, QProcess::ExitStatus)), this, SLOT (finished_cb(int, QProcess::ExitStatus)));
	QObject::connect(this->process, SIGNAL (errorOccurred(QProcess::ProcessError)), this, SLOT (error_occurred_cb(QProcess::ProcessError)));
	QObject::connect(this->process, SIGNAL (readyReadStandardOutput()), this, SLOT (read_stdout_cb()));

	babel.set_program_name(this->program_name, this->args);

	this->process->setProgram(this->program_name);
}




void BabelProcess::set_args(const QStringList & more_args)
{
	this->args += more_args; /* Babel::set_program_name() may have already added one arg to the list. */

	this->process->setArguments(this->args);

	if (true || vik_debug) {
		qDebug() << SG_PREFIX_D << "Program is" << this->program_name;
		for (int i = 0; i < this->args.size(); i++) {
			qDebug() << SG_PREFIX_D << "Arg no." << i << "=" << this->args.at(i);
		}
	}
}




void BabelProcess::set_auxiliary_parameters(AcquireContext & new_acquire_context, DataProgressDialog * progr_dialog)
{
	this->acquire_context.window = new_acquire_context.window;
	this->acquire_context.viewport = new_acquire_context.viewport;
	this->acquire_context.top_level_layer = new_acquire_context.top_level_layer;
	this->acquire_context.selected_layer = new_acquire_context.selected_layer;
	this->acquire_context.target_trw = new_acquire_context.target_trw; /* TODO: call to configure_target_layer() may overwrite ::target_trw. */
	this->acquire_context.target_trk = new_acquire_context.target_trk;
	this->acquire_context.target_trw_allocated = new_acquire_context.target_trw_allocated;

	this->babel_progr_indicator = progr_dialog;
}




BabelProcess::~BabelProcess()
{
	delete this->process;
}




bool BabelProcess::run_process(void)
{
	qDebug() << SG_PREFIX_I;

	qDebug() << SG_PREFIX_I << "    " << this->program_name;
	for (int i = 0; i < this->args.size(); i++) {
		qDebug() << SG_PREFIX_I << "    " << this->args.at(i);
	}

	qDebug() << SG_PREFIX_I;
	this->process->start();
	qDebug() << SG_PREFIX_I;
	this->process->waitForFinished(-1);
	qDebug() << SG_PREFIX_I;

#ifdef K_TODO
	if (this->progress_indicator) { /* TODO_2_LATER: in final version there will be no 'progress_indicator' member, we will simply use import/export_progress_cb() methods. */
		this->progress_indicator->import_progress_cb(AcquireProgressCode::Completed, NULL);
	} else {
		this->import_progress_cb(AcquireProgressCode::Completed, NULL);
	}
#endif

	return true;
}




bool BabelProcess::run_export(void)
{
	this->process->start();
	this->process->waitForFinished(-1);

#ifdef K_TODO
	if (this->progress_indicator) { /* TODO_2_LATER: in final version there will be no 'progress_indicator' member, we will simply use import/export_progress_cb() methods. */
		this->progress_indicator->export_progress_cb(AcquireProgressCode::Completed, NULL);
	} else {
		this->export_progress_cb(AcquireProgressCode::Completed, NULL);
	}
#endif

	return true;
}




int BabelProcess::kill(const QString & status)
{
	if (this->process) {
		qDebug() << "II" PREFIX << "killing process" << status;
		this->process->kill();
		return 0;
	} else {
		qDebug() << "WW" PREFIX << "process doesn't exit" << status;
		return -2;
	}
}




void BabelProcess::started_cb(void)
{
	qDebug() << "II: Babel Converter: process started";
}




void BabelProcess::error_occurred_cb(QProcess::ProcessError error)
{
	qDebug() << "II: Babel Converter: error occurred when running process:" << error;
}




void BabelProcess::finished_cb(int exit_code, QProcess::ExitStatus exitStatus)
{
	qDebug() << "II: Babel Converter: process finished with exit code" << exit_code << "and exit status" << exitStatus;

	QByteArray ba = this->process->readAllStandardOutput();
	qDebug() << "II: Babel Converter: stdout: '" << ba.data() << "'";

	ba = this->process->readAllStandardError();
	qDebug() << "II: Babel Converter: stderr: '" << ba.data() << "'";
}




void BabelProcess::read_stdout_cb()
{
 	char buffer[512];

	qDebug() << SG_PREFIX_E;

	while (this->process->canReadLine()) {
		const qint64 read_size = this->process->readLine(buffer, sizeof (buffer));

		if (read_size < 0) {
			qDebug() << SG_PREFIX_E << "Negative read from gpsbabel's stdout";
			continue;
		}

		//qDebug() << SG_PREFIX_D << "Read gpsbabel stdout" << buffer;

		if (this->importer) {
			this->importer->write(buffer, read_size);
		} else if (this->feature_parser) {
			this->feature_parser->write(buffer, read_size);
		} else {
			; /* No next step in chain. */
		}

		if (this->babel_progr_indicator) {
			static int i = 0;
			if (i % 200 == 0) {
				char current_status[20];
				snprintf(current_status, std::min(sizeof (current_status), (size_t) read_size), "%s", buffer);
				this->babel_progr_indicator->set_current_status(QString(current_status).replace('\n', ' '));
				usleep(1000);
			}
			i++;
		}
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





BabelLocalFileImporter::BabelLocalFileImporter() {}
BabelLocalFileImporter::~BabelLocalFileImporter() {}


BabelLocalFileImporter::BabelLocalFileImporter(const QString & file_full_path)
{
	this->args << "-f";
	this->args << file_full_path;
}




BabelLocalFileImporter::BabelLocalFileImporter(const QString & file_full_path, const QString & file_type)
{
	/* Input file type must be before input file path. */

	this->args << "-i";
	this->args << file_type;

	this->args << "-f";
	this->args << file_full_path;
}
