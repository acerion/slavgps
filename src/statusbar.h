/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef _SG_STATUSBAR_H_
#define _SG_STATUSBAR_H_




#include <cstdint>
#include <vector>

#include <QObject>
#include <QStatusBar>




#if 0
	struct _VikStatusbarClass {
		GtkStatusbarClass statusbar_class;

		void (* clicked) (VikStatusbar * vs, int item);
	};
#endif





namespace SlavGPS {




	enum class StatusBarField {
		TOOL,
		ITEMS,
		ZOOM,
		INFO,
		POSITION,
		NUM_FIELDS
	};




	class StatusBar : public QStatusBar {
		Q_OBJECT
	public:
		StatusBar(QWidget * parent);
		~StatusBar();

		void set_message(StatusBarField field, QString const & message);

	private:

		std::vector<QWidget *> fields;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_STATUSBAR_H_ */