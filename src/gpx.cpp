/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2008, Hein Ragas <viking@ragas.nl>
 * Copyright (C) 2009, Tal B <tal.bav@gmail.com>
 * Copyright (c) 2012-2014, Rob Norris <rw_norris@hotmail.com>
 *
 * Some of the code adapted from GPSBabel 1.2.7
 * http://gpsbabel.sf.net/
 * Copyright (C) 2002, 2003, 2004, 2005 Robert Lipe, robertlipe@usa.net
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _XOPEN_SOURCE /* glibc2 needs this */

#include <cstdlib>
#include <cassert>

#ifdef HAVE_STRING_H
#include <cstring>
#endif

#ifdef HAVE_MATH_H
#include <cmath>
#endif
#include <time.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "gpx.h"
#include "globals.h"
#include "preferences.h"
#include <expat.h>

#include "util.h"




using namespace SlavGPS;




typedef enum {
        tt_unknown = 0,

        tt_gpx,
        tt_gpx_name,
        tt_gpx_desc,
        tt_gpx_author,
        tt_gpx_time,
        tt_gpx_keywords,

        tt_wpt,
        tt_wpt_cmt,
        tt_wpt_desc,
        tt_wpt_src,
        tt_wpt_type,
        tt_wpt_name,
        tt_wpt_ele,
        tt_wpt_sym,
        tt_wpt_time,
        tt_wpt_url,
        tt_wpt_link,            /* New in GPX 1.1 */

        tt_trk,
        tt_trk_cmt,
        tt_trk_desc,
        tt_trk_src,
        tt_trk_type,
        tt_trk_name,

        tt_rte,

        tt_trk_trkseg,
        tt_trk_trkseg_trkpt,
        tt_trk_trkseg_trkpt_ele,
        tt_trk_trkseg_trkpt_time,
        tt_trk_trkseg_trkpt_name,
        /* extended */
        tt_trk_trkseg_trkpt_course,
        tt_trk_trkseg_trkpt_speed,
        tt_trk_trkseg_trkpt_fix,
        tt_trk_trkseg_trkpt_sat,

        tt_trk_trkseg_trkpt_hdop,
        tt_trk_trkseg_trkpt_vdop,
        tt_trk_trkseg_trkpt_pdop,

        tt_waypoint,
        tt_waypoint_coord,
        tt_waypoint_name,
} tag_type_t;

typedef struct tag_mapping {
        tag_type_t tag_type;              /* Enum from above for this tag. */
        const char *tag_name;             /* xpath-ish tag name. */
} tag_mapping;

typedef struct {
	GpxWritingOptions *options;
	FILE *file;
} GpxWritingContext;




/*
 * xpath(ish) mappings between full tag paths and internal identifiers.
 * These appear in the order they appear in the GPX specification.
 * If it's not a tag we explicitly handle, it doesn't go here.
 */
tag_mapping tag_path_map[] = {

        { tt_gpx,          "/gpx"          },
        { tt_gpx_name,     "/gpx/name"     },
        { tt_gpx_desc,     "/gpx/desc"     },
        { tt_gpx_time,     "/gpx/time"     },
        { tt_gpx_author,   "/gpx/author"   },
        { tt_gpx_keywords, "/gpx/keywords" },

        /* GPX 1.1 variant - basic properties moved into metadata namespace. */
        { tt_gpx_name,     "/gpx/metadata/name"     },
        { tt_gpx_desc,     "/gpx/metadata/desc"     },
        { tt_gpx_time,     "/gpx/metadata/time"     },
        { tt_gpx_author,   "/gpx/metadata/author"   },
        { tt_gpx_keywords, "/gpx/metadata/keywords" },

        { tt_wpt, "/gpx/wpt" },

        { tt_waypoint,        "/loc/waypoint"      },
        { tt_waypoint_coord, "/loc/waypoint/coord" },
        { tt_waypoint_name,  "/loc/waypoint/name"  },

        { tt_wpt_ele,  "/gpx/wpt/ele"       },
        { tt_wpt_time, "/gpx/wpt/time"      },
        { tt_wpt_name, "/gpx/wpt/name"      },
        { tt_wpt_cmt,  "/gpx/wpt/cmt"       },
        { tt_wpt_desc, "/gpx/wpt/desc"      },
        { tt_wpt_src,  "/gpx/wpt/src"       },
        { tt_wpt_type, "/gpx/wpt/type"      },
        { tt_wpt_sym,  "/gpx/wpt/sym"       },
        { tt_wpt_sym,  "/loc/waypoint/type" },
        { tt_wpt_url,  "/gpx/wpt/url"       },
        { tt_wpt_link, "/gpx/wpt/link"      },                    /* GPX 1.1 */

        { tt_trk,                   "/gpx/trk"                   },
        { tt_trk_name,              "/gpx/trk/name"              },
        { tt_trk_cmt,               "/gpx/trk/cmt"               },
        { tt_trk_desc,              "/gpx/trk/desc"              },
        { tt_trk_src,               "/gpx/trk/src"               },
        { tt_trk_type,              "/gpx/trk/type"              },
        { tt_trk_trkseg,            "/gpx/trk/trkseg"            },
        { tt_trk_trkseg_trkpt,      "/gpx/trk/trkseg/trkpt"      },
        { tt_trk_trkseg_trkpt_ele,  "/gpx/trk/trkseg/trkpt/ele"  },
        { tt_trk_trkseg_trkpt_time, "/gpx/trk/trkseg/trkpt/time" },
        { tt_trk_trkseg_trkpt_name, "/gpx/trk/trkseg/trkpt/name" },
        /* Extended. */
        { tt_trk_trkseg_trkpt_course, "/gpx/trk/trkseg/trkpt/course" },
        { tt_trk_trkseg_trkpt_speed,  "/gpx/trk/trkseg/trkpt/speed"  },
        { tt_trk_trkseg_trkpt_fix,    "/gpx/trk/trkseg/trkpt/fix"    },
        { tt_trk_trkseg_trkpt_sat,    "/gpx/trk/trkseg/trkpt/sat"    },

        { tt_trk_trkseg_trkpt_hdop, "/gpx/trk/trkseg/trkpt/hdop" },
        { tt_trk_trkseg_trkpt_vdop, "/gpx/trk/trkseg/trkpt/vdop" },
        { tt_trk_trkseg_trkpt_pdop, "/gpx/trk/trkseg/trkpt/pdop" },

        { tt_rte,                   "/gpx/rte" },
        /* NB Route reuses track point feature tags. */
        { tt_trk_name,              "/gpx/rte/name"       },
        { tt_trk_cmt,               "/gpx/rte/cmt"        },
        { tt_trk_desc,              "/gpx/rte/desc"       },
        { tt_trk_src,               "/gpx/rte/src"        },
        { tt_trk_trkseg_trkpt,      "/gpx/rte/rtept"      },
        { tt_trk_trkseg_trkpt_name, "/gpx/rte/rtept/name" },
        { tt_trk_trkseg_trkpt_ele,  "/gpx/rte/rtept/ele"  },

        {(tag_type_t) 0}
};




