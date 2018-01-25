/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (c) 2014, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_GEOJSON_H_
#define _SG_GEOJSON_H_




#include <cstdio>




#include <QString>




namespace SlavGPS {




	class LayerTRW;




	bool geojson_write_file(FILE * file, LayerTRW * trw);

	QString geojson_program_export(void);
	QString geojson_program_import(void);

	char * geojson_import_to_gpx(const QString & file_full_path);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_GEOJSON_H_ */
