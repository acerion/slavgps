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
 *
 */


#include <algorithm>
#include <cstring>
#include <cstdlib>

#include "gpsmapper.h"
#include "layer_trw.h"
#include "layer_trw_track_internal.h"




using namespace SlavGPS;




/*
  Name of layer -> RGN type and Type.
  format: Name RGN40 0x40
  or:     Name RGN10 0x2f06

  Returns 0 if invalid/no rgn stuff, else returns len of.
*/
static unsigned int print_rgn_stuff(char const * nm, FILE * f)
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

		fprintf(f, "[%.5s]\nType=%.4s\nLabel=", name + len - 10, name + len - 4);
		fwrite(name, sizeof(char), len - 11, f);
		fprintf(f, "\n");

		/* Added by oddgeir@oddgeirkvien.com, 2005.02.02 */
		if (layers) {
			fprintf(f, "%s\n", layers);
		}

		free(name);

		return len - 11;

	} else if (len > 13 && strncasecmp(name + len - 12, "RGN", 3) == 0
		   && strncasecmp(name + len - 6, "0x", 2) == 0) {

		fprintf(f, "[%.5s]\nType=%.6s\nLabel=", name + len - 12, name + len - 6);
		fwrite(name, sizeof(char), len - 13, f);
		fprintf(f, "\n");

		/* Added by oddgeir@oddgeirkvien.com, 2005.02.02 */
		if (layers) {
			fprintf(f, "%s\n", layers);
		}

		free(name);

		return len - 13;
	} else {
		free(name);
		return 0;
	}
}




static void write_waypoints(FILE * f, WaypointsContainer & waypoints)
{
	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		Waypoint * wp = i->second;
		unsigned int len = print_rgn_stuff(wp->comment.toUtf8().constData(), f);
		if (len) {
			fprintf(f, "Data0=(%s)\n", wp->coord.get_latlon().to_string().toUtf8().constData()); /* "Data0=(lat,lon)\n" */
			fprintf(f, "[END-%.5s]\n\n", wp->comment.toUtf8().constData() + len + 1);
		}
	}
}




static void write_trackpoint(Trackpoint * tp, FILE * f)
{
	fprintf(f, "(%s),", tp->coord.get_latlon().to_string().toUtf8().constData()); /* "(%s,%s)," */
}




static void write_track(FILE * f, TracksContainer & tracks)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		unsigned int len = print_rgn_stuff(i->second->comment.toUtf8().constData(), f);
		if (len) {
			fprintf(f, "Data0=");
			for (auto iter = i->second->trackpoints.begin(); iter != i->second->trackpoints.end(); iter++) {
				write_trackpoint(*iter, f);
			}
			fprintf(f, "\n[END-%.5s]\n\n", i->second->comment.toUtf8().constData() + len + 1);
		}
	}
}




void SlavGPS::gpsmapper_write_file(FILE * f, LayerTRW * trw)
{
	TracksContainer & tracks = trw->get_track_items();
	WaypointsContainer & waypoints = trw->get_waypoint_items();

	fprintf(f, "[IMG ID]\nID=%s\nName=%s\nTreSize=1000\nRgnLimit=700\nLevels=2\nLevel0=22\nLevel1=18\nZoom0=0\nZoom1=1\n[END-IMG ID]\n\n",
		trw->name.toUtf8().constData(), trw->name.toUtf8().constData());

	write_waypoints(f, waypoints);
	write_track(f, tracks);
}
