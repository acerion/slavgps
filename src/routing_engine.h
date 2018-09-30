/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2012-2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifndef _SG_ROUTING_ENGINE_H_
#define _SG_ROUTING_ENGINE_H_




#include <cstdint>




#include <QString>




#include "coords.h"
#include "download.h"




namespace SlavGPS {




	class LayerTRW;
	class Track;




	class RoutingEngine {
	public:
		RoutingEngine();
		~RoutingEngine();

		virtual bool find(LayerTRW * trw, const LatLon & start, const LatLon & end);
		virtual QString get_url_from_directions(const QString & start, const QString & end);
		virtual bool supports_direction(void);
		virtual bool refine(LayerTRW * trw, Track * trk);
		virtual bool supports_refine(void);

		QString get_id(void) const { return this->id; }
		QString get_label(void) const { return this->label; }
		QString get_format(void) const { return this->format; }

		QString id;     /* The identifier of the routing engine. */
		QString label;  /* The label of the routing engine. */
		QString format; /* The format of the output (see gpsbabel). */
	};




	/* RoutingEnginePredicate */
	bool routing_engine_supports_refine(RoutingEngine * engine);
}




#endif /* #ifndef _SG_ROUTING_ENGINE_H_ */
