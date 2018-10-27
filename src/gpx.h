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

#ifndef _SG_GPX_H_
#define _SG_GPX_H_




#include <cstdint>




#include <QFile>




#include <expat.h>




#include "globals.h"
#include "coords.h"




namespace SlavGPS {




	class LayerTRW;
	class TRWMetadata;
	class Track;
	class Waypoint;
	class Trackpoint;




	typedef enum {
		tt_unknown = 0,

		tt_gpx,
		tt_gpx_name,
		tt_gpx_desc,
		tt_gpx_author,
		tt_gpx_time,
		tt_gpx_keywords,

		tt_wpt,
		tt_wpt_cmt,
		tt_wpt_desc,
		tt_wpt_src,
		tt_wpt_type,
		tt_wpt_name,
		tt_wpt_ele,
		tt_wpt_sym,
		tt_wpt_time,
		tt_wpt_url,
		tt_wpt_link,            /* New in GPX 1.1 */

		tt_trk,
		tt_trk_cmt,
		tt_trk_desc,
		tt_trk_src,
		tt_trk_type,
		tt_trk_name,

		tt_rte,

		tt_trk_trkseg,
		tt_trk_trkseg_trkpt,
		tt_trk_trkseg_trkpt_ele,
		tt_trk_trkseg_trkpt_time,
		tt_trk_trkseg_trkpt_name,
		/* extended */
		tt_trk_trkseg_trkpt_course,
		tt_trk_trkseg_trkpt_speed,
		tt_trk_trkseg_trkpt_fix,
		tt_trk_trkseg_trkpt_sat,

		tt_trk_trkseg_trkpt_hdop,
		tt_trk_trkseg_trkpt_vdop,
		tt_trk_trkseg_trkpt_pdop,

		tt_waypoint,
		tt_waypoint_coord,
		tt_waypoint_name,
	} tag_type_t;



	class GPXWriteOptions {
	public:
		GPXWriteOptions(bool new_force_ele, bool new_force_time, bool new_hidden, bool new_is_route)
			: force_ele(new_force_ele), force_time(new_force_time), hidden(new_hidden), is_route(new_is_route) {};

		/* 'force' options only apply to trackpoints. */
		bool force_ele = false;  /* Force ele field. */
		bool force_time = false; /* Force time field. */
		bool hidden = false;     /* Write invisible tracks/waypoints (default is yes). */
		bool is_route = false;   /* For internal convience. */
	};




	class GPXImporter {
	public:
		GPXImporter(LayerTRW * trw);
		~GPXImporter();
		sg_ret write(const char * data, size_t size);

		bool set_lat_lon(char const ** attributes);

		LayerTRW * trw = NULL;

		XML_Parser parser;
		enum XML_Status status = XML_STATUS_ERROR;

		size_t n_bytes = 0;


		/* Parser data. */
		Track * trk = NULL;
		QString trk_name;
		Waypoint * wp = NULL;
		QString wp_name;
		Trackpoint * tp = NULL;
		TRWMetadata * md = NULL;

		QString cdata;
		QString xpath;

		unsigned int unnamed_waypoints = 0;
		unsigned int unnamed_tracks = 0;
		unsigned int unnamed_routes = 0;
		bool f_tr_newseg = false;
		tag_type_t current_tag_type = tt_unknown;

		QString slat;
		QString slon;
		LatLon lat_lon;
	};




	class GPX {
	public:
		static sg_ret read_layer_from_file(QFile & file, LayerTRW * trw);
		static sg_ret write_layer_to_file(FILE * file, LayerTRW * trw, GPXWriteOptions * options);
		static sg_ret write_track_to_file(FILE * file, Track * trk, GPXWriteOptions * options);

		static sg_ret write_layer_to_tmp_file(QString & file_full_path, LayerTRW * trw, GPXWriteOptions * options);
		static sg_ret write_track_to_tmp_file(QString & file_full_path, Track * trk, GPXWriteOptions * options);

	private:
		static sg_ret write_layer_track_to_tmp_file(QString & file_full_path, LayerTRW * trw, Track * trk, GPXWriteOptions * options);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_GPX_H_ */
