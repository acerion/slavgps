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
		char * get_url_for_coords(struct LatLon start, struct LatLon end);
		bool find(LayerTRW * trw, struct LatLon start, struct LatLon end);
		char * get_url_from_directions(const char * start, const char * end);
		bool supports_direction(void);
		char * get_url_for_track(Track * trk);
		bool refine(LayerTRW * trw, Track * trk);
		bool supports_refine(void);

		char * url_base = NULL; /* URL's base. The base URL of the routing engine. */

		/* LatLon. */
		char * url_start_ll_fmt = NULL; /* Start part of the URL. The part of the request hosting the start point. */
		char * url_stop_ll_fmt = NULL; /* Stop part of the URL. The part of the request hosting the end point. */
		char * url_via_ll_fmt = NULL; /* Via part of the URL. The param of the request for setting a via point. */

		/* Directions. */
		char * url_start_dir_fmt = NULL; /* Start part of the URL. The part of the request hosting the start point. */
		char * url_stop_dir_fmt = NULL;  /* Stop part of the URL. The part of the request hosting the end point. */

		DownloadOptions dl_options;

	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_ROUTING_ENGINE_WEB_H_ */
