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

#ifndef _SG_ROUTING_ENGINE_WEB_H_
#define _SG_ROUTING_ENGINE_WEB_H_




#include <cstdint>




#include "routing_engine.h"




namespace SlavGPS {




	class Track;




	class RoutingEngineWeb : public RoutingEngine {

	public:
		RoutingEngineWeb() {};
		~RoutingEngineWeb();


		const DownloadOptions * get_download_options(void) const;

		bool find(LayerTRW * trw, const LatLon & start, const LatLon & end);

		QString get_url_for_coords(const LatLon & start, const LatLon & end);
		QString get_url_from_directions(const QString & start, const QString & end);
		QString get_url_for_track(Track * trk);

		bool refine(LayerTRW * trw, Track * trk);

		bool supports_refine(void);
		bool supports_direction(void);

		QString url_base; /* URL's base. The base URL of the routing engine. */

		/* LatLon. */
		QString url_start_ll_fmt; /* Start part of the URL. The part of the request hosting the start point. */
		QString url_stop_ll_fmt;  /* Stop part of the URL. The part of the request hosting the end point. */
		QString url_via_ll_fmt;   /* Via part of the URL. The param of the request for setting a via point. */

		/* Directions. */
		QString url_start_dir_fmt; /* Start part of the URL. The part of the request hosting the start point. */
		QString url_stop_dir_fmt;  /* Stop part of the URL. The part of the request hosting the end point. */

		DownloadOptions dl_options;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_ROUTING_ENGINE_WEB_H_ */
