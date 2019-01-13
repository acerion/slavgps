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




#include <algorithm>
#include <cstring>
#include <cstdlib>




#include "gpsmapper.h"
#include "layer_trw.h"
#include "layer_trw_track_internal.h"




using namespace SlavGPS;




#define SG_MODULE "GPSMapper"




/*
  Name of layer -> RGN type and Type.
  format: Name RGN40 0x40
  or:     Name RGN10 0x2f06

  Returns 0 if invalid/no rgn stuff, else returns len of.
*/
static unsigned int print_rgn_stuff(FILE * file, char const * nm)
{
	if (!nm) {
		return 0;
	}

	char * name = strdup(nm);
	unsigned int len = strlen(name);

	/* --------------------------------------------- */
	/* added by oddgeir@oddgeirkvien.com, 2005.02.02 */
	/* Format may also be: Name RGN40 0x40 Layers=1  */
	/* or: Name RGN10 0x2f06 Layers=1                */

	char * layers = NULL;
	if (len > 20 && strncasecmp(name + len - 8, "LAYERS=", 7) == 0) { /* Layers is added to the description. */
		layers = name + len - 8;
		*(name + len - 9) = 0;
		len = strlen(name);
	} else {
		layers = 0;
	}
	/* --------------------------------------------- */


	if (len > 11 && strncasecmp(name + len - 10, "RGN", 3) == 0
	    && strncasecmp(name + len - 4, "0x", 2) == 0) {

		fprintf(file, "[%.5s]\nType=%.4s\nLabel=", name + len - 10, name + len - 4);
		fwrite(name, sizeof(char), len - 11, file);
		fprintf(file, "\n");

		/* Added by oddgeir@oddgeirkvien.com, 2005.02.02 */
		if (layers) {
			fprintf(file, "%s\n", layers);
		}

		free(name);

		return len - 11;

	} else if (len > 13 && strncasecmp(name + len - 12, "RGN", 3) == 0
		   && strncasecmp(name + len - 6, "0x", 2) == 0) {

		fprintf(file, "[%.5s]\nType=%.6s\nLabel=", name + len - 12, name + len - 6);
		fwrite(name, sizeof(char), len - 13, file);
		fprintf(file, "\n");

		/* Added by oddgeir@oddgeirkvien.com, 2005.02.02 */
		if (layers) {
			fprintf(file, "%s\n", layers);
		}

		free(name);

		return len - 13;
	} else {
		free(name);
		return 0;
	}
}




SaveStatus GPSMapper::write_waypoints_to_file(FILE * file, const std::list<Waypoint *> & waypoints)
{
	for (auto iter = waypoints.begin(); iter != waypoints.end(); iter++) {
		Waypoint * wp = *iter;
		unsigned int len = print_rgn_stuff(file, wp->comment.toUtf8().constData());
		if (len) {
			fprintf(file, "Data0=(%s)\n", wp->coord.get_latlon().to_string().toUtf8().constData()); /* "Data0=(lat,lon)\n" */
			fprintf(file, "[END-%.5s]\n\n", wp->comment.toUtf8().constData() + len + 1);
		}
	}

	return SaveStatus::Code::Success;
}




static void write_trackpoint(FILE * file, Trackpoint * tp)
{
	fprintf(file, "(%s),", tp->coord.get_latlon().to_string().toUtf8().constData()); /* "(%s,%s)," */
}




SaveStatus GPSMapper::write_tracks_to_file(FILE * file, const std::list<Track *> & tracks)
{
	for (auto tracks_iter = tracks.begin(); tracks_iter != tracks.end(); tracks_iter++) {
		Track * trk = *tracks_iter;

		unsigned int len = print_rgn_stuff(file, trk->comment.toUtf8().constData());
		if (len) {
			fprintf(file, "Data0=");
			for (auto tp_iter = trk->trackpoints.begin(); tp_iter != trk->trackpoints.end(); tp_iter++) {
				write_trackpoint(file, *tp_iter);
			}
			fprintf(file, "\n[END-%.5s]\n\n", trk->comment.toUtf8().constData() + len + 1);
		}
	}

	return SaveStatus::Code::Success;
}




SaveStatus GPSMapper::write_layer_to_file(FILE * file, LayerTRW * trw)
{
	const QString line = QString("[IMG ID]\n"
				     "ID=%1\n"
				     "Name=%2\n"
				     "TreSize=1000\n"
				     "RgnLimit=700\n"
				     "Levels=2\n"
				     "Level0=22\n"
				     "Level1=18\n"
				     "Zoom0=0\n"
				     "Zoom1=1\n"
				     "[END-IMG ID]\n\n").arg(trw->name).arg(trw->name);
	fprintf(file, "%s", line.toUtf8().constData());

	SaveStatus save_status;

	save_status = GPSMapper::write_waypoints_to_file(file, trw->get_waypoints());
	if (SaveStatus::Code::Success != save_status) {
		qDebug() << SG_PREFIX_E << "Failed to write waypoints" << save_status;
		return save_status;
	}

	save_status = GPSMapper::write_tracks_to_file(file, trw->get_tracks());
	if (SaveStatus::Code::Success != save_status) {
		qDebug() << SG_PREFIX_E << "Failed to write tracks" << save_status;
		return save_status;
	}

	return SaveStatus::Code::Success;
}
