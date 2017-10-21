/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2012, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifndef _SG_FILE_H_
#define _SG_FILE_H_



#include <cstdio>
#include <cstdint>

#include "ui_builder.h"




namespace SlavGPS {




	class Viewport;
	class LayerAggregate;
	class LayerTRW;
	class Track;



	enum class SGFileTypeFilter {
		ANY = 0,
		IMAGE,   // JPG+PNG+TIFF
		MBTILES,
		XML,
		CARTO,   // MML + MSS
		LAST
	};



	enum class SGFileType {
		GPSPOINT  = 1,
		GPSMAPPER = 2,
		GPX       = 3,
		KML       = 4,
		GEOJSON   = 5
	};

	typedef enum {
		LOAD_TYPE_READ_FAILURE,
		LOAD_TYPE_GPSBABEL_FAILURE,
		LOAD_TYPE_GPX_FAILURE,
		LOAD_TYPE_UNSUPPORTED_FAILURE,
		LOAD_TYPE_VIK_FAILURE_NON_FATAL,
		LOAD_TYPE_VIK_SUCCESS,
		LOAD_TYPE_OTHER_SUCCESS,
	} VikLoadType_t;



	bool a_file_check_ext(const QString & file_name, char const * fileext);

	/* Function to determine if a filename is a 'viking' type file. */
	bool check_file_magic_vik(char const * filename);

	QString append_file_ext(const QString & file_name, SGFileType file_type);
	VikLoadType_t a_file_load(LayerAggregate * top, Viewport * viewport, char const * filename);
	bool a_file_save(LayerAggregate * top, Viewport * viewport, char const * filename);

	/* Only need to define Track if the file type is SGFileType::GPX_TRACK. */
	bool a_file_export(LayerTRW * trw, const QString & file_path, SGFileType file_type, Track * trk, bool write_hidden);
	bool a_file_export_track(Track * trk, const QString & file_path, SGFileType file_type, bool write_hidden);
	bool a_file_export_layer(LayerTRW * trw, const QString & file_path, SGFileType file_type, bool write_hidden);

	bool a_file_export_babel(LayerTRW * trw, const QString & output_file_path, const QString & output_file_type, bool tracks, bool routes, bool waypoints);

	void file_write_layer_param(FILE * f, char const * param_name, SGVariantType type, const SGVariant & param_value);
	char * file_realpath(char const * path, char * real);
	char * file_realpath_dup(char const * path);
	char const * file_GetRelativeFilename(const char * currentDirectory, const char * absoluteFilename);
	QString file_GetRelativeFilename(const QString & current_dir_path, const QString & file_path);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_FILE_H_ */
