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
//#include "viking.h"
#include "gpsmapper.h"

#include <string.h>
#include <stdlib.h>

#include <algorithm>

/* Name of layer -> RGN type and Type
   format: Name RGN40 0x40
   or:     Name RGN10 0x2f06
*/
/* returns 0 if invalid/no rgn stuff, else returns len of */
static unsigned int print_rgn_stuff(const char * nm, FILE * f)
{
	unsigned int len;
	char *layers;
	char *name;

	if (!nm) {
		return 0;
	}

	name = g_strdup(nm);

	len = strlen(name);



	/* --------------------------------------------- */
	/* added by oddgeir@oddgeirkvien.com, 2005.02.02 */
	/* Format may also be: Name RGN40 0x40 Layers=1  */
	/* or: Name RGN10 0x2f06 Layers=1                */

	if (len > 20 && strncasecmp(name + len - 8, "LAYERS=", 7) == 0) { /* Layers is added to the description */
		layers=name + len - 8;
		*(name + len - 9) = 0;
		len = strlen(name);
	} else {
		layers = 0;
	}
	/* --------------------------------------------- */



	if (len > 11 && strncasecmp(name + len - 10, "RGN", 3) == 0 &&
	     strncasecmp(name + len - 4, "0x", 2) == 0) {

		fprintf(f, "[%.5s]\nType=%.4s\nLabel=", name + len - 10, name + len - 4);
		fwrite(name, sizeof(char), len - 11, f);
		fprintf(f, "\n");

		/* added by oddgeir@oddgeirkvien.com, 2005.02.02 */
		if (layers) {
			fprintf(f, "%s\n", layers);
		}

		free(name);

		return len - 11;

	} else if (len > 13 && strncasecmp(name + len - 12, "RGN", 3) == 0 &&
		    strncasecmp(name + len - 6, "0x", 2) == 0) {

		fprintf(f, "[%.5s]\nType=%.6s\nLabel=", name + len - 12, name + len - 6);
		fwrite(name, sizeof(char), len - 13, f);
		fprintf(f, "\n");

		/* added by oddgeir@oddgeirkvien.com, 2005.02.02 */
		if (layers) {
			fprintf(f, "%s\n",layers);
		}

		free(name);

		return len - 13;
	} else {
		free(name);
		return 0;
	}
}

static void write_waypoints(FILE * f, std::unordered_map<sg_uid_t, Waypoint *> & waypoints)
{
	static struct LatLon ll;

	for (auto i = waypoints.begin(); i != waypoints.end(); i++) {
		unsigned int len = print_rgn_stuff(i->second->comment, f);
		if (len) {
			vik_coord_to_latlon(&(i->second->coord), &ll);
			char * s_lat = a_coords_dtostr(ll.lat);
			char * s_lon = a_coords_dtostr(ll.lon);
			fprintf(f, "Data0=(%s,%s)\n", s_lat, s_lon);
			free(s_lat);
			free(s_lon);
			fprintf(f, "[END-%.5s]\n\n", i->second->comment+len+1);
		}
	}
}

static void write_trackpoint(Trackpoint * tp, FILE *f)
{
	static struct LatLon ll;
	char *s_lat, *s_lon;
	vik_coord_to_latlon(&(tp->coord), &ll);
	s_lat = a_coords_dtostr(ll.lat);
	s_lon = a_coords_dtostr(ll.lon);
	fprintf(f, "(%s,%s),", s_lat, s_lon);
	free(s_lat);
	free(s_lon);
}

static void write_track(FILE * f, std::unordered_map<sg_uid_t, Track *> & tracks)
{
	for (auto i = tracks.begin(); i != tracks.end(); i++) {
		unsigned int len = print_rgn_stuff(i->second->comment, f);
		if (len) {
			fprintf(f, "Data0=");
			g_list_foreach(i->second->trackpoints,(GFunc) write_trackpoint, f);
			fprintf(f, "\n[END-%.5s]\n\n", i->second->comment + len + 1);
		}
	}
}

void a_gpsmapper_write_file(FILE * f, LayerTRW * layer)
{
	std::unordered_map<sg_uid_t, Track *> & tracks = layer->get_tracks();
	auto waypoints = layer->get_waypoints();

	fprintf(f, "[IMG ID]\nID=%s\nName=%s\nTreSize=1000\nRgnLimit=700\nLevels=2\nLevel0=22\nLevel1=18\nZoom0=0\nZoom1=1\n[END-IMG ID]\n\n",
		layer->name, layer->name);

	write_waypoints(f, waypoints);
	write_track(f, tracks);
}
