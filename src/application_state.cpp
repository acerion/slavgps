/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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


/*
  Configuration of different aspects of application.  Some settings
  are *not* intended to have any GUI controls. Other settings be can
  used to set other GUI elements.
*/




#include <QDebug>
#include <QSettings>




#include "dir.h"
#include "application_state.h"
#include "globals.h"




using namespace SlavGPS;




#define SG_MODULE "Application State"




static QSettings * settings_file = NULL;




/* ATM, can't see a point in having any more than one group for various settings. */
const QString VIKING_SETTINGS_GROUP("viking/");
#define VIKING_INI_FILE "viking.ini"




void ApplicationState::init()
{
	const QString full_path = SlavGPSLocations::get_file_full_path(VIKING_INI_FILE);
	settings_file = new QSettings(full_path, QSettings::IniFormat);
}




/**
   ATM: The only time settings are saved is on program exit.
   Could change this to occur on window exit or dialog exit or have memory hash of values...?
*/
void ApplicationState::uninit()
{
	settings_file->sync();
	delete settings_file;
}




bool ApplicationState::get_boolean(const char * name, bool * val)
{
	const QVariant value = settings_file->value(VIKING_SETTINGS_GROUP + name);
	if (value.isNull()) {
		qDebug() << SG_PREFIX_E << "Invalid boolean value read for key" << name;
		return false;
	} else {
		*val = value.toBool();
		qDebug() << SG_PREFIX_I << "Valid integer value read for key" << name << *val;
		return true;
	}
}




void ApplicationState::set_boolean(const char * name, bool val)
{
	settings_file->setValue(VIKING_SETTINGS_GROUP + name, QVariant(val));
}




bool ApplicationState::get_string(const char * name, QString & val)
{
	const QVariant value = settings_file->value(VIKING_SETTINGS_GROUP + name);
	if (value.isNull()) {
		qDebug() << SG_PREFIX_E << "Invalid string value read for key" << name;
		return false;
	} else {
		val = value.toString();
		qDebug() << SG_PREFIX_I << "Valid integer value read for key" << name << val;
		return true;
	}
}




void ApplicationState::set_string(const char * name, const QString & val)
{
	settings_file->setValue(VIKING_SETTINGS_GROUP + name, QVariant(val));
}




bool ApplicationState::get_integer(const char * name, int * val)
{
	const QVariant value = settings_file->value(VIKING_SETTINGS_GROUP + name);
	if (value.isNull()) {
		qDebug() << SG_PREFIX_E << "Invalid integer value read for key" << name;
		return false;
	} else {
		*val = value.toInt();
		qDebug() << SG_PREFIX_I << "Valid integer value read for key" << name << *val;
		return true;
	}
}




void ApplicationState::set_integer(const char * name, int val)
{
	settings_file->setValue(VIKING_SETTINGS_GROUP + name, QVariant(val));
}




bool ApplicationState::get_double(const char * name, double * val)
{
	const QVariant value = settings_file->value(VIKING_SETTINGS_GROUP + name);
	if (value.isNull()) {
		qDebug() << SG_PREFIX_E << "Invalid double value read for key" << name;
		return false;
	} else {
		*val = value.toDouble();
		qDebug() << SG_PREFIX_I << "Valid integer value read for key" << name << *val;
		return true;
	}
}




void ApplicationState::set_double(const char * name, double val)
{
	settings_file->setValue(VIKING_SETTINGS_GROUP + name, QVariant(val));
}




/*
  The returned list of integers should be freed when no longer needed.
*/
bool ApplicationState::get_integer_list(const char * name, std::vector<int> & integers)
{
	const QVariant value = settings_file->value(VIKING_SETTINGS_GROUP + name);
	if (value.isNull()) {
		qDebug() << SG_PREFIX_E << "Invalid integer list value read for key" << name;
		return false;
	} else {
		qDebug() << SG_PREFIX_I << "Getting list of integers from file for key name" << name;
		QList<QVariant> list = value.toList();
		integers.resize(list.size());
		for (int i = 0; i < list.size(); i++) {
			const int data = list.at(i).toInt();
			integers.push_back(data);
			qDebug() << SG_PREFIX_I << data;
		}
		return true;
	}
	return false;
}




void ApplicationState::set_integer_list(const char * name, std::vector<int> & integers)
{
	QList<QVariant> list;
	for (auto iter = integers.begin(); iter != integers.end(); iter++) {
		list.append(QVariant(*iter));
	}

	settings_file->setValue(VIKING_SETTINGS_GROUP + name, QVariant(list));
}




bool ApplicationState::get_integer_list_contains(const char * name, int val)
{
	std::vector<int> integers;
	bool contains = false;

	/* Get current list and see if the value supplied is in the list. */
	if (ApplicationState::get_integer_list(name, integers)) {
		auto iter = std::find(integers.begin(), integers.end(), val);
		contains = iter != integers.end();
	}

	return contains;
}




void ApplicationState::set_integer_list_containing(const char * name, int val)
{
	std::vector<int> integers;
	bool need_to_add = true;

	/* Get current list. */
	if (ApplicationState::get_integer_list(name, integers)) {
		/* See if it's not already there. */
		auto iter = std::find(integers.begin(), integers.end(), val);
		need_to_add = iter == integers.end();
	}

	/* Add value into array if necessary. */
	if (need_to_add) {
		/* Not bothering to sort this list ATM as there is not much to be gained. */
		integers.push_back(val);
		/* Save. */
		ApplicationState::set_integer_list(name, integers);
	}
}
