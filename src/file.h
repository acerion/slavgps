 /*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2012, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2016-2020, Kamil Ignacak <acerion@wp.pl>
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

#ifndef _SG_VIK_FILE_H_
#define _SG_VIK_FILE_H_




#include <QFile>




#include "ui_builder.h"




namespace SlavGPS {




	class GisViewport;
	class LayerAggregate;
	class LayerTRW;
	class Track;




	enum class LayerDataReadStatus {
		Error,
		Success,
		Unrecognized, /* E.g. unrecognized section. */
	};




	enum class SGFileType {
		GPSPoint  = 1,
		GPSMapper = 2,
		GPX       = 3,
		KML       = 4,
		GeoJSON   = 5
	};






	QString append_file_ext(const QString & file_name, SGFileType file_type);


	char const * file_GetRelativeFilename(const char * currentDirectory, const char * absoluteFilename);
	QString file_GetRelativeFilename(const QString & current_dir_path, const QString & file_path);




	class VikFile {
	public:

		static SaveStatus save(LayerAggregate * top_layer, GisViewport * gisview, const QString & file_full_path);
		static LoadStatus load(LayerAggregate * top_layer, GisViewport * gisview, const QString & file_full_path);

		/* Function to determine if a filename is a 'viking' type file. */
		static bool has_vik_file_magic(const QString & file_full_path);




		/* Not exactly for exporting to Viking file. */

		/* Only need to define Track if the file type is SGFileType::GPX_TRACK. */
		static SaveStatus export_trw(LayerTRW * trw, const QString & file_full_path, SGFileType file_type, Track * trk, bool write_hidden);
		static SaveStatus export_trw_track(Track * trk, const QString & file_full_path, SGFileType file_type, bool write_hidden);
		static SaveStatus export_trw_layer(LayerTRW * trw, const QString & file_full_path, SGFileType file_type, bool write_hidden);

		static SaveStatus export_with_babel(LayerTRW * trw, const QString & output_file_full_path, const QString & output_data_format, bool tracks, bool routes, bool waypoints);

	private:
		static LoadStatus read_file(QFile & file, LayerAggregate * top_layer, const QString & dirpath, GisViewport * gisview);
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_VIK_FILE_H_ */
