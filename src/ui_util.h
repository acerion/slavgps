/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2007-2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
 * Copyright (C) 2016-2020, Kamil Ignacak <acerion@wp.pl>
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
#include <QPixmap>
#include <QString>




#include "globals.h"




namespace SlavGPS {




	class Window;




	class ImageAlpha {
	public:
		explicit ImageAlpha(int value = ImageAlpha::max());

		int value(void) const { return this->m_value; }
		sg_ret set_value(int value);

		sg_ret set_from_string(const char * string);
		sg_ret set_from_string(const QString & string);

		const QString value_to_string_for_file(void) const;

		/**
		   @brief Get value in range 0.0 to 1.0

		   Such value is useful e.g. for passing to Qt's
		   API.
		*/
		double fractional_value(void) const;

		static int max(void) { return 255; };
		static int min(void) { return 0; };

	private:
		int m_value = ImageAlpha::max();
	};




	void open_url(const QString & url);

	QLabel * ui_label_new_selectable(QString const & text, QWidget * parent = NULL);

	void ui_pixmap_set_alpha(QPixmap & pixmap, const ImageAlpha & alpha);
	void ui_pixmap_scale_alpha(QPixmap & pixmap, const ImageAlpha & alpha);

	void ui_pixmap_scale_size_by(QPixmap & pixmap, double scale_x, double scale_y);
	void ui_pixmap_scale_size_to(QPixmap * pixmap, int width, int height);

	void update_desktop_recent_documents(Window * window, const QString & file_full_path, const QString & mime_type);




}




#endif /* #ifndef _SG_UI_UTIL_H_ */