static tag_type_t get_tag(const char *t)
{
        for (tag_mapping * tm = tag_path_map; tm->tag_type != 0; tm++) {
                if (0 == strcmp(tm->tag_name, t)) {
                        return tm->tag_type;
		}
	}
        return tt_unknown;
}



/******************************************/



tag_type_t current_tag = tt_unknown;
GString * xpath = NULL;
GString * c_cdata = NULL;

/* Current ("c_") objects. */
Trackpoint * c_tp = NULL;
Waypoint * c_wp = NULL;
Track * c_tr = NULL;
TRWMetadata * c_md = NULL;

char * c_wp_name = NULL;
char * c_tr_name = NULL;

/* Temporary things so we don't have to create them lots of times. */
const char * c_slat, * c_slon;
struct LatLon c_ll;

/* Specialty flags / etc. */
bool f_tr_newseg;
unsigned int unnamed_waypoints = 0;
unsigned int unnamed_tracks = 0;
unsigned int unnamed_routes = 0;




static char const * get_attr(char const ** attr, char const * key)
{
	while (*attr) {
		if (strcmp(*attr,key) == 0) {
			return *(attr + 1);
		}
		attr += 2;
	}
	return NULL;
}




static bool set_c_ll(char const ** attr)
{
	if ((c_slat = get_attr(attr, "lat")) && (c_slon = get_attr(attr, "lon"))) {
		c_ll.lat = g_ascii_strtod(c_slat, NULL);
		c_ll.lon = g_ascii_strtod(c_slon, NULL);
		return true;
	}
	return false;
}




static void gpx_start(LayerTRW * trw, char const * el, char const * *attr)
{
	static const char *tmp;

	g_string_append_c(xpath, '/');
	g_string_append(xpath, el);
	current_tag = get_tag(xpath->str);

	switch (current_tag) {

	case tt_gpx:
		c_md = new TRWMetadata();
		break;

	case tt_wpt:
		if (set_c_ll(attr)) {
			c_wp = new Waypoint();
			c_wp->visible = true;
			if (get_attr(attr, "hidden")) {
				c_wp->visible = false;
			}

			vik_coord_load_from_latlon(&(c_wp->coord),
						   trw->get_coord_mode(),
						   &c_ll);
		}
		break;

	case tt_trk:
	case tt_rte:
		c_tr = new Track(current_tag == tt_rte);
		c_tr->set_defaults();
		c_tr->visible = true;
		if (get_attr(attr, "hidden"))
			c_tr->visible = false;
		break;

	case tt_trk_trkseg:
		f_tr_newseg = true;
		break;

	case tt_trk_trkseg_trkpt:
		if (set_c_ll(attr)) {
			c_tp = new Trackpoint();
			vik_coord_load_from_latlon(&(c_tp->coord), trw->get_coord_mode(), &c_ll);
			if (f_tr_newseg) {
				c_tp->newsegment = true;
				f_tr_newseg = false;
			}
			c_tr->trackpointsB->push_back(c_tp);
		}
		break;

	case tt_gpx_name:
	case tt_gpx_author:
	case tt_gpx_desc:
	case tt_gpx_keywords:
	case tt_gpx_time:
	case tt_trk_trkseg_trkpt_name:
	case tt_trk_trkseg_trkpt_ele:
	case tt_trk_trkseg_trkpt_time:
	case tt_wpt_cmt:
	case tt_wpt_desc:
	case tt_wpt_src:
	case tt_wpt_type:
	case tt_wpt_name:
	case tt_wpt_ele:
	case tt_wpt_time:
	case tt_wpt_url:
	case tt_wpt_link:
	case tt_trk_cmt:
	case tt_trk_desc:
	case tt_trk_src:
	case tt_trk_type:
	case tt_trk_name:
		g_string_erase(c_cdata, 0, -1); /* Clear the cdata buffer. */
		break;

	case tt_waypoint:
		c_wp = new Waypoint();
		c_wp->visible = true;
		break;

	case tt_waypoint_coord:
		if (set_c_ll(attr)) {
			vik_coord_load_from_latlon(&(c_wp->coord), trw->get_coord_mode(), &c_ll);
		}
		break;

	case tt_waypoint_name:
		if ((tmp = get_attr(attr, "id"))) {
			if (c_wp_name) {
				free(c_wp_name);
			}
			c_wp_name = g_strdup(tmp);
		}
		g_string_erase(c_cdata, 0, -1); /* Clear the cdata buffer for description. */
		break;

	default: break;
	}
}




