/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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




#ifndef _SG_GOTO_H_
#define _SG_GOTO_H_




#include <QString>
#include <QComboBox>
#include <QLineEdit>




#include "dialog.h"
#include "goto_tool.h"




namespace SlavGPS {




	class Window;
	class GisViewport;




	class GoTo {
	public:
		static void uninit(void);

		static void register_tool(GotoTool * tool);

		static bool goto_location(Window * window, GisViewport * gisview);
		static sg_ret goto_latlon(Window * window, GisViewport * gisview);
		static sg_ret goto_utm(Window * window, GisViewport * gisview);

		static int where_am_i(GisViewport * gisview, LatLon & lat_lon, QString & name);
	};




	class GotoDialog : public BasicDialog {
		Q_OBJECT
	public:
		GotoDialog(const QString & initial_location, QWidget * parent = NULL);
		~GotoDialog();

		QComboBox providers_combo;
		QLineEdit input_field;

	private slots:
		void text_changed_cb(const QString & text);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_GOTO_H_ */
