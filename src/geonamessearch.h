/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2009, Hein Ragas
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

#ifndef _SG_GEONAMESSEARCH_H_
#define _SG_GEONAMESSEARCH_H_




#include <QObject>

#include "coords.h"
#include "globals.h"




namespace SlavGPS {




	class Window;
	class LayerTRW;




	/* Type to contain data returned from GeoNames.org */
	class Geoname : public QObject {
		Q_OBJECT
	public:
		Geoname() {};
		~Geoname() {};

		QString name;
		QString feature;
		LatLon ll;
		double elevation = VIK_DEFAULT_ALTITUDE;
		QString comment;
		QString desc;
	};




	/* Finding Wikipedia entries within a certain box. */
	void a_geonames_wikipedia_box(Window * window, LayerTRW * trw, const LatLonMinMax & min_max);




} /* namespace SlavGPS */




Q_DECLARE_METATYPE(SlavGPS::Geoname*)




#endif /* #ifndef _SG_GEONAMESSEARCH_H_ */
