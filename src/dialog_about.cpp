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




#include <QFormLayout>
#include <QTextEdit>




#include "dialog.h"




using namespace SlavGPS;




#define SLAVGPS_PACKAGE_NAME "SlavGPS"
#define SLAVGPS_VERSION      "0.0.1"
#define SLAVGPS_URL          "url-todo"

#define VIKING_PACKAGE_NAME  PACKAGE_NAME




static const QString viking_author("Evan Battaglia &lt;gtoevan@gmx.net&gt;");
static const QString viking_contributors("Alex Foobarian &lt;foobarian@gmail.com&gt;<br/></br>"
					 "Bernd Zeimetz &lt;bernd@bzed.de&gt;<br/></br>"
					 "Guilhem Bonnefille &lt;guilhem.bonnefille@gmail.com&gt;<br/></br>"
					 "Jocelyn Jaubert &lt;jocelyn.jaubert@gmail.com&gt;<br/></br>"
					 "Mark Coulter &lt;i_offroad@yahoo.com&gt;<br/></br>"
					 "Mathieu Albinet &lt;mathieu17@gmail.com&gt;<br/></br>"
					 "Quy Tonthat &lt;qtonthat@gmail.com&gt;<br/></br>"
					 "Robert Norris &lt;rw_norris@hotmail.com&gt;<br/></br>"
					 "<br/></br>");
static const QString viking_documenters("Guilhem Bonnefille<br/></br>"
					"Rob Norris<br/></br>"
					"username: Alexxy<br/></br>"
					"username: Vikingis<br/></br>"
					"username: Tallguy<br/></br>"
					"username: EliotB<br/></br>"
					"Alex Foobarian<br/></br>");




