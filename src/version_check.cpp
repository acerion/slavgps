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




#include <QDebug>
#include <QDateTime>




#include "version_check.h"
#include "window.h"
#include "dialog.h"
#include "vikutils.h"
#include "application_state.h"
#include "preferences.h"




using namespace SlavGPS;




#define SG_MODULE "Version Check"

#define VIK_SETTINGS_VERSION_CHECKED_DATE "version_checked_date"
#define VIK_SETTINGS_VERSION_CHECK_PERIOD "version_check_period_days"




VersionCheck::VersionCheck(Window * main_window)
{
	qDebug() << "DD: VersionCheck object is being created";
	this->window = main_window;

}




VersionCheck::~VersionCheck()
{
	qDebug() << "DD: VersionCheck object is being automatically removed";
}




void VersionCheck::new_version_available_dialog(const QString & new_version)
{
	/* TODO_MAYBE: it would be nice if we could run this in idle time. */

	/* Only a simple goto website option is offered.
	   Trying to do an installation update is platform specific. */
	if (Dialog::yes_or_no(QObject::tr("There is a newer version of Viking available: %1\n\nDo you wish to go to Viking's website now?").arg(new_version), this->window)) {

		/* 'VIKING_URL' redirects to the Wiki, here we want to go the main site. */
		open_url("http://sourceforge.net/projects/viking/"); /* TODO_LATER: provide correct URL for SlavGPS. */
	}

	return;
}




/* Re-implementation of QRunnable::run() */
void VersionCheck::run()
{
	/* Need to allow a few redirects, as SF file is often served from different server. */
	DownloadOptions dl_options(5);
	DownloadHandle dl_handle(&dl_options);

	QTemporaryFile tmp_file;
	// const char *file_full_path = strdup("VERSION");
	if (!dl_handle.download_to_tmp_file(tmp_file, "http://sourceforge.net/projects/viking/files/VERSION")) { /* TODO_2_LATER: provide correct URL for SlavGPS. */
		return;
	}

	char latest_version_buffer[32 + 1] = { 0 };
	off_t file_size = tmp_file.size();
	if (file_size > (off_t) sizeof (latest_version_buffer) - 1) {
		/* TODO_LATER: report very large files - this should never happen and may be a sign of problems. */
		file_size = (off_t) sizeof (latest_version_buffer) - 1;
	}
	unsigned char * file_contents = tmp_file.map(0, file_size, QFileDevice::MapPrivateOption);
	for (size_t i = 0; i < sizeof (latest_version_buffer) - 1; i++) {
		latest_version_buffer[i] = (char) file_contents[i];
	}

	int latest_version = viking_version_to_number(latest_version_buffer);
	int my_version = viking_version_to_number((char *) PACKAGE_VERSION);

	qDebug() << SG_PREFIX_I << "This version =" << PACKAGE_VERSION << ", most recent version = " << latest_version_buffer;

	if (my_version < latest_version) {
		this->new_version_available_dialog(latest_version_buffer);
	} else {
		qDebug() << SG_PREFIX_I << "Running the lastest version:" << PACKAGE_VERSION;
	}

	tmp_file.unmap(file_contents);
	tmp_file.close();

	/* Update last checked time. */
	const QDateTime date_time_now = QDateTime::currentDateTime();
	ApplicationState::set_string(VIK_SETTINGS_VERSION_CHECKED_DATE, date_time_now.toString(Qt::ISODate));
}





/**
   @window: Somewhere where we may need use the display to inform the user about the version status.

   Periodically checks the released latest VERSION file on the website to compare with the running version.
*/
void VersionCheck::run_check(Window * main_window)
{
	if (!Preferences::get_check_version()) {
		return;
	}

	bool do_check = false;

	int check_period;
	if (!ApplicationState::get_integer(VIK_SETTINGS_VERSION_CHECK_PERIOD, &check_period)) {
		check_period = 14;
	}

	/* Get date of last checking of version. */
	QDateTime date_last;
	QString date_last_string;
	if (ApplicationState::get_string(VIK_SETTINGS_VERSION_CHECKED_DATE, date_last_string)) {
		date_last = QDateTime::fromString(date_last_string, Qt::ISODate);
		if (!date_last.isValid()) {
			/* Previous check date is invalid, so force performing check of version. */
			do_check = true;
		}
	} else {
		/* Previous check date is unavailable, so force performing check of version. */
		do_check = true;
	}


	if (!do_check) {
		/* Until now the check was not forced by unavailability or invalidity data.
		   See if it will be forced by the fact that check_period elapsed. */
		QDateTime date_now = QDateTime::currentDateTime();
		date_last.addDays(check_period);
		if (date_last.date() < date_now.date()) {
			do_check = true;
		}
	}

	if (do_check) {
		VersionCheck * version_check = new VersionCheck(main_window);
		/* Thread pool takes ownership of version_check and will delete it automatically. */
		QThreadPool::globalInstance()->start(version_check);
	}
}