static void gpx_end(LayerTRW * trw, char const * el)
{
	static GTimeVal tp_time;
	static GTimeVal wp_time;

	g_string_truncate(xpath, xpath->len - strlen(el) - 1);

	switch (current_tag) {

	case tt_gpx:
		trw->set_metadata(c_md);
		c_md = NULL;
		break;

	case tt_gpx_name:
		trw->rename(c_cdata->str);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_gpx_author:
		c_md->set_author(c_cdata->str);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_gpx_desc:
		c_md->set_description(c_cdata->str);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_gpx_keywords:
		c_md->set_keywords(c_cdata->str);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_gpx_time:
		c_md->set_timestamp(c_cdata->str);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_waypoint:
	case tt_wpt:
		if (!c_wp_name) {
			c_wp_name = g_strdup_printf("VIKING_WP%04d", unnamed_waypoints++);
		}
		trw->filein_add_waypoint(c_wp, c_wp_name);
		free(c_wp_name);
		c_wp = NULL;
		c_wp_name = NULL;
		break;

	case tt_trk:
		if (!c_tr_name) {
			c_tr_name = g_strdup_printf("VIKING_TR%03d", unnamed_tracks++);
		}
		/* Delibrate fall through. */
	case tt_rte:
		if (!c_tr_name) {
			c_tr_name = g_strdup_printf("VIKING_RT%03d", unnamed_routes++);
		}
		trw->filein_add_track(c_tr, c_tr_name);
		free(c_tr_name);
		c_tr = NULL;
		c_tr_name = NULL;
		break;

	case tt_wpt_name:
		if (c_wp_name) {
			free(c_wp_name);
		}
		c_wp_name = g_strdup(c_cdata->str);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_trk_name:
		if (c_tr_name) {
			free(c_tr_name);
		}
		c_tr_name = g_strdup(c_cdata->str);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_wpt_ele:
		c_wp->altitude = g_ascii_strtod(c_cdata->str, NULL);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_trk_trkseg_trkpt_ele:
		c_tp->altitude = g_ascii_strtod(c_cdata->str, NULL);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_waypoint_name: /* .loc name is really description. */
	case tt_wpt_desc:
		c_wp->set_description(c_cdata->str);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_wpt_cmt:
		c_wp->set_comment(c_cdata->str);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_wpt_src:
		c_wp->set_source(c_cdata->str);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_wpt_type:
		c_wp->set_type(c_cdata->str);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_wpt_url:
		c_wp->set_url(c_cdata->str);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_wpt_link:
		c_wp->set_image(c_cdata->str);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_wpt_sym: {
		c_wp->set_symbol(c_cdata->str);
		g_string_erase(c_cdata, 0, -1);
		break;
	}

	case tt_trk_desc:
		c_tr->set_description(c_cdata->str);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_trk_src:
		c_tr->set_source(c_cdata->str);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_trk_type:
		c_tr->set_type(c_cdata->str);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_trk_cmt:
		c_tr->set_comment(c_cdata->str);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_wpt_time:
		if (g_time_val_from_iso8601(c_cdata->str, &wp_time)) {
			c_wp->timestamp = wp_time.tv_sec;
			c_wp->has_timestamp = true;
		}
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_trk_trkseg_trkpt_name:
		c_tp->set_name(c_cdata->str);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_trk_trkseg_trkpt_time:
		if (g_time_val_from_iso8601(c_cdata->str, &tp_time)) {
			c_tp->timestamp = tp_time.tv_sec;
			c_tp->has_timestamp = true;
		}
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_trk_trkseg_trkpt_course:
		c_tp->course = g_ascii_strtod(c_cdata->str, NULL);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_trk_trkseg_trkpt_speed:
		c_tp->speed = g_ascii_strtod(c_cdata->str, NULL);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_trk_trkseg_trkpt_fix:
		if (!strcmp("2d", c_cdata->str)) {
			c_tp->fix_mode = VIK_GPS_MODE_2D;
		} else if (!strcmp("3d", c_cdata->str)) {
			c_tp->fix_mode = VIK_GPS_MODE_3D;
		} else if (!strcmp("dgps", c_cdata->str)) {
			c_tp->fix_mode = VIK_GPS_MODE_DGPS;
		} else if (!strcmp("pps", c_cdata->str)) {
			c_tp->fix_mode = VIK_GPS_MODE_PPS;
		} else {
			c_tp->fix_mode = VIK_GPS_MODE_NOT_SEEN;
		}
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_trk_trkseg_trkpt_sat:
		c_tp->nsats = atoi(c_cdata->str);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_trk_trkseg_trkpt_hdop:
		c_tp->hdop = g_strtod(c_cdata->str, NULL);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_trk_trkseg_trkpt_vdop:
		c_tp->vdop = g_strtod(c_cdata->str, NULL);
		g_string_erase(c_cdata, 0, -1);
		break;

	case tt_trk_trkseg_trkpt_pdop:
		c_tp->pdop = g_strtod(c_cdata->str, NULL);
		g_string_erase(c_cdata, 0, -1);
		break;

	default: break;
	}

	current_tag = get_tag(xpath->str);
}




