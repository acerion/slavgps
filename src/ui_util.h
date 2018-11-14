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




#include <QLabel>
#include <QStandardItem>
#include <QPixmap>




namespace SlavGPS {




	class Window;




	void open_url(const QString & url);

	QLabel * ui_label_new_selectable(QString const & text, QWidget * parent = NULL);

	/* @alpha should be in range 0-255 */
	void ui_pixmap_set_alpha(QPixmap & pixmap, int alpha);
	void ui_pixmap_scale_alpha(QPixmap & pixmap, int alpha);

	void ui_pixmap_scale_size_by(QPixmap & pixmap, double scale_x, double scale_y);
	void ui_pixmap_scale_size_to(QPixmap * pixmap, int width, int height);

	void update_desktop_recent_documents(Window * window, const QString & file_full_path, const QString & mime_type);




}




#endif /* #ifndef _SG_UI_UTIL_H_ */
