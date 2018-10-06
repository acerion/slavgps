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




#include <map>
#include <vector>




#include <QObject>
#include <QProcess>
#include <QStringList>




#include "download.h"
#include "acquire.h"




namespace SlavGPS {




	class LayerTRW;
	class Track;
	class GPXImporter;
	class BabelFeatureParser;
	class BabelLocalFileImporter;




	enum class BabelOptionsMode {
		None,
		FromURL,
		FromFile,
		FromShellCommand
	};




	class BabelProcess : public AcquireTool {
		Q_OBJECT
	public:
		BabelProcess();
		~BabelProcess();

		void set_args(const QStringList & args);
		void set_auxiliary_parameters(AcquireContext & acquire_context, DataProgressDialog * progr_dialog);

		/* Input file -> gpsbabel -> gpx format -> gpx importer -> trw layer. */
		bool convert_through_gpx(LayerTRW * trw);

		bool run_process(void);
		bool run_export(void);
		int kill(const QString & status);

		QProcess * process = NULL;

		GPXImporter * importer = NULL;
		BabelFeatureParser * feature_parser = NULL;
		DataProgressDialog * babel_progr_indicator = NULL;

		QString program_name;
		QStringList args;


	public slots:
		void started_cb(void);
		void error_occurred_cb(QProcess::ProcessError error);
		void finished_cb(int exit_code, QProcess::ExitStatus exitStatus);
		void read_stdout_cb(void);

	private:
		AcquireContext acquire_context;
	};




	/**
	   Need to specify at least one of babel_args, URL or shell_command.
	*/
	struct BabelOptions : public AcquireOptions {
	public:
		BabelOptions(const BabelOptions & other);
		BabelOptions(BabelOptionsMode new_mode) : mode(new_mode) { };
		virtual ~BabelOptions() {};

		bool universal_import_fn(LayerTRW * trw, DownloadOptions * dl_options, AcquireContext & acquire_context, DataProgressDialog * progr_dialog);
		bool import_from_url(LayerTRW * trw, DownloadOptions * dl_options, DataProgressDialog * progr_dialog);
		bool import_from_local_file(LayerTRW * trw, AcquireContext & acquire_context, DataProgressDialog * progr_dialog);
		bool import_with_shell_command(LayerTRW * trw, AcquireContext & acquire_context, DataProgressDialog * progr_dialog);

		bool is_valid(void) const;

		bool universal_export_fn(LayerTRW * trw, Track * trk, AcquireContext & acquire_context, DataProgressDialog * progr_dialog);

		bool turn_off_device(void);

		int kill(const QString & status);

		QString input;             /* Full path to input file, or input device (e.g. /dev/ttyS0), or URL. */
		QString input_data_format; /* If empty, then uses internal file format handler (GPX only ATM), otherwise specify gpsbabel input type like "kml","tcx", etc... */
		QString output;

		QString babel_args;      /* The standard initial arguments to gpsbabel (if gpsbabel is to be used) - normally should include the input file type (-i) option. */
		QString babel_filters;   /* Optional filter arguments to gpsbabel. */
		QString shell_command;   /* Optional shell command to run instead of gpsbabel - but will be (Unix) platform specific. */

		BabelOptionsMode mode;
		BabelProcess * babel_process = NULL;

		BabelLocalFileImporter * importer = NULL;
	};




	/**
	   Store the Read/Write support offered by gpsbabel for a given format.
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
	   Representation of a supported device.
	*/
	class BabelDevice {
	public:
		BabelDevice(const QString & new_mode, const QString & new_identifier, const QString & new_label);
		~BabelDevice();

		BabelMode mode;
		QString identifier;  /* gpsbabel's identifier of the device (is it a name of protocol as well?). */
		QString label;       /* Human readable label. */
	};




	/**
	   Representation of a supported file format.
	*/
	class BabelFileType {
	public:
		BabelFileType(const QString & new_mode, const QString & new_identifier, const QString & new_extension, const QString & new_label);
		~BabelFileType();

		BabelMode mode;
		QString identifier;  /* gpsbabel's identifier of the format. */
		QString extension;   /* File's extension for this format. */
		QString label;       /* Human readable label. */
	};




	class Babel {
	public:
		Babel() {};
		~Babel() {};

		static void init();
		static void post_init();
		static void uninit();

		static bool is_available(void);
		bool is_detected = false; /* Has gpsbabel been detected in the system and is available for operation? */

		void get_unbuffer_path_from_system(void);
		void get_gpsbabel_path_from_system(void);
		void get_gpsbabel_path_from_preferences(void);

		bool set_program_name(QString & program, QStringList & args);

		QString gpsbabel_path; /* Path to gpsbabel. */
		QString unbuffer_path; /* Path to unbuffer. */



		/* Collection of file types supported by gpsbabel. */
		static std::map<int, BabelFileType *> file_types;

		/* List of devices supported by gpsbabel. */
		static std::vector<BabelDevice *> devices;
	};




	/*
	  A special case of gpsbabel process.
	  Not used for import or export of geodata, but for loading of gpsbabel program features.
	*/
	class BabelFeatureLoader : public BabelProcess {
	public:
		BabelFeatureLoader();
		~BabelFeatureLoader();
	};

	class BabelFeatureParser : public BabelProcess {
	public:
		BabelFeatureParser() {};
		size_t write(const char * data, size_t size);
	};




	class BabelLocalFileImporter : public BabelProcess {
	public:
		BabelLocalFileImporter();
		BabelLocalFileImporter(const QString & file_full_path);
		BabelLocalFileImporter(const QString & file_full_path, const QString & file_type);
		~BabelLocalFileImporter();
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_BABEL_H_ */