static void gpx_cdata(void * dta, const XML_Char * s, int len)
{
	switch (current_tag) {
	case tt_gpx_name:
	case tt_gpx_author:
	case tt_gpx_desc:
	case tt_gpx_keywords:
	case tt_gpx_time:
	case tt_wpt_name:
	case tt_trk_name:
	case tt_wpt_ele:
	case tt_trk_trkseg_trkpt_ele:
	case tt_wpt_cmt:
	case tt_wpt_desc:
	case tt_wpt_src:
	case tt_wpt_type:
	case tt_wpt_sym:
	case tt_wpt_url:
	case tt_wpt_link:
	case tt_trk_cmt:
	case tt_trk_desc:
	case tt_trk_src:
	case tt_trk_type:
	case tt_trk_trkseg_trkpt_time:
	case tt_wpt_time:
	case tt_trk_trkseg_trkpt_name:
	case tt_trk_trkseg_trkpt_course:
	case tt_trk_trkseg_trkpt_speed:
	case tt_trk_trkseg_trkpt_fix:
	case tt_trk_trkseg_trkpt_sat:
	case tt_trk_trkseg_trkpt_hdop:
	case tt_trk_trkseg_trkpt_vdop:
	case tt_trk_trkseg_trkpt_pdop:
	case tt_waypoint_name: /* .loc name is really description. */
		g_string_append_len(c_cdata, s, len);
		break;

	default: break;  /* Ignore cdata from other things. */
	}
}




/* Make like a "stack" of tag names like gpspoint's separated like /gpx/wpt/whatever. */

bool SlavGPS::a_gpx_read_file(LayerTRW * trw, FILE * f)
{
	assert (f != NULL && trw != NULL);

	XML_Parser parser = XML_ParserCreate(NULL);
	int done = 0;
	int len;
	enum XML_Status status = XML_STATUS_ERROR;

	XML_SetElementHandler(parser, (XML_StartElementHandler) gpx_start, (XML_EndElementHandler) gpx_end);
	XML_SetUserData(parser, trw); /* In the future we could remove all global variables. */
	XML_SetCharacterDataHandler(parser, (XML_CharacterDataHandler) gpx_cdata);

	char buf[4096];

	xpath = g_string_new("");
	c_cdata = g_string_new("");

	unnamed_waypoints = 1;
	unnamed_tracks = 1;
	unnamed_routes = 1;

	while (!done) {
		len = fread(buf, 1, sizeof(buf)-7, f);
		done = feof(f) || !len;
		status = XML_Parse(parser, buf, len, done);
	}

	XML_ParserFree(parser);
	g_string_free(xpath, true);
	g_string_free(c_cdata, true);

	return status != XML_STATUS_ERROR;
}



/**** Entitize from GPSBabel ****/
typedef struct {
        const char * text;
        const char * entity;
        int  not_html;
} entity_types;

static entity_types stdentities[] = {
        { "&",  "&amp;",  0 },
        { "'",  "&apos;", 1 },
        { "<",  "&lt;",   0 },
        { ">",  "&gt;",   0 },
        { "\"", "&quot;", 0 },
        { NULL, NULL,     0 }
};




