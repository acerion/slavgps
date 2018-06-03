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




	/**
	   Need to specify at least one of babel_args, URL or shell_command.
	*/
	struct BabelOptions : public AcquireOptions {
	public:
		BabelOptions() {};
		BabelOptions(const QString & babel_args, const QString & input_file_name, const QString & input_file_type, const QString & url);
		~BabelOptions() {};

		bool universal_import_fn(LayerTRW * trw, DownloadOptions * dl_options, AcquireTool * progress_indicator);
		bool import_from_url(LayerTRW * trw, DownloadOptions * dl_options);
		bool import_from_local_file(LayerTRW * trw, AcquireTool * progress_indicator);
		bool import_with_shell_command(LayerTRW * trw, AcquireTool * progress_indicator);

		bool is_valid(void) const;

		bool universal_export_fn(LayerTRW * trw, Track * trk, AcquireTool * babel_something);

		bool turn_off_device(void);

		QString babel_args;      /* The standard initial arguments to gpsbabel (if gpsbabel is to be used) - normally should include the input file type (-i) option. */
		QString input_file_name; /* Input filename (or device port e.g. /dev/ttyS0). */
		QString input_file_type; /* If empty, then uses internal file format handler (GPX only ATM), otherwise specify gpsbabel input type like "kml","tcx", etc... */
		QString url;             /* URL input rather than a filename. */
		QString babel_filters;   /* Optional filter arguments to gpsbabel. */
		QString shell_command;   /* Optional shell command to run instead of gpsbabel - but will be (Unix) platform specific. */
		QString output_file_full_path;
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
		bool convert_through_intermediate_file(const QString & program, const QStringList & args, AcquireTool * progress_indicator, LayerTRW * trw, const QString & intermediate_file_path);

		QString gpsbabel_path; /* Path to gpsbabel. */
		QString unbuffer_path; /* Path to unbuffer. */



		/* Collection of file types supported by gpsbabel. */
		static std::map<int, BabelFileType *> file_types;

		/* List of devices supported by gpsbabel. */
		static std::vector<BabelDevice *> devices;
	};




	class BabelProcess : public AcquireTool {
		Q_OBJECT
	public:
		BabelProcess(const QString & program, const QStringList & args, AcquireTool * new_progress_indicator);
		~BabelProcess();

		bool run_process(bool do_import);

		QProcess * process = NULL;

	public slots:
		void started_cb(void);
		void error_occurred_cb(QProcess::ProcessError error);
		void finished_cb(int exit_code, QProcess::ExitStatus exitStatus);
		void read_stdout_cb(void);

	private:
		AcquireTool * progress_indicator = NULL;
	};




	class BabelFeatureLoader : public BabelProcess {
	public:
		BabelFeatureLoader(const QString & program, const QStringList & args, AcquireTool * new_progress_indicator)
			: BabelProcess(program, args, NULL) {};
		void import_progress_cb(AcquireProgressCode code, void * data);
		void export_progress_cb(AcquireProgressCode code, void * data) { return; };
	};




	/* Parent class for data sources that have the same process
	   function: universal_import_fn(), called either directly or
	   indirectly. */
	class DataSourceBabel : public DataSource {
	public:
		DataSourceBabel() {};
		~DataSourceBabel() {};

		virtual bool acquire_into_layer(LayerTRW * trw, AcquireTool * babel_something);
		virtual void cleanup(void * data) { return; };

		virtual int run_config_dialog(AcquireProcess * acquire_context) { return QDialog::Rejected; };
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_BABEL_H_ */
