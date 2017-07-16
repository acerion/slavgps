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




	/**
	 * Options adapting GPX writing.
	 */
	typedef struct {
		/* NB force options only apply to trackpoints. */
		bool force_ele;  /* Force ele field. */
		bool force_time; /* Force time field. */
		bool hidden;     /* Write invisible tracks/waypoints (default is yes). */
		bool is_route;   /* For internal convience. */
	} GpxWritingOptions;




	bool a_gpx_read_file(LayerTRW * trw, FILE *f );
	void a_gpx_write_file(LayerTRW * trw, FILE *f, GpxWritingOptions * options);
	void a_gpx_write_track_file(Track * trk, FILE *f, GpxWritingOptions * options);

	char * a_gpx_write_tmp_file(LayerTRW * trw, GpxWritingOptions * options);
	char * a_gpx_write_track_tmp_file(Track * trk, GpxWritingOptions * options);




} /* namespace SlavGPS */




#endif /* #ifndef _SG_GPX_H_ */
