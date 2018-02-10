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




namespace SlavGPS {




	class LayerTRW;
	class Track;




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




	class GPX {
	public:
		static bool read_file(FILE * file, LayerTRW * trw);
		static void write_file(FILE * file, LayerTRW * trw, GPXWriteOptions * options);
		static void write_track_file(FILE * file, Track * trk, GPXWriteOptions * options);

		static QString write_tmp_file(LayerTRW * trw, GPXWriteOptions * options);
		static QString write_track_tmp_file(Track * trk, GPXWriteOptions * options);

	private:
		static QString write_tmp_file(LayerTRW * trw, Track * trk, GPXWriteOptions * options);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_GPX_H_ */
