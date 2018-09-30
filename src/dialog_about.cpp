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




#include <QFormLayout>
#include <QTextBrowser>




#include "dialog.h"




using namespace SlavGPS;




/* Needed to indicate parent project. */
#define VIKING_PACKAGE  "Viking"
#define VIKING_VERSION  "1.6.1"
#define VIKING_URL      "http://viking.sf.net/"




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
	dialog.setWindowTitle(QObject::tr("About %1").arg(PROJECT));

	QTabWidget * tabs = new QTabWidget(parent);
	dialog.grid->addWidget(tabs, 0, 0);

	QTextBrowser * text_about_this_program = new QTextBrowser(&dialog);
	tabs->addTab(text_about_this_program, QObject::tr("About this program"));

	QTextBrowser * text_license = new QTextBrowser(&dialog);
	tabs->addTab(text_license, QObject::tr("License"));

	QTextBrowser * text_libraries = new QTextBrowser(&dialog);
	tabs->addTab(text_libraries, QObject::tr("Libraries"));

	QTextBrowser * text_about_viking = new QTextBrowser(&dialog);
	tabs->addTab(text_about_viking, QObject::tr("About Viking"));

	static const QString copyright = QObject::tr("<b>Copyright:</b><br/>"
						     "2016-" CURRENT_YEAR", Kamil Ignacak<br/>"
						     "2003-2008, Evan Battaglia<br/>"
						     "2008-2016, Viking's contributors<br/><br/><br/>"); /* 2016: year of forking of SlavGPS. */

	static const QString copyright_viking = QObject::tr("<b>Copyright:</b><br/>"
							    "2003-2008, Evan Battaglia<br/>"
							    "2008-2016, Viking's contributors<br/><br/><br/>"); /* 2016: year of forking of SlavGPS. */

	static const QString short_description_viking = QObject::tr("GPS Data and Topo Analyzer, Explorer, and Manager.<br/><br/><br/>");
	static const QString short_description = QObject::tr("GPS Data and Topo Analyzer, Explorer, and Manager.<br/><br/><br/>");
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

	text_about_this_program->insertPlainText(QString("%1 %2\n").arg(PROJECT).arg(PACKAGE_VERSION));
	text_about_this_program->insertHtml(short_description);
	text_about_this_program->insertHtml(QString("<a href=\"%1\">%2</a><br/><br/><br/>").arg(PACKAGE_URL).arg(PACKAGE_URL));
	text_about_this_program->insertHtml(copyright);
	text_about_this_program->insertPlainText(QObject::tr("%1 is a fork of %2 %3. The fork has been made in April 2016.").arg(PROJECT).arg(VIKING_PACKAGE).arg(VIKING_VERSION));
	text_about_this_program->setOpenExternalLinks(true); /* Open with system's default browser. */
	text_about_this_program->setReadOnly(true);
	text_about_this_program->moveCursor(QTextCursor::Start); /* Scroll to top of page. */

	text_license->insertHtml(QString("%1%2%3").arg(short_description).arg(copyright).arg(license));
	text_license->setReadOnly(true);
	text_license->moveCursor(QTextCursor::Start); /* Scroll to top of page. */

	text_about_viking->insertPlainText(QString("%1 %2\n").arg(VIKING_PACKAGE).arg(VIKING_VERSION));
	text_about_viking->insertHtml(short_description_viking);
	text_about_viking->insertHtml(QString("<a href=\"%1\">%2</a><br/><br/></br/>").arg(VIKING_URL).arg(VIKING_URL));
	text_about_viking->insertHtml(copyright_viking);
	text_about_viking->insertHtml(QObject::tr("<b>Author:</b><br/>%1<br/><br/><br/>").arg(viking_author));
	text_about_viking->insertHtml(QObject::tr("<b>Contributors:</b><br/>%1<br/>").arg(viking_contributors));
	text_about_viking->insertHtml(QObject::tr("Few other bugfixes/minor patches from various contributors. See ChangeLog for details.<br/><br/><br/>"));
	text_about_viking->insertHtml(QObject::tr("<b>Documenters:</b><br/>%1<br/><br/>").arg(viking_documenters));
	text_about_viking->insertHtml(QObject::tr("Translation is coordinated on <a href=\"http://launchpad.net/viking\">http://launchpad.net/viking</a>"));
	text_about_viking->setOpenExternalLinks(true); /* Open with system's default browser. */
	//text_about_viking->setReadOnly(true);
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
#ifdef HAVE_X11_XLIB_H
	libs += "libX11<br/>";
#endif
#ifdef HAVE_MAGIC_H
	libs += "libmagic<br/>";
#endif
#ifdef HAVE_BZLIB_H
	libs += "libbz2<br/>";
#endif
#ifdef HAVE_LIBZIP
	libs += "libzip<br/>";
#endif
#ifdef HAVE_SQLITE3_H
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
