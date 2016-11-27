/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2007-2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
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
 *
 */

#ifndef _SG_UI_UTIL_H_
#define _SG_UI_UTIL_H_

#include <cstdint>

#include <QLabel>
#include <QStandardItem>

#include <glib.h>
//#include <gtk/gtk.h>
#include "slav_qt.h"
#include "track.h"




void open_url(GtkWindow * parent, const char * url);
void new_email(GtkWindow * parent, const char * address);

GtkWidget * ui_button_new_with_image(const char * stock_id, const char * text);
int ui_get_gtk_settings_integer(const char *property_name, int default_value);
GtkWidget * ui_lookup_widget(GtkWidget * widget, const char * widget_name);
QLabel * ui_label_new_selectable(QString const & text, QWidget * parent = NULL);

GdkPixbuf * ui_pixbuf_set_alpha(GdkPixbuf * pixbuf, uint8_t alpha);
GdkPixbuf * ui_pixbuf_scale_alpha(GdkPixbuf * pixbuf, uint8_t alpha);
void ui_add_recent_file(const char * filename);




namespace SlavGPS {




	class SGItem : public QStandardItem {
	public:
		SGItem();
		SGItem(Track * trk);
		SGItem(QString const & text);
		~SGItem() {};

		virtual QStandardItem * clone() const;
	};



}




#endif /* #ifndef _SG_UI_UTIL_H_ */
