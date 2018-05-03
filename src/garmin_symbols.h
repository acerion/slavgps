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
 */

#ifndef _SG_GARMIN_SYMBOLS_H_
#define _SG_GARMIN_SYMBOLS_H_




#include <QPixmap>
#include <QComboBox>




namespace SlavGPS {




	class GarminSymbols {
	public:
		static QPixmap * get_wp_symbol(const QString & symbol_name);
		static QString get_normalized_symbol_name(const QString & symbol_name);

		static void populate_symbols_list(QComboBox * symbol_list, const QString & preselected_symbol_name);

		/* See if specific symbol name is equal to name used by this module to indicate "no Garmin symbol selected". */
		static bool is_none_symbol_name(const QString & symbol_name);

		/* Use when preferences have changed to reload icons. */
		static void clear_symbols(void);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_GARMIN_SYMBOLS_H_ */
