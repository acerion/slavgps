/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2008, Hein Ragas <viking@ragas.nl>
 * Copyright (C) 2010-2014, Rob Norris <rw_norris@hotmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

#include <QDialog>
#include <QInputDialog>
#include <QObject>
#include <QHeaderView>
#include <QDebug>

#include "dialog.h"
#include "viewport.h"
#include "ui_util.h"
#include "date_time_dialog.h"
#include "degrees_converters.h"
#include "authors.h"
#include "documenters.h"
#include "globals.h"




using namespace SlavGPS;




void Dialog::info(QString const & message, QWidget * parent)
{
	QMessageBox box(parent);
	box.setText(message);
	box.setIcon(QMessageBox::Information);
	box.exec();
	return;
}




void Dialog::warning(QString const & message, QWidget * parent)
{
	QMessageBox box(parent);
	box.setText(message);
	box.setIcon(QMessageBox::Warning);
	box.exec();
	return;
}




void Dialog::error(QString const & message, QWidget * parent)
{
	QMessageBox box(parent);
	box.setText(message);
	box.setIcon(QMessageBox::Critical);
	box.exec();
	return;
}




/**
 * Returns: a date as a string - always in ISO8601 format (YYYY-MM-DD).
 * This string can be NULL (especially when the dialog is cancelled).
 * Free the string after use.
 */
char * a_dialog_get_date(const QString & title, QWidget * parent)
{
	time_t mytime = date_dialog(parent, title, time(NULL));

	if (!mytime) {
		return NULL;
	}

	struct tm * out = localtime(&mytime);
	size_t size = strlen("YYYY-MM-DD") + 1;
	char * date_str = (char *) malloc(size);
	snprintf(date_str, size, "%04d-%02d-%02d", 1900 + out->tm_year, out->tm_mon + 1, out->tm_mday);
	qDebug() << "II: Dialog: get date:" << date_str;

	return date_str;
}




bool Dialog::yes_or_no(QString const & message, QWidget * parent, QString const & title)
{
	return QMessageBox::Yes == QMessageBox::question(parent, title, message);
}




/**
 * Dialog to return a positive number via a spinbox within the supplied limits.
 *
 * Returns: A value of zero indicates the dialog was cancelled.
 */
int a_dialog_get_positive_number(const QString & title, const QString & label, int default_num, int min, int max, int step, QWidget * parent)
{
	int result = QInputDialog::getInt(parent, title, label, default_num, min, max, step);

	return result;
}




#ifdef K
static void about_url_hook(GtkAboutDialog * about, const char * link, void * data)
{
	open_url(GTK_WINDOW(about), link);
}




static void about_email_hook(GtkAboutDialog *about, const char * email,  void * data)
{
	new_email(GTK_WINDOW(about), email);
}
#endif




/**
   Creates a dialog with list of text.
   Mostly useful for longer messages that have several lines of information.
*/
void a_dialog_list(const QString & title, const QStringList & items, int padding, QWidget * parent)
{
	QMessageBox box(parent);
	//box.setTitle(title);
	box.setIcon(QMessageBox::Information);

	QVBoxLayout vbox;
	QLayout * old = box.layout();
	delete old;
	box.setLayout(&vbox);

	for (int i = 0; i < items.size(); i++) {
		QLabel * label = ui_label_new_selectable(items.at(i), &box);
		vbox.addWidget(label);
		// gtk_box_pack_start(GTK_BOX(vbox), label, false, true, padding);
	}

	box.exec();
}




