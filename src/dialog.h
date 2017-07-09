/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_DIALOG_H_
#define _SG_DIALOG_H_




#include <cstdint>

#include <QObject>
#include <QString>
#include <QMessageBox>
#include <QTableView>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QCheckBox>

#include "globals.h"
#include "coords.h"
#include "window.h"
#include "ui_util.h"
#include "widget_radio_group.h"





namespace SlavGPS {




	class Dialog {
	public:
		static void info(QString const & message, QWidget * parent);
		static void warning(QString const & message, QWidget * parent);
		static void error(QString const & message, QWidget * parent);
		static bool yes_or_no(QString const & message, QWidget * parent = NULL, QString const & title = SG_APPLICATION_NAME);
		static void about(QWidget * parent);
		static void license(const char * map, const char * license, const char * url, QWidget * parent);
	};




} /* namespace SlavGPS */





void a_dialog_list(const QString & title, const QStringList & items, int padding, QWidget * parent = NULL);

/* Okay, everthing below here is an architechtural flaw. */

char * a_dialog_get_date(const QString & title, QWidget * parent = NULL);
bool a_dialog_custom_zoom(double * xmpp, double * ympp, QWidget * parent);


int a_dialog_get_positive_number(const QString & title, const QString & label, int default_num, int min, int max, int step, QWidget * parent);




#endif /* #ifndef _SG_DIALOG_H_ */
