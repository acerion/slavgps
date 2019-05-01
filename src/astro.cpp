/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005-2008, Alex Foobarian <foobarian@gmail.com>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2009, Hein Ragas <viking@ragas.nl>
 * Copyright (c) 2012-2015, Rob Norris <rw_norris@hotmail.com>
 * Copyright (c) 2012-2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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




#include <cmath>
#include <cstdio>




#include <glib.h>




#include <QStandardPaths>




#include "astro.h"
#include "application_state.h"
#include "util.h"
#include "dialog.h"




using namespace SlavGPS;




#define SG_MODULE "Astro"
#define VIK_SETTINGS_EXTERNAL_ASTRO_PROGRAM "external_astro_program"




bool g_have_astro_program = false;
static QString g_astro_program;




/*
  Format of stellarium lat & lon seems designed to be particularly awkward
  who uses ' & " in the parameters for the command line?!
  -1d4'27.48"
  +53d58'16.65"
*/
QString Astro::convert_to_dms(double dec)
{
	char sign_c = ' ';
	if (dec > 0) {
		sign_c = '+';
	} else if (dec < 0) {
		sign_c = '-';
	} else { /* Nul value. */
		sign_c = ' ';
	}

	/* Degrees. */
	double tmp = fabs(dec);
	const int val_d = (int)tmp;

	/* Minutes. */
	tmp = (tmp - val_d) * 60;
	const int val_m = (int) tmp;

	/* Seconds. */
	const double val_s = (tmp - val_m) * 60;

	/* Format. */
	char buffer[sizeof ("+111d58'16.6543\"") + 10] = { 0 };
	snprintf(buffer, sizeof (buffer), "%c%dd%d\\\'%.4f\\\"", sign_c, val_d, val_m, val_s);
	return QString(buffer);
}




void Astro::init(void)
{
	/* Astronomy Domain. */
	if (!ApplicationState::get_string(VIK_SETTINGS_EXTERNAL_ASTRO_PROGRAM, g_astro_program)) {
#ifdef WINDOWS
		//g_astro_program = "C:\\Program Files\\Stellarium\\stellarium.exe";
		g_astro_program = "C:/Progra~1/Stellarium/stellarium.exe";
#else
		g_astro_program = "stellarium";
#endif
	} else {
		/* User specified so assume it works. */
		g_have_astro_program = true;
	}
	if (!QStandardPaths::findExecutable(g_astro_program).isEmpty()) {
		g_have_astro_program = true;
	}
}





/**
 * Open a program at the specified date
 * Mainly for Stellarium - http://stellarium.org/
 * But could work with any program that accepts the same command line options...
 * FUTURE: Allow configuring of command line options + format or parameters
 */
sg_ret Astro::open(const QString & date_str, const QString & time_str, const QString & lat_str, const QString & lon_str, const QString & alt_str, QWidget * parent)
{
	GError *err = NULL;
	char * ini_file_path;
	int fd = g_file_open_tmp("vik-astro-XXXXXX.ini", &ini_file_path, &err);
	if (fd < 0) {
		fprintf(stderr, "WARNING: %s: Failed to open temporary file: %s\n", __FUNCTION__, err->message);
		g_clear_error(&err);
		return sg_ret::err;
	}
	const QString cmd = QString("%1 -c %2 --full-screen no --sky-date %3 --sky-time %4 --latitude %5 --longitude %6 --altitude %7")
		.arg(g_astro_program)
		.arg(ini_file_path)
		.arg(date_str)
		.arg(time_str)
		.arg(lat_str)
		.arg(lon_str)
		.arg(alt_str);

	qDebug() << SG_PREFIX_I << "Command is " << cmd;

	if (!g_spawn_command_line_async(cmd.toUtf8().constData(), &err)) {
		Dialog::error(QObject::tr("Could not launch %1").arg(g_astro_program), parent);
		fprintf(stderr, "WARNING: %s\n", err->message);
		g_error_free(err);
	}
	Util::add_to_deletion_list(QString(ini_file_path));
	free(ini_file_path);

	return sg_ret::ok;
}
