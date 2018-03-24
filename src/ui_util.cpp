/*
 *    Viking - GPS data editor
 *    Copyright (C) 2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *    Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, version 2.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */
 /*
  * Ideally dependencies should just be on Gtk,
  * see vikutils for things that further depend on other Viking types
  * see utils for things only depend on Glib
  */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdlib>

#include <QDesktopServices>
#include <QUrl>
#include <QDebug>
#include <QPainter>

#include <glib/gstdio.h>
#include <glib/gprintf.h>

#include "util.h"
#include "ui_util.h"
#include "dialog.h"
#include "layer_trw_track_internal.h"
#include "layer_trw_waypoint.h"

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
#ifdef K_TODO
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
   Reads an integer from the GTK default settings registry
   (see http://library.gnome.org/devel/gtk/stable/GtkSettings.html).
   @param property_name The property to read.
   @param default_value The default value in case the value could not be read.
   @return The value for the property if it exists, otherwise the @a default_value.
*/
int SlavGPS::ui_get_gtk_settings_integer(const char * property_name, int default_value)
{
#ifdef K_TODO
	if (g_object_class_find_property(G_OBJECT_GET_CLASS(G_OBJECT(
		gtk_settings_get_default())), property_name)) {

		int value;
		g_object_get(G_OBJECT(gtk_settings_get_default()), property_name, &value, NULL);
		return value;
	} else {
#endif
		return default_value;
#ifdef K_TODO
	}
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




/**
 * Apply the alpha value to the specified pixbuf.
 */
QPixmap * SlavGPS::ui_pixmap_set_alpha(QPixmap * pixmap, uint8_t alpha)
{
#ifdef K_TODO
	unsigned char *pixels;
	int iii, jjj;

	if (!pixmap->hasAlphaChannel()) {
		QPixmap * tmp = gdk_pixbuf_add_alpha(pixmap, false,0,0,0);
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
				*pixels = alpha;
			}
			pixels++;
		}
	}
#endif
	return pixmap;
}




QPixmap SlavGPS::ui_pixmap_set_alpha(const QPixmap & input, int alpha)
{
	QImage image(input.size(), QImage::Format_ARGB32_Premultiplied);
	QPainter painter(&image);
	painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
	painter.setOpacity(alpha / 255.0);
	painter.drawPixmap(0, 0, input);
	QPixmap output = QPixmap::fromImage(image);

	return output;
}




/**
 * Reduce the alpha value of the specified pixbuf by alpha / 255.
 */
QPixmap * SlavGPS::ui_pixmap_scale_alpha(QPixmap * pixmap, uint8_t alpha)
{
#ifdef K_TODO
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
				*pixels = (uint8_t)(((uint16_t)*pixels * (uint16_t)alpha) / 255);
			}
			pixels++;
		}
	}
#endif
	return pixmap;
}




void SlavGPS::ui_add_recent_file(const QString & file_full_path)
{
#ifdef K_TODO
	if (!file_full_path.isEmpty()) {
		GtkRecentManager * manager = gtk_recent_manager_get_default();
		GFile * file = g_file_new_for_commandline_arg(file_full_path);
		char * uri = g_file_get_uri (file);
		if (uri && manager) {
			gtk_recent_manager_add_item(manager, uri);
		}
		g_object_unref(file);
		free(uri);
	}
#endif
}





SGItem::SGItem() : QStandardItem()
{

}




SGItem::SGItem(QString const & text_) : QStandardItem(text_)
{
	this->setText(text_);
	this->setData(QVariant::fromValue(text_), RoleLayerData);
}




SGItem::SGItem(Geoname * geoname) : QStandardItem()
{
	this->setText(QString(geoname->name));
	this->setData(QVariant::fromValue(geoname), RoleLayerData);
}




SGItem::SGItem(Track * trk) : QStandardItem()
{
	this->setText(trk->name);
	this->setData(QVariant::fromValue(trk), RoleLayerData);
}




SGItem::SGItem(Waypoint * wp) : QStandardItem()
{
	this->setText(wp->name);
	this->setData(QVariant::fromValue(wp), RoleLayerData);
}




QStandardItem * SGItem::clone() const
{
	return new SGItem();
}
