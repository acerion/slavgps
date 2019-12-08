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
		RoutingEngine(const QString & id, const QString & name, const QString & format) :
			m_id(id), m_name(name), m_format(format) {}
		virtual ~RoutingEngine() {}

		virtual QString get_url_from_directions(const QString & start, const QString & end) const;

		virtual bool supports_refine(void) const;
		virtual bool supports_direction(void);

		virtual bool find_route(LayerTRW * trw, const LatLon & start, const LatLon & end) const;
		virtual bool refine_route(LayerTRW * trw, Track * route) const;

		const QString & get_id(void) const { return this->m_id; }
		const QString & get_name(void) const { return this->m_name; }
		const QString & get_format(void) const { return this->m_format; }

	protected:
		const QString m_id;      /* The unique identifier of the routing engine. */
		const QString m_name;    /* User-facing label of the routing engine. */
		const QString m_format;  /* The format of the output (see gpsbabel). */
	};




	/* RoutingEnginePredicate */
	bool routing_engine_supports_refine(const RoutingEngine * engine);
}




#endif /* #ifndef _SG_ROUTING_ENGINE_H_ */