void utf8_to_int(const char *cp, int *bytes, int *value)
{
        if ((*cp & 0xe0) == 0xc0) {
                if ((*(cp+1) & 0xc0) != 0x80) goto dodefault;
                *bytes = 2;
                *value = ((*cp & 0x1f) << 6) |
                        (*(cp+1) & 0x3f);

        } else if ((*cp & 0xf0) == 0xe0) {
                if ((*(cp+1) & 0xc0) != 0x80) goto dodefault;
                if ((*(cp+2) & 0xc0) != 0x80) goto dodefault;
                *bytes = 3;
                *value = ((*cp & 0x0f) << 12) |
                        ((*(cp+1) & 0x3f) << 6) |
                        (*(cp+2) & 0x3f);

        } else if ((*cp & 0xf8) == 0xf0) {
                if ((*(cp+1) & 0xc0) != 0x80) goto dodefault;
                if ((*(cp+2) & 0xc0) != 0x80) goto dodefault;
                if ((*(cp+3) & 0xc0) != 0x80) goto dodefault;
                *bytes = 4;
                *value = ((*cp & 0x07) << 18) |
                        ((*(cp+1) & 0x3f) << 12) |
                        ((*(cp+2) & 0x3f) << 6) |
                        (*(cp+3) & 0x3f);

        } else if ((*cp & 0xfc) == 0xf8) {
                if ((*(cp+1) & 0xc0) != 0x80) goto dodefault;
                if ((*(cp+2) & 0xc0) != 0x80) goto dodefault;
                if ((*(cp+3) & 0xc0) != 0x80) goto dodefault;
                if ((*(cp+4) & 0xc0) != 0x80) goto dodefault;
                *bytes = 5;
                *value = ((*cp & 0x03) << 24) |
                        ((*(cp+1) & 0x3f) << 18) |
                        ((*(cp+2) & 0x3f) << 12) |
                        ((*(cp+3) & 0x3f) << 6) |
                        (*(cp+4) & 0x3f);

        } else if ((*cp & 0xfe) == 0xfc) {
                if ((*(cp+1) & 0xc0) != 0x80) goto dodefault;
                if ((*(cp+2) & 0xc0) != 0x80) goto dodefault;
                if ((*(cp+3) & 0xc0) != 0x80) goto dodefault;
                if ((*(cp+4) & 0xc0) != 0x80) goto dodefault;
                if ((*(cp+5) & 0xc0) != 0x80) goto dodefault;
                *bytes = 6;
                *value = ((*cp & 0x01) << 30) |
                        ((*(cp+1) & 0x3f) << 24) |
                        ((*(cp+2) & 0x3f) << 18) |
                        ((*(cp+3) & 0x3f) << 12) |
                        ((*(cp+4) & 0x3f) << 6) |
                        (*(cp+5) & 0x3f);

        } else {
dodefault:
                *bytes = 1;
                *value = (unsigned char) *cp;
        }
}




static char * entitize(char const * str)
{
        char const * cp;
        char * p, * xstr;

        char tmpsub[20];
        int bytes = 0;
        int value = 0;
        entity_types * ep = stdentities;
        int elen = 0;
	int ecount = 0;
	int nsecount = 0;

        /* Figure # of entity replacements and additional size. */
        while (ep->text) {
                cp = str;
                while ((cp = strstr(cp, ep->text)) != NULL) {
                        elen += strlen(ep->entity) - strlen(ep->text);
                        ecount++;
                        cp += strlen(ep->text);
                }
                ep++;
        }

        /* figure the same for other than standard entities (i.e. anything
         * that isn't in the range U+0000 to U+007F */
        for (cp = str; *cp; cp++) {
                if (*cp & 0x80) {

                        utf8_to_int(cp, &bytes, &value);
                        cp += bytes-1;
                        elen += sprintf(tmpsub, "&#x%x;", value) - bytes;
                        nsecount++;
                }
        }

        /* enough space for the whole string plus entity replacements, if any */
        char * tmp = (char *) malloc((strlen(str) + elen + 1));
        strcpy(tmp, str);

        /* no entity replacements */
        if (ecount == 0 && nsecount == 0) {
                return (tmp);
	}

        if (ecount != 0) {
                for (ep = stdentities; ep->text; ep++) {
                        p = tmp;
                        while ((p = strstr(p, ep->text)) != NULL) {
                                elen = strlen(ep->entity);

                                xstr = g_strdup(p + strlen(ep->text));

                                strcpy(p, ep->entity);
                                strcpy(p + elen, xstr);

                                free(xstr);

                                p += elen;
                        }
                }
        }

        if (nsecount != 0) {
                p = tmp;
                while (*p) {
                        if (*p & 0x80) {
                                utf8_to_int(p, &bytes, &value);
                                if (p[bytes]) {
                                        xstr = g_strdup(p + bytes);
                                } else {
                                        xstr = NULL;
                                }
                                sprintf(p, "&#x%x;", value);
                                p = p+strlen(p);
                                if (xstr) {
                                        strcpy(p, xstr);
                                        free(xstr);
                                }
                        } else {
                                p++;
                        }
                }
        }
        return (tmp);
}
/**** End GPSBabel code. ****/




/* Export GPX. */

