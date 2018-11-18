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
#include "layer_trw.h"
#include "layer_aggregate.h"




using namespace SlavGPS;




extern bool vik_debug;




#define SG_MODULE "Babel"




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
		qDebug() << SG_PREFIX_W << "gpsbabel not found in PATH";
	} else {
		qDebug() << SG_PREFIX_I << "Path to gpsbabel initialized as" << this->gpsbabel_path;
	}
}




/* Path set here may be overwritten by path from preferences. */
void Babel::get_unbuffer_path_from_system(void)
{
	this->unbuffer_path = QStandardPaths::findExecutable("unbuffer");

	if (this->unbuffer_path.isEmpty()) {
		qDebug() << SG_PREFIX_W << "unbuffer not found in PATH";
	} else {
		qDebug() << SG_PREFIX_I << "Path to unbuffer initialized as" << this->unbuffer_path;
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
		qDebug() << SG_PREFIX_I << "Path to gpsbabel set from preferences as" << this->gpsbabel_path;
	}

	if (!this->gpsbabel_path.isEmpty()) {
		this->gpsbabel_is_available = true;
		qDebug() << SG_PREFIX_I << "gpsbabel detected as" << this->gpsbabel_path;
	} else {
		qDebug() << SG_PREFIX_W << "gpsbabel not detected";
	}
}




/*
  Call the function when you need to decide if the main program should
  be 'unbuffer' or 'gpsbabel'.
*/
bool Babel::set_program_name(QString & program_name, QString & first_arg)
{
	if (this->unbuffer_path.isEmpty()) {
		program_name = this->gpsbabel_path;
	} else {
		program_name = this->unbuffer_path;
		first_arg = this->gpsbabel_path;
	}

	return true;
}




void BabelProcess::set_options(const QString & new_options)
{
	this->options = new_options;
}




void BabelProcess::set_input(const QString & file_type, const QString & file_full_path)
{
	this->input_type = file_type;
	this->input_file = file_full_path;
}




void BabelProcess::set_filters(const QString & new_filters)
{
	this->filters = new_filters;
}




