/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _SG_KMZ_H_
#define _SG_KMZ_H_




#include <QPixmap>
#include <QString>




namespace SlavGPS {




	class Viewport;
	class LayersPanel;



	enum class KMZOpenStatus {
		KMZNotSupported = -1,    /* KMZ not supported (this shouldn't happen). */
		Success = 0,
		ZipError,                /* Problems with zip archive (zip error code returned in second field of tuple). */
		NoDoc = 128,             /* No doc.kml file in KMZ. */
		CantUnderstandDoc = 129, /* Couldn't understand the doc.kml file. */
		NoBounds = 130,          /* Couldn't get bounds from KML (error not detected ATM). */
		NoImage = 131,           /* No image file in KML. */
		CantGetImage = 132,      /* Couldn't get image from KML. */
		ImageFileProblem,        /* Image file problem. */
	};



	enum {
		SG_KMZ_OPEN_KML = 0,
		SG_KMZ_OPEN_ZIP = 1
	};


	int kmz_save_file(const QPixmap & pixmap, const QString & file_full_path, double north, double east, double south, double west);
	std::tuple<KMZOpenStatus, int> kmz_open_file(const QString & file_full_path, Viewport * viewport, LayersPanel * panel);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_KMZ_H_ */
