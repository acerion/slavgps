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




namespace SlavGPS {




	class LayerTRW;
	class Track;
	class BabelSomething;




	/**
	   Used when calling BabelCallback.
	*/
	typedef enum {
		BABEL_DIAG_OUTPUT, /* A line of diagnostic output is available. The pointer is to a NULL-terminated line of diagnostic output from gpsbabel. */
		BABEL_DONE,        /* gpsbabel finished, or NULL if no callback is needed. */
	} BabelProgressCode;




	/**
	   Callback function.
	*/
	typedef void (* BabelCallback)(BabelProgressCode, void *, void *);




	/**
	   All values are set to NULL by default.
	   Need to specify at least one of babel_args, URL or shell_command.
	*/
	struct ProcessOptions {
	public:
		ProcessOptions();
		ProcessOptions(const QString & babel_args, const QString & input_file_name, const QString & input_file_type, const QString & url);
		~ProcessOptions();

		QString babel_args;      /* The standard initial arguments to gpsbabel (if gpsbabel is to be used) - normally should include the input file type (-i) option. */
		QString input_file_name; /* Input filename (or device port e.g. /dev/ttyS0). */
		QString input_file_type; /* If empty, then uses internal file format handler (GPX only ATM), otherwise specify gpsbabel input type like "kml","tcx", etc... */
		QString url;             /* URL input rather than a filename. */
		QString babel_filters;   /* Optional filter arguments to gpsbabel. */
		QString shell_command;   /* Optional shell command to run instead of gpsbabel - but will be (Unix) platform specific. */
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




	bool a_babel_convert_import(LayerTRW * trw, ProcessOptions *process_options, DownloadOptions * dl_options, BabelSomething * babel_something);
	bool a_babel_convert_export(LayerTRW * trw, Track * trk, const QString & babel_args, const QString & target_file_full_path, BabelSomething * babel_something);




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
		bool convert_through_intermediate_file(const QString & program, const QStringList & args, BabelSomething * babel_something, LayerTRW * trw, const QString & intermediate_file_path);

		QString gpsbabel_path; /* Path to gpsbabel. */
		QString unbuffer_path; /* Path to unbuffer. */



		/* Collection of file types supported by gpsbabel. */
		static std::map<int, BabelFileType *> file_types;

		/* List of devices supported by gpsbabel. */
		static std::vector<BabelDevice *> devices;
	};




	class BabelConverter : public QObject {
		Q_OBJECT
	public:
		BabelConverter(const QString & program, const QStringList & args, BabelSomething * babel_something);
		~BabelConverter();

		bool run_conversion(bool do_import);

		QProcess * process = NULL;

	public slots:
		void started_cb(void);
		void error_occurred_cb(QProcess::ProcessError error);
		void finished_cb(int exit_code, QProcess::ExitStatus exitStatus);
		void read_stdout_cb(void);

	private:
		BabelSomething * babel_something = NULL;
	};




	class BabelSomething : public QObject {
		Q_OBJECT
	public:
		BabelSomething() {};
		virtual void import_progress_cb(BabelProgressCode code, void * data) { return; };
		virtual void export_progress_cb(BabelProgressCode code, void * data) { return; };
	};




	class BabelFeatureLoader : public BabelSomething {
		Q_OBJECT
	public:
		BabelFeatureLoader() {};
		void import_progress_cb(BabelProgressCode code, void * data);
		void export_progress_cb(BabelProgressCode code, void * data) { return; };
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_BABEL_H_ */