static void gpx_write_waypoint(Waypoint * wp, GpxWritingContext * context)
{
	/* Don't write invisible waypoints when specified. */
	if (context->options && !context->options->hidden && !wp->visible) {
		return;
	}

	FILE *f = context->file;
	static struct LatLon ll;
	char *s_lat,*s_lon;
	char *tmp;
	vik_coord_to_latlon(&(wp->coord), &ll);
	s_lat = a_coords_dtostr(ll.lat);
	s_lon = a_coords_dtostr(ll.lon);
	/* NB 'hidden' is not part of any GPX standard - this appears to be a made up Viking 'extension'.
	   Luckily most other GPX processing software ignores things they don't understand. */
	fprintf(f, "<wpt lat=\"%s\" lon=\"%s\"%s>\n",
		 s_lat, s_lon, wp->visible ? "" : " hidden=\"hidden\"");
	free(s_lat);
	free(s_lon);

	/* Sanity clause. */
	if (wp->name) {
		tmp = entitize(wp->name);
	} else {
		tmp = strdup("waypoint");
	}

	fprintf(f, "  <name>%s</name>\n", tmp);
	free(tmp);

	if (wp->altitude != VIK_DEFAULT_ALTITUDE) {
		tmp = a_coords_dtostr(wp->altitude);
		fprintf(f, "  <ele>%s</ele>\n", tmp);
		free(tmp);
	}

	if (wp->has_timestamp) {
		GTimeVal timestamp;
		timestamp.tv_sec = wp->timestamp;
		timestamp.tv_usec = 0;

		char * time_iso8601 = g_time_val_to_iso8601(&timestamp);
		if (time_iso8601 != NULL) {
			fprintf(f, "  <time>%s</time>\n", time_iso8601);
		}
		free(time_iso8601);
	}

	if (wp->comment) {
		tmp = entitize(wp->comment);
		fprintf(f, "  <cmt>%s</cmt>\n", tmp);
		free(tmp);
	}
	if (wp->description) {
		tmp = entitize(wp->description);
		fprintf(f, "  <desc>%s</desc>\n", tmp);
		free(tmp);
	}
	if (wp->source) {
		tmp = entitize(wp->source);
		fprintf(f, "  <src>%s</src>\n", tmp);
		free(tmp);
	}
	if (wp->type) {
		tmp = entitize(wp->type);
		fprintf(f, "  <type>%s</type>\n", tmp);
		free(tmp);
	}
	if (wp->url) {
		tmp = entitize(wp->url);
		fprintf(f, "  <url>%s</url>\n", tmp);
		free(tmp);
	}
	if (wp->image) {
		tmp = entitize(wp->image);
		fprintf(f, "  <link>%s</link>\n", tmp);
		free(tmp);
	}
	if (wp->symbol) {
		tmp = entitize(wp->symbol);
		if (Preferences::get_gpx_export_wpt_sym_name()) {
			/* Lowercase the symbol name. */
			char * tmp2 = g_utf8_strdown(tmp, -1);
			fprintf(f, "  <sym>%s</sym>\n",  tmp2);
			free(tmp2);
		} else {
			fprintf(f, "  <sym>%s</sym>\n", tmp);
		}
		free(tmp);
	}

	fprintf(f, "</wpt>\n");
}




static void gpx_write_trackpoint(Trackpoint * tp, GpxWritingContext * context)
{
	FILE * f = context->file;
	static struct LatLon ll;
	char *s_lat,*s_lon, *s_alt, *s_dop;
	char *time_iso8601;
	vik_coord_to_latlon(&(tp->coord), &ll);

	/* No such thing as a rteseg! So make sure we don't put them in. */
	if (context->options && !context->options->is_route && tp->newsegment) {
		fprintf(f, "  </trkseg>\n  <trkseg>\n");
	}

	s_lat = a_coords_dtostr(ll.lat);
	s_lon = a_coords_dtostr(ll.lon);
	fprintf(f, "  <%spt lat=\"%s\" lon=\"%s\">\n", (context->options && context->options->is_route) ? "rte" : "trk", s_lat, s_lon);
	free(s_lat); s_lat = NULL;
	free(s_lon); s_lon = NULL;

	if (tp->name) {
		char *s_name = entitize(tp->name);
		fprintf(f, "    <name>%s</name>\n", s_name);
		free(s_name);
	}

	s_alt = NULL;
	if (tp->altitude != VIK_DEFAULT_ALTITUDE) {
		s_alt = a_coords_dtostr(tp->altitude);
	} else if (context->options != NULL && context->options->force_ele) {
		s_alt = a_coords_dtostr(0);
	}
	if (s_alt != NULL) {
		fprintf(f, "    <ele>%s</ele>\n", s_alt);
	}
	free(s_alt); s_alt = NULL;

	time_iso8601 = NULL;
	if (tp->has_timestamp) {
		GTimeVal timestamp;
		timestamp.tv_sec = tp->timestamp;
		timestamp.tv_usec = 0;

		time_iso8601 = g_time_val_to_iso8601(&timestamp);
	} else if (context->options != NULL && context->options->force_time) {
		GTimeVal current;
		g_get_current_time(&current);

		time_iso8601 = g_time_val_to_iso8601(&current);
	}
	if (time_iso8601 != NULL) {
		fprintf(f, "    <time>%s</time>\n", time_iso8601);
	}
	free(time_iso8601);
	time_iso8601 = NULL;

	if (!isnan(tp->course)) {
		char *s_course = a_coords_dtostr(tp->course);
		fprintf(f, "    <course>%s</course>\n", s_course);
		free(s_course);
	}
	if (!isnan(tp->speed)) {
		char *s_speed = a_coords_dtostr(tp->speed);
		fprintf(f, "    <speed>%s</speed>\n", s_speed);
		free(s_speed);
	}
	if (tp->fix_mode == VIK_GPS_MODE_2D) {
		fprintf(f, "    <fix>2d</fix>\n");
	}
	if (tp->fix_mode == VIK_GPS_MODE_3D) {
		fprintf(f, "    <fix>3d</fix>\n");
	}
	if (tp->fix_mode == VIK_GPS_MODE_DGPS) {
		fprintf(f, "    <fix>dgps</fix>\n");
	}
	if (tp->fix_mode == VIK_GPS_MODE_PPS) {
		fprintf(f, "    <fix>pps</fix>\n");
	}
	if (tp->nsats > 0) {
		fprintf(f, "    <sat>%d</sat>\n", tp->nsats);
	}

	s_dop = NULL;
	if (tp->hdop != VIK_DEFAULT_DOP) {
		s_dop = a_coords_dtostr(tp->hdop);
	}
	if (s_dop != NULL) {
		fprintf(f, "    <hdop>%s</hdop>\n", s_dop);
	}
	free(s_dop); s_dop = NULL;

	if (tp->vdop != VIK_DEFAULT_DOP) {
		s_dop = a_coords_dtostr(tp->vdop);
	}
	if (s_dop != NULL) {
		fprintf(f, "    <vdop>%s</vdop>\n", s_dop);
	}
	free(s_dop);
	s_dop = NULL;

	if (tp->pdop != VIK_DEFAULT_DOP) {
		s_dop = a_coords_dtostr(tp->pdop);
	}
	if (s_dop != NULL) {
		fprintf(f, "    <pdop>%s</pdop>\n", s_dop);
	}
	free(s_dop); s_dop = NULL;

	fprintf(f, "  </%spt>\n", (context->options && context->options->is_route) ? "rte" : "trk");
}




