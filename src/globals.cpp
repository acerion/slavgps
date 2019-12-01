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




#include <mutex>




#include "globals.h"
#include "dialog.h"




using namespace SlavGPS;




#define SG_MODULE "Globals"




static int g_object_type_id_counter;
static std::mutex g_object_type_id_mutex;




QDebug SlavGPS::operator<<(QDebug debug, const LoadStatus & load_status)
{
	switch (load_status.code) {
	case LoadStatus::Code::Success:
		debug << "Success";
		break;
	case LoadStatus::Code::OtherSuccess:
		debug << "Other success";
		break;
	case LoadStatus::Code::Error:
		debug << "Generic error";
		break;
	case LoadStatus::Code::InternalError:
		debug << "Internal logic error";
		break;
	case LoadStatus::Code::ReadFailure:
		debug << "Read failure";
		break;
	case LoadStatus::Code::FileAccess:
		debug << "Can't access file";
		break;
	case LoadStatus::Code::IntermediateFileAccess:
		debug << "Can't access intermediate file";
		break;
	case LoadStatus::Code::GPSBabelFailure:
		debug << "gpsbabel failure";
		break;
	case LoadStatus::Code::GPXFailure:
		debug << "GPX failure";
		break;
	case LoadStatus::Code::UnsupportedFailure:
		debug << "Failure: unsupported feature";
		break;
	case LoadStatus::Code::FailureNonFatal:
		debug << "Non-fatal failure";
		break;

	default:
		debug << "EE: Unhandled load status" << (int) load_status.code;
		break;
	}

	return debug;
}




void LoadStatus::show_error_dialog(QWidget * parent) const
{
	QString message;

	qDebug() << SG_PREFIX_I << "Will show error dialog for load status code" << *this;

	switch (this->code) {

	case LoadStatus::Code::Success:
	case LoadStatus::Code::OtherSuccess:
		qDebug() << SG_PREFIX_E << "Called the method for 'success' code";
		break;
	case LoadStatus::Code::Error:
	case LoadStatus::Code::InternalError:
	case LoadStatus::Code::ReadFailure:
	case LoadStatus::Code::FileAccess:
	case LoadStatus::Code::IntermediateFileAccess:
	case LoadStatus::Code::GPSBabelFailure:
	case LoadStatus::Code::GPXFailure:
	case LoadStatus::Code::UnsupportedFailure:
	case LoadStatus::Code::FailureNonFatal:
		message = QObject::tr("Can't load file: error");
		break;
	default:
		message = QObject::tr("Can't load file: unknown error");
		qDebug() << SG_PREFIX_E << "Unhandled load status" << (int) this->code;
		break;
	}

	if (!message.isEmpty()) {
		Dialog::error(message, parent);
	}

	return;
}




bool SlavGPS::operator==(LoadStatus::Code code, const LoadStatus & lhs)
{
	return code == lhs.code;
}




bool SlavGPS::operator!=(LoadStatus::Code code, const LoadStatus & lhs)
{
	return !(code == lhs);
}




QDebug SlavGPS::operator<<(QDebug debug, const SaveStatus & save_status)
{
	switch (save_status.code) {
	case SaveStatus::Code::Error:
		debug << "Generic error";
		break;
	case SaveStatus::Code::InternalError:
		debug << "Internal logic error";
		break;
	case SaveStatus::Code::FileAccess:
		debug << "Can't access file";
		break;
	case SaveStatus::Code::IntermediateFileAccess:
		debug << "Can't access intermediate file";
		break;
	case SaveStatus::Code::Success:
		debug << "Success";
		break;
	default:
		debug << "EE: Unhandled save status" << (int) save_status.code;
		break;
	}

	return debug;
}




void SaveStatus::show_error_dialog(QWidget * parent) const
{
	QString message;

	qDebug() << SG_PREFIX_I << "Will show error dialog for save status code" << *this;

	switch (this->code) {
	case SaveStatus::Code::Error:
		message = QObject::tr("Can't save file: error");
		break;
	case SaveStatus::Code::InternalError:
		message = QObject::tr("Can't save file: internal error");
		break;
	case SaveStatus::Code::FileAccess:
		/*
		  Examples of situation when this error can occur:
		  1. trying to save-again file in directory that has been removed.
		*/
		message = QObject::tr("Can't save file: can't open file for writing. Try to save file in different location.");
		break;
	case SaveStatus::Code::IntermediateFileAccess:
		message = QObject::tr("Can't save file: Can't access intermediate file");
		break;
	case SaveStatus::Code::Success:
		qDebug() << SG_PREFIX_E << "Called the method for 'success' code";
		break;
	default:
		message = QObject::tr("Can't save file: unknown error");
		qDebug() << SG_PREFIX_E << "Unhandled save status" << (int) this->code;
		break;
	}

	if (!message.isEmpty()) {
		Dialog::error(message, parent);
	}

	return;
}




bool SlavGPS::operator==(SaveStatus::Code code, const SaveStatus & lhs)
{
	return code == lhs.code;
}




bool SlavGPS::operator!=(SaveStatus::Code code, const SaveStatus & lhs)
{
	return !(code == lhs);
}




/**
   @reviewed on 2019-12-01
*/
SGObjectTypeID::SGObjectTypeID() {}




/**
   @reviewed on 2019-12-01
*/
SGObjectTypeID::SGObjectTypeID(const char * debug_id)
{
	if (nullptr != debug_id && '\0' != debug_id[0]) {
		snprintf(this->m_debug_string, sizeof (this->m_debug_string), "%s", debug_id);

		g_object_type_id_mutex.lock();
		/* First iterate then assign: first valid ID will be non-zero. */
		g_object_type_id_counter++;
		this->m_val = g_object_type_id_counter;
		g_object_type_id_mutex.unlock();

		qDebug() << SG_PREFIX_I << "Created object type id" << this->m_val << "for object class" << debug_id;
	} else {
		/* Client code shouldn't use empty ID. Let't not set
		   m_val, and treat it as error. */
		qDebug() << SG_PREFIX_E << "Creating empty object type id because debug id is empty";
	}
}




/**
   @reviewed on 2019-12-01
*/
QDebug SlavGPS::operator<<(QDebug debug, const SGObjectTypeID & type_id)
{
	if (0 == type_id.m_val) {
		debug << "Empty object type id";
	} else {
		debug << type_id.m_val << "/" << type_id.m_debug_string;
	}
	return debug;
}




/**
   @reviewed on 2019-12-01
*/
bool SGObjectTypeID::operator==(const SGObjectTypeID & other) const
{
	return this->m_val == other.m_val;
}




/**
   @reviewed on 2019-12-01
*/
bool SGObjectTypeID::is_empty(void) const
{
	return 0 == this->m_val;
}