void BabelProcess::set_output(const QString & file_type, const QString & file_full_path)
{
	this->output_type = file_type;
	this->output_file = file_full_path;
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
sg_ret BabelProcess::convert_through_gpx(LayerTRW * trw)
{
	qDebug() << SG_PREFIX_I;

	this->gpx_importer = new GPXImporter(trw);


	if (!this->run_process()) {
		qDebug() << SG_PREFIX_E << "Conversion failed";
		return sg_ret::err;
	}
	this->gpx_importer->write("", 0); /* Just to ensure proper termination by GPX parser. */
	sleep(3);

	if (this->babel_progr_indicator) {
		this->babel_progr_indicator->set_headline(tr("Import completed"));
		this->babel_progr_indicator->set_current_status("");
	}

	if (trw == NULL) {
		/* No data actually required but still need to have run gpsbabel anyway
		   - eg using the device power command_off. */
		return sg_ret::ok;
	}

	const sg_ret read_success = this->gpx_importer->status != XML_STATUS_ERROR ? sg_ret::ok : sg_ret::err;

	delete this->gpx_importer;

	return read_success;
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




bool BabelFeatureLoader::run_process(void)
{
	qDebug() << SG_PREFIX_I;

	QStringList args;

	/* There isn't much variation in arguments for feature
	   loading. Therefore we don't use
	   BabelProcess::set_arguments(), we set the arguments here
	   instead. */

	if (!this->first_arg.isEmpty()) {
		args << this->first_arg;
	}
	args << "-^3";

	qDebug() << SG_PREFIX_I << "   Program name:" << this->program_name;
	for (int i = 0; i < args.size(); i++) {
		qDebug() << SG_PREFIX_I << "   Arg:" << args.at(i);
	}


	this->process->setProgram(this->program_name);
	this->process->setArguments(args);


	this->process->start();
	this->process->waitForFinished(-1);

	return true;
}




static bool load_babel_features(void)
{
	if (Babel::is_available()) {
		BabelFeatureLoader feature_loader;
		return feature_loader.run_process();
	} else {
		qDebug() << SG_PREFIX_W << "Can't load features, gpsbabel not found";
		return false;
	}
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




bool Babel::is_available(void)
{
	return babel.gpsbabel_is_available;
}




BabelProcess::BabelProcess()
{
	this->process = new QProcess();

	QObject::connect(this->process, SIGNAL (started()), this, SLOT (started_cb()));
	QObject::connect(this->process, SIGNAL (finished(int, QProcess::ExitStatus)), this, SLOT (finished_cb(int, QProcess::ExitStatus)));
	QObject::connect(this->process, SIGNAL (errorOccurred(QProcess::ProcessError)), this, SLOT (error_occurred_cb(QProcess::ProcessError)));
	QObject::connect(this->process, SIGNAL (readyReadStandardOutput()), this, SLOT (read_stdout_cb()));

	babel.set_program_name(this->program_name, this->first_arg);

	this->process->setProgram(this->program_name);
}




void BabelProcess::set_acquire_context(AcquireContext * new_acquire_context)
{
	this->acquire_context = new_acquire_context;
}




void BabelProcess::set_progress_dialog(AcquireProgressDialog * progr_dialog)
{
	this->babel_progr_indicator = progr_dialog;
}




BabelProcess::~BabelProcess()
{
	delete this->process;
}




bool BabelProcess::run_process(void)
{
	qDebug() << SG_PREFIX_I;


	QStringList args;

	if (!this->first_arg.isEmpty()) {
		args << this->first_arg;
	}

	if (!this->options.isEmpty()) {
		const QStringList opts = this->options.split(" ", QString::SkipEmptyParts); /* Some version of gpsbabel can not take extra blank arg. */
		if (opts.size()) {
			args << opts;
		}
	}

	args << "-i";
	if (this->input_type.isEmpty()) {
		qDebug() << SG_PREFIX_N << "Empty input type. Defaulting to 'gpx'";
		args << "gpx";
	} else {
		args << this->input_type;
	}
	args << "-f";
	args << this->input_file;

	if (!this->filters.isEmpty()) {
		const QStringList sub_filters = this->filters.split(" ", QString::SkipEmptyParts); /* Some version of gpsbabel can not take extra blank arg. */
		if (sub_filters.size()) {
			args << sub_filters;
		}
	}

	args << "-o";
	args << this->output_type;
	args << "-F";
	args << this->output_file;


	qDebug() << SG_PREFIX_I << "   Program name:" << this->program_name;
	for (int i = 0; i < args.size(); i++) {
		qDebug() << SG_PREFIX_I << "   Arg:" << args.at(i);
	}


	this->process->setProgram(this->program_name);
	this->process->setArguments(args);


	this->process->start();
	this->process->waitForFinished(-1);

#ifdef K_TODO
	if (this->progress_indicator) { /* TODO_2_LATER: in final version there will be no 'progress_indicator' member, we will simply use import/export_progress_cb() methods. */
		this->progress_indicator->import_progress_cb(AcquireProgressCode::Completed, NULL);
	} else {
		this->import_progress_cb(AcquireProgressCode::Completed, NULL);
	}
#endif

	return true;
}




int BabelProcess::kill(const QString & status)
{
	if (this->process) {
		qDebug() << SG_PREFIX_I << "Killing process" << status;
		this->process->kill();
		return 0;
	} else {
		qDebug() << SG_PREFIX_W << "Process doesn't exit" << status;
		return -2;
	}
}




void BabelProcess::started_cb(void)
{
	qDebug() << SG_PREFIX_I << "Process started";
}




void BabelProcess::error_occurred_cb(QProcess::ProcessError error)
{
	qDebug() << SG_PREFIX_I << "Error occurred when running process:" << error;
}




void BabelProcess::finished_cb(int exit_code, QProcess::ExitStatus exitStatus)
{
	qDebug() << SG_PREFIX_I << "Process finished with exit code" << exit_code << "and exit status" << exitStatus;

	QByteArray ba = this->process->readAllStandardOutput();
	qDebug() << SG_PREFIX_I << "stdout: '" << ba.data() << "'";

	ba = this->process->readAllStandardError();
	qDebug() << SG_PREFIX_I << "stderr: '" << ba.data() << "'";
}




void BabelProcess::read_stdout_cb()
{
	qDebug() << SG_PREFIX_D;

 	char buffer[512];

	while (this->process->canReadLine()) {
		const qint64 read_size = this->process->readLine(buffer, sizeof (buffer));

		if (read_size < 0) {
			qDebug() << SG_PREFIX_E << "Negative read from gpsbabel's stdout";
			continue;
		}

		//qDebug() << SG_PREFIX_D << "Read gpsbabel stdout" << buffer;

		if (this->gpx_importer) {
			this->gpx_importer->write(buffer, read_size);
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




/**
   @brief Export data using gpsbabel

   @trw: The TRW layer from which data is taken.
   @trk: Operate on the individual track if specified. Use NULL when operating on a TRW layer.

   @return true on successful invocation of GPSBabel command
   @return false otherwise
*/
sg_ret BabelProcess::export_through_gpx(LayerTRW * trw, Track * trk)
{
	if (!Babel::is_available()) {
		qDebug() << SG_PREFIX_E << "gpsbabel not found in PATH";
		return sg_ret::err;
	}


	QTemporaryFile tmp_file;
	if (!SGUtils::create_temporary_file(tmp_file, "tmp-viking.XXXXXX")) {
		return sg_ret::err;
	}
	const QString tmp_file_full_path = tmp_file.fileName();
	qDebug() << SG_PREFIX_D << "Temporary file:" << tmp_file_full_path;


	/* Now strips out invisible tracks and waypoints. */
	if (!VikFile::export_trw(trw, tmp_file_full_path, SGFileType::GPX, trk, false)) {
		qDebug() << SG_PREFIX_E << "Error exporting to" << tmp_file_full_path;
		return sg_ret::err;
	}

	this->set_input("gpx", tmp_file_full_path);

	return this->run_process() ? sg_ret::ok : sg_ret::err;
}




QString BabelProcess::get_trw_string(bool do_tracks, bool do_routes, bool do_waypoints)
{
	const QString result = QString("%1 %2 %3").arg(do_tracks ? "-t" : "").arg(do_routes ? "-r" : "").arg(do_waypoints ? "-w" : "");
	return result;
}




BabelFileType::BabelFileType(const QString & new_mode, const QString & new_identifier, const QString & new_extension, const QString & new_label)
{
	set_mode(&this->mode, new_mode);
	this->identifier = new_identifier;
	this->extension = new_extension;
	this->label = new_label;

#if 1
	qDebug() << SG_PREFIX_I << "gpsbabel file type #"
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
	qDebug() << SG_PREFIX_D << "Delete Babel file type" << this->identifier << "/" << this->label;
}




BabelDevice::BabelDevice(const QString & new_mode, const QString & new_identifier, const QString & new_label)
{
	set_mode(&this->mode, new_mode);
	this->identifier = new_identifier;
	this->label = new_label.left(50); /* Limit really long label text. */

#if 1
	qDebug() << SG_PREFIX_D << "New gpsbabel device:"
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
	qDebug() << SG_PREFIX_D << "Delete Babel device" << this->identifier << "/" << this->label;
}




bool BabelTurnOffDevice::run_process(void)
{
	qDebug() << SG_PREFIX_I;

	QStringList args;

	/* There isn't much variation in arguments for turning off
	   device. Therefore we don't use
	   BabelProcess::set_arguments(), we set the arguments here
	   instead. */

	if (!this->first_arg.isEmpty()) {
		args << this->first_arg;
	}

	args << "-i";
	args << QString("%1,%2").arg(this->protocol).arg("power_off");
	args << "-f";
	args << this->port;


	qDebug() << SG_PREFIX_I << "   Program name:" << this->program_name;
	for (int i = 0; i < args.size(); i++) {
		qDebug() << SG_PREFIX_I << "   Arg:" << args.at(i);
	}


	this->process->setProgram(this->program_name);
	this->process->setArguments(args);


	this->process->start();
	this->process->waitForFinished(-1);

	return true;
}