static void gpx_write_track(Track * trk, GpxWritingContext * context)
{
	/* Don't write invisible tracks when specified. */
	if (context->options && !context->options->hidden && !trk->visible) {
		return;
	}

	FILE * f = context->file;
	char * tmp;

	/* Sanity clause. */
	if (trk->name) {
		tmp = entitize(trk->name);
	} else {
		tmp = strdup("track");
	}

	/* NB 'hidden' is not part of any GPX standard - this appears to be a made up Viking 'extension'.
	   Luckily most other GPX processing software ignores things they don't understand. */
	fprintf(f, "<%s%s>\n  <name>%s</name>\n",
		trk->sublayer_type == SublayerType::ROUTE ? "rte" : "trk",
		trk->visible ? "" : " hidden=\"hidden\"",
		tmp);
	free(tmp);

	if (trk->comment) {
		tmp = entitize(trk->comment);
		fprintf(f, "  <cmt>%s</cmt>\n", tmp);
		free(tmp);
	}

	if (trk->description) {
		tmp = entitize(trk->description);
		fprintf(f, "  <desc>%s</desc>\n", tmp);
		free(tmp);
	}

	if (trk->source) {
		tmp = entitize(trk->source);
		fprintf(f, "  <src>%s</src>\n", tmp);
		free(tmp);
	}

	if (trk->type) {
		tmp = entitize(trk->type);
		fprintf(f, "  <type>%s</type>\n", tmp);
		free(tmp);
	}

	/* No such thing as a rteseg! */
	if (trk->sublayer_type == SublayerType::TRACK) {
		fprintf(f, "  <trkseg>\n");
	}

	if (!trk->empty()) {

		auto first = trk->trackpointsB->begin();
		bool first_tp_is_newsegment = (*first)->newsegment;
		(*first)->newsegment = false; /* So we won't write </trkseg><trkseg> already. */

		for (auto iter = trk->trackpointsB->begin(); iter != trk->trackpointsB->end(); iter++) {
			gpx_write_trackpoint(*iter, context);
		}

		(*first)->newsegment = first_tp_is_newsegment; /* Restore state. */
	}

	/* NB apparently no such thing as a rteseg! */
	if (trk->sublayer_type == SublayerType::TRACK) {
		fprintf(f, "  </trkseg>\n");
	}

	fprintf(f, "</%s>\n", trk->sublayer_type == SublayerType::ROUTE ? "rte" : "trk");
}




static void gpx_write_header(FILE * f)
{
	fprintf(f, "<?xml version=\"1.0\"?>\n"
		"<gpx version=\"1.0\" creator=\"Viking -- http://viking.sf.net/\"\n"
		"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
		"xmlns=\"http://www.topografix.com/GPX/1/0\"\n"
		"xsi:schemaLocation=\"http://www.topografix.com/GPX/1/0 http://www.topografix.com/GPX/1/0/gpx.xsd\">\n");
}




static void gpx_write_footer(FILE * f)
{
	fprintf(f, "</gpx>\n");
}




static int gpx_waypoint_compare(const void * x, const void * y)
{
	Waypoint * a = (Waypoint *) x;
	Waypoint * b = (Waypoint *) y;
	return strcmp(a->name,b->name);
}




static int gpx_track_compare_name(const void * x, const void * y)
{
	Track * a = (Track *) x;
	Track * b = (Track *) y;
	return strcmp(a->name,b->name);
}




