/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2009, Hein Ragas
 * Copyright (C) 2016-2018, Kamil Ignacak <acerion@wp.pl>
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

#ifndef _SG_GEONAMES_SEARCH_H_
#define _SG_GEONAMES_SEARCH_H_




#include <QObject>




#include "coords.h"
#include "measurements.h"




namespace SlavGPS {




	class Window;
	class LayerTRW;
	class Waypoint;
	class AcquireProgressDialog;
	class BasicDialog;
	class ListSelectionWidget;
	enum class CoordMode;




	/* Type to contain data returned from GeoNames.org */
	class Geoname : public QObject {
		Q_OBJECT
	public:
		Geoname() {};
		Geoname(const Geoname & geoname);
		~Geoname() {};

		Waypoint * create_waypoint(CoordMode coord_mode) const;

		QString name;
		QString feature;
		LatLon lat_lon;
		Altitude elevation;
		QString comment;
		QString desc;
	};




	class Geonames {
	public:
		/* Create waypoints from Wikipedia entries within a certain box. */
		static void create_wikipedia_waypoints(LayerTRW * trw, const LatLonBBox & bbox, Window * window);

		static std::list<Geoname *> generate_geonames(const LatLonBBox & bbox, AcquireProgressDialog * progress_dialog);
		static std::list<Geoname *> select_geonames(const std::list<Geoname *> & all_geonames, AcquireProgressDialog * progress_dialog, ListSelectionWidget & list_widget);

	private:
		static std::list<Geoname *> select_from_list(BasicDialog & dialog, const QString & title, const QStringList & headers, std::list<Geoname *> & geonames, Window * parent);
	};




} /* namespace SlavGPS */




Q_DECLARE_METATYPE(SlavGPS::Geoname*)




#endif /* #ifndef _SG_GEONAMES_SEARCH_H_ */
