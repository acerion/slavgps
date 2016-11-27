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

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include "util.h"
#include "ui_util.h"
#include "dialog.h"
#include "track.h"

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

	cmdline = g_strdup_printf("%s '%s'", cmd, arg);
	fprintf(stderr, "DEBUG: Running: %s\n", cmdline);

	status = g_spawn_command_line_async(cmdline, NULL);

	free(cmdline);

	return status;
}
#endif
*/




/* Annoyingly gtk_show_uri() doesn't work so resort to ShellExecute method
   (non working at least in our Windows build with GTK+2.24.10 on Windows 7). */

void open_url(GtkWindow * parent, const char * url)
{
#ifdef K
#ifdef WINDOWS
	ShellExecute(NULL, NULL, (char *) url, NULL, ".\\", 0);
#else
	GError * error = NULL;
	gtk_show_uri(gtk_widget_get_screen(GTK_WIDGET(parent)), url, GDK_CURRENT_TIME, &error);
	if (error) {
		dialog_error(QString("Could not launch web browser. %1").arg(QString(error->message)), parent);
		g_error_free(error);
	}
#endif
#endif
}




void new_email(GtkWindow * parent, const char * address)
{
#ifdef K
	char * uri = g_strdup_printf("mailto:%s", address);
	GError *error = NULL;
	gtk_show_uri(gtk_widget_get_screen(GTK_WIDGET(parent)), uri, GDK_CURRENT_TIME, &error);
	if (error) {
		dialog_error(QString("Could not create new email. %1").arg(QString(error->message)), parent);
		g_error_free(error);
	}
	/*
	  #ifdef WINDOWS
	  ShellExecute(NULL, NULL, (char *) uri, NULL, ".\\", 0);
	  #else
	  if (!spawn_command_line_async("xdg-email", uri))
	  dialog_error("Could not create new email.", parent);
	  #endif
	*/
	free(uri);
	uri = NULL;
#endif
}




/**
   Creates a @c GtkButton with custom text and a stock image similar to gtk_button_new_from_stock().

   @param stock_id A @c GTK_STOCK_NAME string.
   @param text Button label text, can include mnemonics.
   @return The new @c GtkButton.
*/
GtkWidget * ui_button_new_with_image(const char * stock_id, const char * text)
{
#ifdef K
	GtkWidget *image, *button;

	button = gtk_button_new_with_mnemonic(text);
	gtk_widget_show(button);
	image = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image(GTK_BUTTON(button), image);
	/* Note: image is shown by gtk. */
	return button;
#endif
}




/**
   Reads an integer from the GTK default settings registry
   (see http://library.gnome.org/devel/gtk/stable/GtkSettings.html).
   @param property_name The property to read.
   @param default_value The default value in case the value could not be read.
   @return The value for the property if it exists, otherwise the @a default_value.
*/
int ui_get_gtk_settings_integer(const char * property_name, int default_value)
{
#ifdef K
	if (g_object_class_find_property(G_OBJECT_GET_CLASS(G_OBJECT(
		gtk_settings_get_default())), property_name)) {

		int value;
		g_object_get(G_OBJECT(gtk_settings_get_default()), property_name, &value, NULL);
		return value;
	} else {
		return default_value;
	}
#endif
}




/**
   Returns a widget from a name in a component, usually created by Glade.
   Call it with the toplevel widget in the component (i.e. a window/dialog),
   or alternatively any widget in the component, and the name of the widget
   you want returned.
   @param widget Widget with the @a widget_name property set.
   @param widget_name Name to lookup.
   @return The widget found.
   @see ui_hookup_widget().
*/
GtkWidget * ui_lookup_widget(GtkWidget * widget, const char * widget_name)
{
#ifdef K
	GtkWidget *parent, *found_widget;

	if (!widget) {
		return NULL;
	}
	if (!widget_name) {
		return NULL;
	}

	for (;;) {
		if (GTK_IS_MENU(widget)) {
			parent = gtk_menu_get_attach_widget(GTK_MENU(widget));
		} else {
			parent = gtk_widget_get_parent(widget);
		}

		if (parent == NULL) {
			parent = (GtkWidget*) g_object_get_data(G_OBJECT(widget), "GladeParentKey");
		}

		if (parent == NULL) {
			break;
		}
		widget = parent;
	}

	found_widget = (GtkWidget*) g_object_get_data(G_OBJECT(widget), widget_name);
	if (G_UNLIKELY(found_widget == NULL)) {
		fprintf(stderr, "WARNING: Widget not found: %s\n", widget_name);
	}
	return found_widget;
#endif
}




/**
 * Returns a label widget that is made selectable (i.e. the user can copy the text).
 * @param text String to display - maybe NULL
 * @return The label widget
 */
QLabel * ui_label_new_selectable(QString const & text, QWidget * parent)
{
	QLabel * label = new QLabel(text, parent);
	label->setTextInteractionFlags(Qt::TextSelectableByMouse);
	return label;
}




/**
 * Apply the alpha value to the specified pixbuf.
 */
GdkPixbuf * ui_pixbuf_set_alpha(GdkPixbuf * pixbuf, uint8_t alpha)
{
#ifdef K
	unsigned char *pixels;
	int width, height, iii, jjj;

	if (!gdk_pixbuf_get_has_alpha (pixbuf)) {
		GdkPixbuf *tmp = gdk_pixbuf_add_alpha(pixbuf,false,0,0,0);
		g_object_unref(G_OBJECT(pixbuf));
		pixbuf = tmp;
	 	if (!pixbuf) {
			return NULL;
		}
	}

	pixels = gdk_pixbuf_get_pixels(pixbuf);
	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);

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
	return pixbuf;
#endif
}




/**
 * Reduce the alpha value of the specified pixbuf by alpha / 255.
 */
GdkPixbuf * ui_pixbuf_scale_alpha(GdkPixbuf * pixbuf, uint8_t alpha)
{
#ifdef K
	unsigned char *pixels;
	int width, height, iii, jjj;

	if (!gdk_pixbuf_get_has_alpha(pixbuf)) {
		GdkPixbuf * tmp = gdk_pixbuf_add_alpha(pixbuf,false,0,0,0);
		g_object_unref(G_OBJECT(pixbuf));
		pixbuf = tmp;
		if (!pixbuf) {
			return NULL;
		}
	}

	pixels = gdk_pixbuf_get_pixels(pixbuf);
	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);

	/* r,g,b,a,r,g,b,a.... */
	for (iii = 0; iii < width; iii++)
		for (jjj = 0; jjj < height; jjj++) {
			pixels += 3;
			if (*pixels != 0) {
				*pixels = (uint8_t)(((uint16_t)*pixels * (uint16_t)alpha) / 255);
			}
			pixels++;
		}
	return pixbuf;
#endif
}




void ui_add_recent_file(const char * filename)
{
#ifdef K
	if (filename) {
		GtkRecentManager * manager = gtk_recent_manager_get_default();
		GFile * file = g_file_new_for_commandline_arg(filename);
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




SGItem::SGItem(QString const & text) : QStandardItem(text)
{
	this->setText(text);
	this->setData(QVariant::fromValue(text), RoleLayerData);
}




SGItem::SGItem(Track * trk) : QStandardItem()
{
	this->setText(QString(trk->name));
	this->setData(QVariant::fromValue(trk), RoleLayerData);
}




QStandardItem * SGItem::clone() const
{
	return new SGItem();
}
