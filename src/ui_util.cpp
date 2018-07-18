/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdlib>

#include <QDesktopServices>
#include <QUrl>
#include <QDebug>
#include <QPainter>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>

#include "util.h"
#include "ui_util.h"
#include "dialog.h"

#ifdef WINDOWS
#include <windows.h>
#endif




using namespace SlavGPS;




/*
#ifndef WINDOWS
static bool spawn_command_line_async(const char * cmd,
				     const char * arg)
{
	char * cmdline = NULL;
	bool status;

	cmdline = QString("%1 '%2'").arg(cmd).arg(arg);
	qDebug() << "DD" PREFIX << "Running command" << cmdline;

	status = g_spawn_command_line_async(cmdline.toUtf8().constData(), NULL);

	free(cmdline);

	return status;
}
#endif
*/




void SlavGPS::open_url(const QString & url)
{
	qDebug() << "II: Open URL" << url;
	QDesktopServices::openUrl(QUrl(url));
}




void SlavGPS::new_email(const QString & address, Window * parent)
{
#ifdef K_FIXME_RESTORE
	const QString uri = QString("mailto:%1").arg(address);
	GError *error = NULL;
	gtk_show_uri(gtk_widget_get_screen(GTK_WIDGET(parent)), uri.toUtf8().constData(), GDK_CURRENT_TIME, &error);
	if (error) {
		Dialog::error(tr("Could not create new email. %1").arg(QString(error->message)), parent);
		g_error_free(error);
	}
	/*
	  #ifdef WINDOWS
	  ShellExecute(NULL, NULL, (char *) uri, NULL, ".\\", 0);
	  #else
	  if (!spawn_command_line_async("xdg-email", uri))
	  Dialog::error(tr("Could not create new email."), parent);
	  #endif
	*/
#endif
}




/**
 * Returns a label widget that is made selectable (i.e. the user can copy the text).
 * @param text String to display - maybe NULL
 * @return The label widget
 */
QLabel * SlavGPS::ui_label_new_selectable(QString const & text, QWidget * parent)
{
	QLabel * label = new QLabel(text, parent);
	label->setTextInteractionFlags(Qt::TextSelectableByMouse);
	return label;
}




void SlavGPS::ui_pixmap_set_alpha(QPixmap & pixmap, int alpha)
{
	QImage image(pixmap.size(), QImage::Format_ARGB32_Premultiplied);
	QPainter painter(&image);
	painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
	painter.setOpacity(alpha / 255.0);
	painter.drawPixmap(0, 0, pixmap);
	pixmap = QPixmap::fromImage(image);
}




/**
 * Reduce the alpha value of the specified pixbuf by alpha / 255.
 */
void SlavGPS::ui_pixmap_scale_alpha(QPixmap & pixmap, int alpha)
{
#ifdef K_FIXME_RESTORE
	unsigned char *pixels;
	int iii, jjj;

	if (!pixmap->hasAlphaChannel()) {
		QPixmap * tmp = gdk_pixbuf_add_alpha(pixmap,false,0,0,0);
		g_object_unref(G_OBJECT(pixmap));
		pixmap = tmp;
		if (!pixmap) {
			return NULL;
		}
	}

	pixels = gdk_pixbuf_get_pixels(pixmap);
	int width = pixmap->width();
	int height = pixmap->height();

	/* r,g,b,a,r,g,b,a.... */
	for (iii = 0; iii < width; iii++) {
		for (jjj = 0; jjj < height; jjj++) {
			pixels += 3;
			if (*pixels != 0) {
				*pixels = (uint8_t)(((uint16_t)*pixels * alpha) / 255);
			}
			pixels++;
		}
	}
#endif
	return;
}




/* Update desktop manager's list of recently used documents. */
void SlavGPS::update_desktop_recent_documents(Window * window, const QString & file_full_path, const QString & mime_type)
{
#ifdef K_FIXME_RESTORE
	/* Update Recently Used Document framework */
	GtkRecentManager *manager = gtk_recent_manager_get_default();
	GtkRecentData * recent_data = g_slice_new(GtkRecentData);
	char *groups[] = { (char *) "viking", NULL};
	GFile * file = g_file_new_for_commandline_arg(file_full_path);
	char * uri = g_file_get_uri(file);
	char * basename = g_path_get_basename(file_full_path);
	g_object_unref(file);
	file = NULL;

	recent_data->display_name   = basename;
	recent_data->description    = NULL;
	recent_data->mime_type      = (char *) mime_type;
	recent_data->app_name       = (char *) g_get_application_name();
	recent_data->app_exec       = g_strjoin(" ", g_get_prgname(), "%f", NULL);
	recent_data->groups         = groups;
	recent_data->is_private     = false;
	if (!gtk_recent_manager_add_full(manager, uri, recent_data)) {
		this->get_statusbar()->set_message(StatusBarField::INFO, QString("Unable to add '%s' to the list of recently used documents").arg(uri));
	}

	free(uri);
	free(basename);
	free(recent_data->app_exec);
	g_slice_free(GtkRecentData, recent_data);
#endif
}