void SlavGPS::a_gpx_write_file(LayerTRW * trw, FILE * f, GpxWritingOptions * options)
{
	GpxWritingContext context = { options, f };

	gpx_write_header(f);

	char *tmp;
	const char *name = trw->get_name();
	if (name) {
		tmp = entitize(name);
		fprintf(f, "  <name>%s</name>\n", tmp);
		free(tmp);
	}

	TRWMetadata * md = trw->get_metadata();
	if (md) {
		if (md->author) {
			tmp = entitize(md->author);
			fprintf(f, "  <author>%s</author>\n", tmp);
			free(tmp);
		}
		if (md->description) {
			tmp = entitize(md->description);
			fprintf(f, "  <desc>%s</desc>\n", tmp);
			free(tmp);
		}
		if (md->timestamp) {
			tmp = entitize(md->timestamp);
			fprintf(f, "  <time>%s</time>\n", tmp);
			free(tmp);
		}
		if (md->keywords) {
			tmp = entitize(md->keywords);
			fprintf(f, "  <keywords>%s</keywords>\n", tmp);
			free(tmp);
		}
	}

	if (trw->get_waypoints_visibility() || (options && options->hidden)) {
		/* Gather waypoints in a list, then sort. */
		Waypoints & waypoints = trw->get_waypoints();
		int index = 0;
		GList * gl = NULL;
		for (auto iter = waypoints.begin(); iter != waypoints.end(); iter++) {
			gl = g_list_insert(gl, iter->second, index++);
		}

		gl = g_list_sort(gl, gpx_waypoint_compare);

		for (GList * iter = g_list_first(gl); iter != NULL; iter = g_list_next(iter)) {
			gpx_write_waypoint((Waypoint *) iter->data, &context);
		}
		g_list_free(gl);
	}

	GList * gl = NULL;
	if (trw->get_tracks_visibility() || (options && options->hidden)) {
		//gl = g_hash_table_get_values(vik_trw_layer_get_tracks (trw));
		/* Forming the list manually seems to produce one that is more likely to be nearer to the creation order. */

		Tracks & tracks = trw->get_tracks();
		for (auto i = tracks.begin(); i != tracks.end(); i++) {
			gl = g_list_prepend(gl, i->second);
		}
		gl = g_list_reverse(gl);

		/* Sort method determined by preference. */
		if (Preferences::get_gpx_export_trk_sort() == VIK_GPX_EXPORT_TRK_SORT_TIME) {
			gl = g_list_sort(gl, Track::compare_timestamp);
		} else if (Preferences::get_gpx_export_trk_sort() == VIK_GPX_EXPORT_TRK_SORT_ALPHA) {
			gl = g_list_sort(gl, gpx_track_compare_name);
		} else {
			;
		}
	}

	GList * glrte = NULL;
	/* Routes sorted by name. */
	if (trw->get_routes_visibility() || (options && options->hidden)) {

		Tracks & routes = trw->get_routes();
		int index = 0;
		for (auto i = routes.begin(); i != routes.end(); i++) {
			glrte = g_list_insert(glrte, i->second, index);
		}
		glrte = g_list_sort(glrte, gpx_track_compare_name);
	}

	/* g_list_concat doesn't copy memory properly so process each list separately. */

	GpxWritingContext context_tmp = context;
	GpxWritingOptions opt_tmp = { false, false, false, false };
	/* Force trackpoints on tracks. */
	if (!context.options) {
		context_tmp.options = &opt_tmp;
	}
	context_tmp.options->is_route = false;

	/* Loop around each list and write each one. */
	for (GList *iter = g_list_first(gl); iter != NULL; iter = g_list_next(iter)) {
		gpx_write_track((Track *) iter->data, &context_tmp);
	}

	/* Routes (to get routepoints). */
	context_tmp.options->is_route = true;
	for (GList * iter = g_list_first(glrte); iter != NULL; iter = g_list_next(iter)) {
		gpx_write_track((Track *) iter->data, &context_tmp);
	}

	g_list_free(gl);
	g_list_free(glrte);

	gpx_write_footer(f);
}




void SlavGPS::a_gpx_write_track_file(Track * trk, FILE * f, GpxWritingOptions * options)
{
	GpxWritingContext context = {options, f};
	gpx_write_header(f);
	gpx_write_track(trk, &context);
	gpx_write_footer(f);
}




/**
 * Common write of a temporary GPX file.
 */
static char * write_tmp_file(LayerTRW * trw, Track * trk, GpxWritingOptions * options)
{
	char * tmp_filename = NULL;
	GError * error = NULL;
	/* Opening temporary file. */
	int fd = g_file_open_tmp("viking_XXXXXX.gpx", &tmp_filename, &error);
	if (fd < 0) {
		fprintf(stderr, _("WARNING: failed to open temporary file: %s\n"), error->message);
		g_clear_error(&error);
		return NULL;
	}
	fprintf(stderr, "DEBUG: %s: temporary file = %s\n", __FUNCTION__, tmp_filename);

	FILE * ff = fdopen(fd, "w");

	if (trk) {
		a_gpx_write_track_file(trk, ff, options);
	} else {
		a_gpx_write_file(trw, ff, options);
	}

	fclose(ff);

	return tmp_filename;
}




/*
 * @trw:     The #LayerTRW to write
 * @options: Possible ways of writing the file data (can be NULL)
 *
 * Returns: The name of newly created temporary GPX file.
 *          This file should be removed once used and the string freed.
 *          If NULL then the process failed.
 */
char * SlavGPS::a_gpx_write_tmp_file(LayerTRW * trw, GpxWritingOptions * options)
{
	return write_tmp_file(trw, NULL, options);
}




/*
 * @trk:     The #Track to write
 * @options: Possible ways of writing the file data (can be NULL)
 *
 * Returns: The name of newly created temporary GPX file.
 *          This file should be removed once used and the string freed.
 *          If NULL then the process failed.
 */
char * SlavGPS::a_gpx_write_track_tmp_file(Track * trk, GpxWritingOptions * options)
{
	return write_tmp_file(NULL, trk, options);
}
