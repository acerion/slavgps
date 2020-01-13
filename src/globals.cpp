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
	debug << load_status.get_message_string();
	return debug;
}




QString LoadStatus::get_message_string(void) const
{
	QString message;

	qDebug() << SG_PREFIX_I << "Will get message string for load status code" << (int) this->code;;

	switch (this->code) {
	case LoadStatus::Code::Success:
		message = QObject::tr("Successfully loaded new data.");
		break;
	case LoadStatus::Code::GenericError:
		message = QObject::tr("Generic error during processing of file.");
		break;
	case LoadStatus::Code::InternalLogicError:
		message = QObject::tr("Internal logic error.");
		break;
	case LoadStatus::Code::ParseError:
		message = QObject::tr("Can't parse file.");
		break;
	case LoadStatus::Code::ParseWarning:
		message = QObject::tr("Encountered non-critical problems during parsing.");
		break;
	case LoadStatus::Code::CantOpenFileError:
		message = QObject::tr("Can't open file.");
		break;
	case LoadStatus::Code::CantOpenIntermediateFileError:
		message = QObject::tr("Can't open intermediate file.");
		break;
	case LoadStatus::Code::GPSBabelError:
		message = QObject::tr("Error related to gpsbabel.");
		break;
	case LoadStatus::Code::GPXError:
		message = QObject::tr("Error related to parsing of GPX.");
		break;
	case LoadStatus::Code::UnsupportedFileTypeError:
		message = QObject::tr("Unsupported file type.");
		break;
	default:
		message = QObject::tr("Can't load file: unknown error.");
		qDebug() << SG_PREFIX_E << "Unhandled load status" << (int) this->code;
		break;
	}

	if (this->parser_line != -1) {
		message += "\n";
		message += QObject::tr("Parsing error in line %1.").arg(this->parser_line);
	}

	if ("" != this->parser_message) {
		message += "\n";
		message += QObject::tr("Parser message: '%1'").arg(this->parser_message);
	}

	return message;
}




void LoadStatus::show_status_dialog(QWidget * parent) const
{
	const QString message = this->get_message_string();

	if (LoadStatus::Code::Success == this->code) {
		Dialog::info(message, parent);
	} else if (LoadStatus::Code::ParseWarning == this->code) {
		Dialog::warning(message, parent);
	} else {
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
	debug << save_status.get_message_string();
	return debug;
}




QString SaveStatus::get_message_string(void) const
{
	QString message;

	qDebug() << SG_PREFIX_I << "Will get message string for save status code" << (int) this->code;

	switch (this->code) {
	case SaveStatus::Code::Success:
		message = QObject::tr("Successfully saved the data.");
		break;
	case SaveStatus::Code::GenericError:
		message = QObject::tr("Can't save file: generic error.");
		break;
	case SaveStatus::Code::InternalLogicError:
		message = QObject::tr("Can't save file: internal error.");
		break;
	case SaveStatus::Code::CantOpenFileError:
		/*
		  Examples of situation when this error can occur:
		  1. trying to save-again file in directory that has been removed.
		*/
		message = QObject::tr("Can't save file: can't open file for writing. Try to save file in different location.");
		break;
	case SaveStatus::Code::CantOpenIntermediateFileError:
		message = QObject::tr("Can't save file: can't access intermediate file.");
		break;
	case SaveStatus::Code::GPSBabelError:
		message = QObject::tr("Error related to gpsbabel.");
		break;
	default:
		message = QObject::tr("Can't save file: unknown error.");
		qDebug() << SG_PREFIX_E << "Unhandled save status" << (int) this->code;
		break;
	}

	return message;
}




void SaveStatus::show_status_dialog(QWidget * parent) const
{
	const QString message = this->get_message_string();
	if (this->code == SaveStatus::Code::Success) {
		Dialog::info(message, parent);
	} else {
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