void Dialog::about(QWidget * parent)
{
	BasicMessage dialog(parent);

	dialog.setMinimumSize(500, 300);
	dialog.setWindowTitle(QObject::tr("About %1").arg(SLAVGPS_PACKAGE_NAME));

	QTabWidget * tabs = new QTabWidget(parent);
	dialog.grid->addWidget(tabs, 0, 0);

	QTextEdit * text_about_this_program = new QTextEdit(&dialog);
	tabs->addTab(text_about_this_program, QObject::tr("About this program"));

	QTextEdit * text_license = new QTextEdit(&dialog);
	tabs->addTab(text_license, QObject::tr("License"));

	QTextEdit * text_libraries = new QTextEdit(&dialog);
	tabs->addTab(text_libraries, QObject::tr("Libraries"));

	QTextEdit * text_about_viking = new QTextEdit(&dialog);
	tabs->addTab(text_about_viking, QObject::tr("About Viking"));

	/* TODO: make links clickable. */

	static const QString copyright = QObject::tr("<b>Copyright:</b><br/>"
						     "2017, Kamil Ignacak<br/>"
						     "2003-2008, Evan Battaglia<br/>"
						     "2008-" THEYEAR", Viking's contributors<br/>");

	static const QString copyright_viking = QObject::tr("<b>Copyright:</b><br/>"
							    "2003-2008, Evan Battaglia<br/>"
							    "2008-" THEYEAR", Viking's contributors<br/>");

	static const QString short_description_viking = QObject::tr("GPS Data and Topo Analyzer, Explorer, and Manager.");
	static const QString short_description = QObject::tr("GPS Data and Topo Analyzer, Explorer, and Manager.");
	static const QString license = QObject::tr("This program is free software; you can redistribute it and/or modify "
						   "it under the terms of the GNU General Public License as published by "
						   "the Free Software Foundation; either version 2 of the License, or "
						   "(at your option) any later version."
						   "<br/><br/>"
						   "This program is distributed in the hope that it will be useful, "
						   "but WITHOUT ANY WARRANTY; without even the implied warranty of "
						   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
						   "GNU General Public License for more details."
						   "<br/><br/>"
						   "You should have received a copy of the GNU General Public License "
						   "along with this program; if not, write to the Free Software "
						   "Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA");

	text_about_this_program->insertHtml(QString("%1 %2<br/>%3<br/>%4<br/><br/>%5<br/><br/>").arg(SLAVGPS_PACKAGE_NAME).arg(SLAVGPS_VERSION).arg(short_description).arg(SLAVGPS_URL).arg(copyright));
	text_about_this_program->insertHtml(QObject::tr("%1 is a fork of %2 %3. The fork has been made in year 2017").arg(SLAVGPS_PACKAGE_NAME).arg(VIKING_PACKAGE_NAME).arg(VIKING_VERSION));
	text_about_this_program->setReadOnly(true);
	text_about_this_program->moveCursor(QTextCursor::Start); /* Scroll to top of page. */

	text_license->insertHtml(QString("%1<br/><br/>%2<br/><br/>%3").arg(short_description).arg(copyright).arg(license));
	text_license->setReadOnly(true);
	text_license->moveCursor(QTextCursor::Start); /* Scroll to top of page. */

	text_about_viking->insertHtml(QString("%1 %2<br/>%3<br/>%4<br/><br/>%5<br/><br/>").arg(PACKAGE_NAME).arg(VIKING_VERSION).arg(short_description_viking).arg(VIKING_URL).arg(copyright_viking));
	text_about_viking->insertHtml(QObject::tr("<b>Author:</b><br/>%1<br/><br/><br/>").arg(viking_author));
	text_about_viking->insertHtml(QObject::tr("<b>Contributors:</b><br/>%1<br/>").arg(viking_contributors));
	text_about_viking->insertHtml(QObject::tr("Few other bugfixes/minor patches from various contributors. See ChangeLog for details.<br/><br/><br/>"));
	text_about_viking->insertHtml(QObject::tr("<b>Documenters:</b><br/>%1<br/><br/>").arg(viking_documenters));
	text_about_viking->insertHtml(QObject::tr("Translation is coordinated on http://launchpad.net/viking"));
	text_about_viking->setReadOnly(true);
	text_about_viking->moveCursor(QTextCursor::Start); /* Scroll to top of page. */

	QString libs;
	libs += QObject::tr("<b>Compiled in libraries:</b><br/>");

	/* Default libs. */
	libs += "libglib-2.0<br/>";
	libs += "libgthread-2.0<br/>";
	libs += "libgtk+-2.0<br/>";
	libs += "libgio-2.0<br/>";

	/* Potentially optional libs (but probably couldn't build without them). */
#ifdef HAVE_LIBM
	libs += "libm<br/>";
#endif
#ifdef HAVE_LIBZ
	libs += "libz<br/>";
#endif
#ifdef HAVE_LIBCURL
	libs += "libcurl<br/>";
#endif
#ifdef HAVE_EXPAT_H
	libs += "libexpat<br/>";
#endif
		/* Actually optional libs. */
#ifdef HAVE_LIBGPS
	libs += "libgps<br/>";
#endif
#ifdef HAVE_LIBGEXIV2
	libs += "libgexiv2<br/>";
#endif
#ifdef HAVE_LIBEXIF
	libs += "libexif<br/>";
#endif
#ifdef HAVE_LIBX11
	libs += "libX11<br/>";
#endif
#ifdef HAVE_LIBMAGIC
	libs += "libmagic<br/>";
#endif
#ifdef HAVE_LIBBZ2
	libs += "libbz2<br/>";
#endif
#ifdef HAVE_LIBZIP
	libs += "libzip<br/>";
#endif
#ifdef HAVE_LIBSQLITE3
	libs += "libsqlite3<br/>";
#endif
#ifdef HAVE_LIBMAPNIK
	libs += "libmapnik<br/>";
#endif

	text_libraries->insertHtml(libs);
	text_libraries->setReadOnly(true);
	text_libraries->moveCursor(QTextCursor::Start); /* Scroll to top of page. */

	dialog.exec();
	return;
}