void Dialog::about(QWidget * parent)
{
#ifdef K
	const char *program_name = PACKAGE_NAME;
	const char *version = VIKING_VERSION;
	const char *website = VIKING_URL;
	const char *copyright = "2003-2008, Evan Battaglia\n2008-" THEYEAR", Viking's contributors";
	const char *comments = _("GPS Data and Topo Analyzer, Explorer, and Manager.");
	const char *license = _("This program is free software; you can redistribute it and/or modify "
				"it under the terms of the GNU General Public License as published by "
				"the Free Software Foundation; either version 2 of the License, or "
				"(at your option) any later version."
				"\n\n"
				"This program is distributed in the hope that it will be useful, "
				"but WITHOUT ANY WARRANTY; without even the implied warranty of "
				"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
				"GNU General Public License for more details."
				"\n\n"
				"You should have received a copy of the GNU General Public License "
				"along with this program; if not, write to the Free Software "
				"Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA");

	/* Would be nice to use gtk_about_dialog_add_credit_section (), but that requires gtk 3.4. */
	/* For now shove it in the 'artists' section so at least the information is easily visible. */
	/* Something more advanced might have proper version information too... */
	const char *libs[] = {
		"Compiled in libraries:",
		/* Default libs. */
		"libglib-2.0",
		"libgthread-2.0",
		"libgtk+-2.0",
		"libgio-2.0",
		/* Potentially optional libs (but probably couldn't build without them). */
#ifdef HAVE_LIBM
		"libm",
#endif
#ifdef HAVE_LIBZ
		"libz",
#endif
#ifdef HAVE_LIBCURL
		"libcurl",
#endif
#ifdef HAVE_EXPAT_H
		"libexpat",
#endif
		/* Actually optional libs. */
#ifdef HAVE_LIBGPS
		"libgps",
#endif
#ifdef HAVE_LIBGEXIV2
		"libgexiv2",
#endif
#ifdef HAVE_LIBEXIF
		"libexif",
#endif
#ifdef HAVE_LIBX11
		"libX11",
#endif
#ifdef HAVE_LIBMAGIC
		"libmagic",
#endif
#ifdef HAVE_LIBBZ2
		"libbz2",
#endif
#ifdef HAVE_LIBZIP
		"libzip",
#endif
#ifdef HAVE_LIBSQLITE3
		"libsqlite3",
#endif
#ifdef HAVE_LIBMAPNIK
		"libmapnik",
#endif
		NULL
	};
	/* Newer versions of GTK 'just work', calling gtk_show_uri() on the URL or email and opens up the appropriate program.
	   This is the old method: */
	gtk_about_dialog_set_url_hook(about_url_hook, NULL, NULL);
	gtk_about_dialog_set_email_hook(about_email_hook, NULL, NULL);

	gtk_show_about_dialog(parent,
			      /* TODO do not set program-name and correctly set info for g_get_application_name. */
			      "program-name", program_name,
			      "version", version,
			      "website", website,
			      "comments", comments,
			      "copyright", copyright,
			      "license", license,
			      "wrap-license", true,
			      /* Logo automatically retrieved via gtk_window_get_default_icon_list. */
			      "authors", AUTHORS,
			      "documenters", DOCUMENTERS,
			      "translator-credits", _("Translation is coordinated on http://launchpad.net/viking"),
			      "artists", libs,
			      NULL);
#endif
}




/**
 * Display a dialog presenting the license of a map.
 * Allow to read the license by launching a web browser.
 */
void Dialog::license(const char * map, const char * license, const char * url, QWidget * parent)
{
	const QString primary_text = QString(QObject::tr("The map data is licensed: %1.")).arg(license);
	const QString secondary_text = QString(QObject::tr("The data provided by '<b>%1</b>' are licensed under the following license: <b>%1</b>."))
		.arg(map)
		.arg(license);

	QMessageBox box;
	box.setText(primary_text);
	box.setInformativeText(secondary_text);
	box.setStandardButtons(QMessageBox::Ok);

	if (url != NULL) {
		box.addButton(QObject::tr("Open license"), QMessageBox::HelpRole);
	}
	int response;
	do {
		response = box.exec();
		if (response == QMessageBox::Help) {
#ifdef K
			open_url(url, parent);
#endif
		}
	} while (response != QMessageBox::Cancel && response != QMessageBox::Ok);
}
