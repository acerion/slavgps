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
 */

#ifndef _SG_UI_UTIL_H_
#define _SG_UI_UTIL_H_




#include <cstdint>




#include <QLabel>
#include <QStandardItem>
#include <QPixmap>




namespace SlavGPS {




	class Window;




	void open_url(const QString & url);
	void new_email(const QString & address, Window * parent);

	int ui_get_gtk_settings_integer(const char * property_name, int default_value);
	QLabel * ui_label_new_selectable(QString const & text, QWidget * parent = NULL);

	/* @alpha should be in range 0-255 */
	QPixmap ui_pixmap_set_alpha(const QPixmap & input, int alpha);

	QPixmap * ui_pixmap_set_alpha(QPixmap * pixmap, uint8_t alpha);
	QPixmap * ui_pixmap_scale_alpha(QPixmap * pixmap, uint8_t alpha);

	void ui_add_recent_file(const QString & file_full_path);




}




#endif /* #ifndef _SG_UI_UTIL_H_ */
