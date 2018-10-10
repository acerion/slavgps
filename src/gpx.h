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




	class GPXImporter {
	public:
		GPXImporter(LayerTRW * trw);
		~GPXImporter();
		size_t write(const char * data, size_t size);

		LayerTRW * trw = NULL;

		XML_Parser parser;
		enum XML_Status status = XML_STATUS_ERROR;
	};




	class GPX {
	public:
		static sg_ret read_layer_from_file(QFile & file, LayerTRW * trw);
		static sg_ret write_layer_to_file(QFile & file, LayerTRW * trw, GPXWriteOptions * options);
		static sg_ret write_track_to_file(QFile & file, Track * trk, GPXWriteOptions * options);

		static sg_ret write_layer_to_tmp_file(QString & file_full_path, LayerTRW * trw, GPXWriteOptions * options);
		static sg_ret write_track_to_tmp_file(QString & file_full_path, Track * trk, GPXWriteOptions * options);

	private:
		static sg_ret write_layer_track_to_tmp_file(QString & file_full_path, LayerTRW * trw, Track * trk, GPXWriteOptions * options);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_GPX_H_ */
