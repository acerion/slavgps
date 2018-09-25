/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifndef _SG_OSM_TRACES_H_
#define _SG_OSM_TRACES_H_




#include <QString>
#include <QLineEdit>




namespace SlavGPS {




	class LayerTRW;
	class Track;




	class OSMTraces {
	public:
		static void init(void);
		static void uninit(void);
		static void upload_trw_layer(LayerTRW * trw, Track * trk);

		static void save_current_credentials(const QString & user, const QString & password);
		static QString get_current_credentials(void);
		static void fill_credentials_widgets(QLineEdit & user_entry, QLineEdit & password_entry);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_OSM_TRACES_H_ */
