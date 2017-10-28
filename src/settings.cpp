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
#include "settings.h"




using namespace SlavGPS;




static QSettings * settings_file = NULL;




/* ATM, can't see a point in having any more than one group for various settings. */
const QString VIKING_SETTINGS_GROUP("viking/");
#define VIKING_INI_FILE "viking.ini"




void ApplicationState::init()
{
	const QString full_path = get_viking_dir() + QDir::separator() + VIKING_INI_FILE;
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
		qDebug() << "EE: ApplicationState: invalid boolean value read for key" << name;
		return false;
	} else {
		*val = value.toBool();
		qDebug() << "II: ApplicationState: valid integer value read for key" << name << *val;
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
		qDebug() << "EE: ApplicationState: invalid string value read for key" << name;
		return false;
	} else {
		val = value.toString();
		qDebug() << "II: ApplicationState: valid integer value read for key" << name << val;
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
		qDebug() << "EE: ApplicationState: invalid integer value read for key" << name;
		return false;
	} else {
		*val = value.toInt();
		qDebug() << "II: ApplicationState: valid integer value read for key" << name << *val;
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
		qDebug() << "EE: ApplicationState: invalid double value read for key" << name;
		return false;
	} else {
		*val = value.toDouble();
		qDebug() << "II: ApplicationState: valid integer value read for key" << name << *val;
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
bool ApplicationState::get_integer_list(const char * name, int ** vals, size_t * length)
{
#ifdef K
	const QVariant value = settings_file->value(VIKING_SETTINGS_GROUP + name);
	if (value.isNull()) {
		qDebug() << "EE: ApplicationState: invalid integer list value read for key" << name;
		return false;
	} else {
		*vals = value.to();
		qDebug() << "II: ApplicationState: valid integer value read for key" << name << *val;
		return true;
	}
#else
	return false;
#endif
}




void ApplicationState::set_integer_list(const char * name, int vals[], size_t length)
{
#ifdef K
	settings_file->setValue(VIKING_SETTINGS_GROUP + name, QVariant(val));
#endif
}




bool ApplicationState::get_integer_list_contains(const char * name, int val)
{
	int * vals = NULL;
	size_t length;
	/* Get current list and see if the value supplied is in the list. */
	bool contains = false;

	/* Get current list. */
	if (ApplicationState::get_integer_list(name, &vals, &length)) {
		/* See if it's not already there. */
		size_t ii = 0;
		if (vals && length) {
			while (ii < length) {
				if (vals[ii] == val) {
					contains = true;
					break;
				}
				ii++;
			}
			/* Free old array. */
			free(vals);
		}
	}

	return contains;
}




void ApplicationState::set_integer_list_containing(const char * name, int val)
{
	int * vals = NULL;
	size_t length = 0;
	bool need_to_add = true;

	/* Get current list. */
	if (ApplicationState::get_integer_list(name, &vals, &length)) {
		/* See if it's not already there. */
		if (vals) {
			size_t ii = 0;
			while (ii < length) {
				if (vals[ii] == val) {
					need_to_add = false;
					break;
				}
				ii++;
			}
		}
	}
	/* Add value into array if necessary. */
	if (vals && need_to_add) {
		/* NB not bothering to sort this 'list' ATM as there is not much to be gained. */
		unsigned int new_length = length + 1;
		int new_vals[new_length];
		/* Copy array. */
		for (unsigned int ii = 0; ii < length; ii++) {
			new_vals[ii] = vals[ii];
		}
		new_vals[length] = val; /* Set the new value. */
		/* Apply. */
		ApplicationState::set_integer_list(name, new_vals, new_length);
		/* Free old array. */
		free(vals);
	}
}
