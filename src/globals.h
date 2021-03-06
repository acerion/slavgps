/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2018, Kamil Ignacak <acerion@wp.pl>
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

#ifndef _SG_GLOBALS_H_
#define _SG_GLOBALS_H_




#include <QObject>
#include <QString>
#include <QDebug>




namespace SlavGPS {




	class SGObjectTypeID {
		friend QDebug operator<<(QDebug debug, const SGObjectTypeID & id);
	public:
		/**
		   Used by QT's metatype system, and to create empty
		   (non-initialized) objects.
		*/
		SGObjectTypeID();

		/**
		   @param label is mandatory. It is used only for
		   debugs, but if you pass NULL or empty string, you
		   will get empty type id object.
		*/
		SGObjectTypeID(const char * label);
		// SGObjectTypeID(const QString & val);

		bool operator==(const SGObjectTypeID & other) const;
		bool operator!=(const SGObjectTypeID & other) const { return !(*this == other); }

		bool is_empty(void) const;

		/* Comparison class for std::maps with SGObjectTypeID as a key. */
		struct compare {
			bool operator() (const SGObjectTypeID & id1, const SGObjectTypeID & id2) const { return id1.m_val < id2.m_val; }
		};

	protected:
		int m_val = 0; /* Zero means empty type id. */
		char m_debug_string[100] = { '\0' };
	};
	QDebug operator<<(QDebug debug, const SGObjectTypeID & id);




	typedef int param_id_t; /* This shall be a signed type. */



	/* Function return value. */
	enum class sg_ret : int {
		err_cond,  /* Conditions necessary to perform operation were not met. */
		err_algo,  /* Function algorithm error. */
		err_arg,   /* Function arguments error. */
		err_null_ptr, /* Some pointer variable should be non-null, but is null (is not constructed properly (yet)). */
		err,       /* General error. */
		ok = 0,    /* Function completed without errors. */
	};




	/* Status of "load from file" operation. */
	class LoadStatus {
	public:

		enum class Code {
			Success,
			GenericError,           /* Generic error code. */
			InternalLogicError,     /* Error in code logic. */
			ParseError,             /* Error encountered during parsing of file (or during read operation). */
			ParseWarning, /* Non-critical problems during parsing of a file. */
			CantOpenFileError,      /* Can't open target file for reading. */
			CantOpenIntermediateFileError, /* Can't open intermediate file for reading/writing. */
			GPSBabelError,
			GPXError,
			UnsupportedFileTypeError,
		};
		LoadStatus() {}
		LoadStatus(LoadStatus::Code new_code) { this->code = new_code; }

		void show_status_dialog(QWidget * parent = NULL) const;

		LoadStatus & operator=(LoadStatus::Code new_code) { this->code = new_code; return *this; }

		LoadStatus::Code code = LoadStatus::Code::GenericError;

		/* Additional info from parser. */
		int parser_line = -1;
		QString parser_message;

		QString get_message_string(void) const;
	};
	bool operator==(LoadStatus::Code code, const LoadStatus & lhs);
	bool operator!=(LoadStatus::Code code, const LoadStatus & lhs);
	QDebug operator<<(QDebug debug, const LoadStatus & save_status);




	/* Status of "save to file" operation. */
	class SaveStatus {
	public:
		enum class Code {
			Success,
			GenericError,           /* Generic error code. */
			InternalLogicError,     /* Error in code logic. */
			CantOpenFileError,      /* Can't open target file for writing. */
			CantOpenIntermediateFileError, /* Can't open intermediate file for reading/writing. */
			GPSBabelError,
		};

		SaveStatus() {}
		SaveStatus(SaveStatus::Code new_code) { this->code = new_code; }

		void show_status_dialog(QWidget * parent = NULL) const;

		SaveStatus & operator=(SaveStatus::Code new_code) { this->code = new_code; return *this; }

		SaveStatus::Code code = SaveStatus::Code::GenericError;

		QString get_message_string(void) const;
	};
	bool operator==(SaveStatus::Code code, const SaveStatus & lhs);
	bool operator!=(SaveStatus::Code code, const SaveStatus & lhs);
	QDebug operator<<(QDebug debug, const SaveStatus & save_status);






#ifndef MSECS_PER_SEC
#define MSECS_PER_SEC 1000
#endif

#ifndef NANOSECS_PER_SEC
#define NANOSECS_PER_SEC 1000000000UL
#endif




#define SG_NAMESPACE_PREFIX "SlavGPS::"

#define SG_PREFIX_D      "DD  " << SG_MODULE <<  " > " << QString(__PRETTY_FUNCTION__).replace("std::__cxx11::", "").replace(SG_NAMESPACE_PREFIX, "").replace("virtual ", "").replace("const ", "") << __LINE__ << " > "
#define SG_PREFIX_I      "II  " << SG_MODULE <<  " > " << QString(__PRETTY_FUNCTION__).replace("std::__cxx11::", "").replace(SG_NAMESPACE_PREFIX, "").replace("virtual ", "").replace("const ", "") << __LINE__ << " > "
#define SG_PREFIX_N      "NN  " << SG_MODULE <<  " > " << QString(__PRETTY_FUNCTION__).replace("std::__cxx11::", "").replace(SG_NAMESPACE_PREFIX, "").replace("virtual ", "").replace("const ", "") << __LINE__ << " > "
#define SG_PREFIX_W      "WW  " << SG_MODULE <<  " > " << QString(__PRETTY_FUNCTION__).replace("std::__cxx11::", "").replace(SG_NAMESPACE_PREFIX, "").replace("virtual ", "").replace("const ", "") << __LINE__ << " > "
#define SG_PREFIX_E      "EE  " << SG_MODULE <<  " > " << QString(__PRETTY_FUNCTION__).replace("std::__cxx11::", "").replace(SG_NAMESPACE_PREFIX, "").replace("virtual ", "").replace("const ", "") << __LINE__ << " > "
#define SG_PREFIX_SLOT   "SLT " << SG_MODULE <<  " > " << QString(__PRETTY_FUNCTION__).replace("std::__cxx11::", "").replace(SG_NAMESPACE_PREFIX, "").replace("virtual ", "").replace("const ", "") << __LINE__ << " > "
#define SG_PREFIX_SIGNAL "SIG " << SG_MODULE <<  " > " << QString(__PRETTY_FUNCTION__).replace("std::__cxx11::", "").replace(SG_NAMESPACE_PREFIX, "").replace("virtual ", "").replace("const ", "") << __LINE__ << " > "




/* TODO_2_LATER: provide correct URLs for SlavGPS. */
#define SG_URL_VERSION_FILE     "http://sourceforge.net/projects/viking/files/VERSION"
#define SG_URL_MAIN_PAGE        "http://sourceforge.net/projects/viking/"




} /* namespace SlavGPS */




Q_DECLARE_METATYPE(SlavGPS::SGObjectTypeID)




#endif /* #ifndef _SG_GLOBALS_H_ */
