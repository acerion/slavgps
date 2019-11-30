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




#include <QString>
#include <QDebug>




namespace SlavGPS {




	class SGObjectTypeID {
	public:
		SGObjectTypeID() {}
		SGObjectTypeID(const QString & val) : m_val(val) {}
		SGObjectTypeID(const char * val) : m_val(val) {}

		bool operator==(const SGObjectTypeID & other) const;
		bool operator==(const QString & other) const;
		bool operator==(const char * other) const;

		bool operator!=(const SGObjectTypeID & other) const { return !(*this == other); }
		bool operator!=(const QString & other) const { return !(*this == other); }
		bool operator!=(const char * other) const { return !(*this == other); }

		QString m_val;
	};
	QDebug operator<<(QDebug debug, const SGObjectTypeID & id);

	/* Comparison class for std::maps with SGObjectTypeID as a key. */
	struct sg_object_type_id_compare {
		bool operator() (const SGObjectTypeID & id1, const SGObjectTypeID & id2) const { return id1.m_val < id2.m_val; }
	};
#define SG_OBJ_TYPE_ID_ANY                      ""
#define SG_OBJ_TYPE_ID_TRW_SINGLE_TRACK         "sg.trw.track"
#define SG_OBJ_TYPE_ID_TRW_SINGLE_ROUTE         "sg.trw.route"
#define SG_OBJ_TYPE_ID_TRW_SINGLE_WAYPOINT      "sg.trw.waypoint"
#define SG_OBJ_TYPE_ID_TRW_TRACKS               "sg.trw.tracks"
#define SG_OBJ_TYPE_ID_TRW_ROUTES               "sg.trw.routes"
#define SG_OBJ_TYPE_ID_TRW_WAYPOINTS            "sg.trw.waypoints"

/* Globally unique tool IDS. */
#define LAYER_TRW_TOOL_CREATE_WAYPOINT "sg.tool.layer_trw.create_waypoint"
#define LAYER_TRW_TOOL_CREATE_TRACK    "sg.tool.layer_trw.create_track"
#define LAYER_TRW_TOOL_CREATE_ROUTE    "sg.tool.layer_trw.create_route"
#define LAYER_TRW_TOOL_ROUTE_FINDER    "sg.tool.layer_trw.route_finder"
#define LAYER_TRW_TOOL_EDIT_WAYPOINT   "sg.tool.layer_trw.edit_waypoint"
#define LAYER_TRW_TOOL_EDIT_TRACKPOINT "sg.tool.layer_trw.edit_trackpoint"
#define LAYER_TRW_TOOL_SHOW_PICTURE    "sg.tool.layer_trw.show_picture"




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
			OtherSuccess,
			Error,                  /* General error. */
			InternalError,          /* Error in code logic. */
			ReadFailure,
			FileAccess,             /* Can't open target file for reading. */
			IntermediateFileAccess, /* Can't open intermediate file for reading/writing. */
			GPSBabelFailure,
			GPXFailure,
			UnsupportedFailure,
			FailureNonFatal,
		};
		LoadStatus() {}
		LoadStatus(LoadStatus::Code new_code) { this->code = new_code; }

		void show_error_dialog(QWidget * parent = NULL) const;

		LoadStatus & operator=(LoadStatus::Code new_code) { this->code = new_code; return *this; }

		LoadStatus::Code code = LoadStatus::Code::Error;
	};
	bool operator==(LoadStatus::Code code, const LoadStatus & lhs);
	bool operator!=(LoadStatus::Code code, const LoadStatus & lhs);
	QDebug operator<<(QDebug debug, const LoadStatus & save_status);




	/* Status of "save to file" operation. */
	class SaveStatus {
	public:
		enum class Code {
			Success,
			Error,                  /* General error. */
			InternalError,          /* Error in code logic. */
			FileAccess,             /* Can't open target file for writing. */
			IntermediateFileAccess, /* Can't open intermediate file for reading/writing. */
		};

		SaveStatus() {}
		SaveStatus(SaveStatus::Code new_code) { this->code = new_code; }

		void show_error_dialog(QWidget * parent = NULL) const;

		SaveStatus & operator=(SaveStatus::Code new_code) { this->code = new_code; return *this; }

		SaveStatus::Code code = SaveStatus::Code::Error;
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




#endif /* #ifndef _SG_GLOBALS_H_ */
