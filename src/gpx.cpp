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
 */




#define _XOPEN_SOURCE /* glibc2 needs this */




#include <cstdlib>
#include <cassert>
#include <cstring>
#include <vector>
#include <list>
#include <cmath>
#include <time.h>




#include <QDebug>




#include <glib.h>




#include "gpx.h"
#include "preferences.h"
#include "layer_trw.h"
#include "layer_trw_track_internal.h"
#include "util.h"




using namespace SlavGPS;




#define SG_MODULE "GPX"
#define GPX_BUFFER_SIZE 4096




typedef struct tag_mapping {
        tag_type_t tag_type;              /* Enum from above for this tag. */
        const char * tag_name;            /* xpath-ish tag name. */
} tag_mapping;




class GPXWriteContext {
public:
	GPXWriteContext(GPXWriteOptions * new_options, FILE * new_file) : options(new_options), file(new_file) {};
	GPXWriteOptions * options = NULL;
	FILE * file = NULL;
};




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




static tag_type_t get_tag_type(const QString & tag)
{
        for (tag_mapping * tm = tag_path_map; tm->tag_type != 0; tm++) {
                if (QString(tm->tag_name) == tag) {
                        return tm->tag_type;
		}
	}
        return tt_unknown;
}




static QString get_attr(char const ** attributes, char const * key)
{
	while (*attributes) {
		if (strcmp(*attributes, key) == 0) {
			return QString(*(attributes + 1));
		}
		attributes += 2;
	}
	return "";
}




bool GPXImporter::set_lat_lon(char const ** attributes)
{
	this->slat = get_attr(attributes, "lat");
	if (this->slat.isEmpty()) {
		return false;
	}

	this->slon = get_attr(attributes, "lon");
	if (this->slon.isEmpty()) {
		return false;
	}

	this->lat_lon.lat = SGUtils::c_to_double(this->slat);
	this->lat_lon.lon = SGUtils::c_to_double(this->slon);

	return true;
}




static void gpx_start(GPXImporter * importer, char const * el, char const ** attributes)
{
	/* Expand current xpath. */
	//fprintf(stderr, "++++++++ before append of '%s', xpath = '%s'\n", el, importer->xpath.toUtf8().constData());
	importer->xpath.append('/');
	importer->xpath.append(QString(el));
	importer->current_tag_type = get_tag_type(importer->xpath);
	//fprintf(stderr, "++++++++ after append, xpath = '%s'\n", importer->xpath.toUtf8().constData());


	switch (importer->current_tag_type) {
	case tt_gpx:
		importer->md = new TRWMetadata();
		break;

	case tt_wpt:
		if (importer->set_lat_lon(attributes)) {
			importer->wp = new Waypoint();
			importer->wp->visible = get_attr(attributes, "hidden").isEmpty();
			importer->wp->coord = Coord(importer->lat_lon, importer->trw->get_coord_mode());
		}
		break;

	case tt_trk:
	case tt_rte:
		importer->trk = new Track(importer->current_tag_type == tt_rte);
		importer->trk->set_defaults();
		importer->trk->visible = get_attr(attributes, "hidden").isEmpty();
		break;

	case tt_trk_trkseg:
		importer->f_tr_newseg = true;
		break;

	case tt_trk_trkseg_trkpt:
		if (importer->set_lat_lon(attributes)) {
			importer->tp = new Trackpoint();
			importer->tp->coord = Coord(importer->lat_lon, importer->trw->get_coord_mode());
			if (importer->f_tr_newseg) {
				importer->tp->newsegment = true;
				importer->f_tr_newseg = false;
			}
			importer->trk->trackpoints.push_back(importer->tp);
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
		importer->cdata.clear();
		break;

	case tt_waypoint:
		importer->wp = new Waypoint();
		importer->wp->visible = true;
		break;

	case tt_waypoint_coord:
		if (importer->set_lat_lon(attributes)) {
			importer->wp->coord = Coord(importer->lat_lon, importer->trw->get_coord_mode());
		}
		break;

	case tt_waypoint_name:
		{
			QString tmp = get_attr(attributes, "id");
			if (!tmp.isEmpty()) {
				importer->wp_name = tmp;
			}
			importer->cdata.clear();
		}
		break;

	default: break;
	}
}




static void gpx_end(GPXImporter * importer, char const * el)
{
	/* Truncate current xpath by removing last tag from it (+1 to remove slash too). */
	//fprintf(stderr, "-------- before chop of '%s' xpath ='%s'\n", el, importer->xpath.toUtf8().constData());
	importer->xpath.chop(strlen(el) + 1);
	//fprintf(stderr, "-------- after chop, xpath = '%s'\n", importer->xpath.toUtf8().constData());;


	switch (importer->current_tag_type) {
	case tt_gpx:
		importer->trw->set_metadata(importer->md);
		importer->md = NULL;
		break;

	case tt_gpx_name:
		importer->trw->set_name(importer->cdata);
		importer->cdata.clear();
		break;

	case tt_gpx_author:
		importer->md->set_author(importer->cdata);
		importer->cdata.clear();
		break;

	case tt_gpx_desc:
		importer->md->set_description(importer->cdata);
		importer->cdata.clear();
		break;

	case tt_gpx_keywords:
		importer->md->set_keywords(importer->cdata);
		importer->cdata.clear();
		break;

	case tt_gpx_time:
		importer->md->set_iso8601_timestamp(importer->cdata);
		importer->cdata.clear();
		break;

	case tt_waypoint:
	case tt_wpt:
		if (importer->wp_name.isEmpty()) {
			importer->wp_name = QString("VIKING_WP%1").arg(importer->unnamed_waypoints++, 4, 10, (QChar) '0');
		}
		importer->wp->set_name(importer->wp_name);
		importer->trw->add_waypoint_from_file(importer->wp);
		importer->wp = NULL;
		break;

	case tt_trk:
		if (importer->trk_name.isEmpty()) {
			importer->trk_name = QString("VIKING_TR%1").arg(importer->unnamed_tracks++, 3, 10, (QChar) '0');
		}
		/* Delibrate fall through. */
	case tt_rte:
		if (importer->trk_name.isEmpty()) {
			importer->trk_name = QString("VIKING_RT%1").arg(importer->unnamed_routes++, 3, 10, (QChar) '0');
		}
		importer->trk->set_name(importer->trk_name);
		importer->trw->add_track_from_file(importer->trk);
		importer->trk = NULL;
		break;

	case tt_wpt_name:
		importer->wp_name = importer->cdata;
		importer->cdata.clear();
		break;

	case tt_trk_name:
		importer->trk_name = importer->cdata;
		importer->cdata.clear();
		break;

	case tt_wpt_ele:
		importer->wp->altitude = Altitude(SGUtils::c_to_double(importer->cdata.toUtf8().constData()), HeightUnit::Metres);
		importer->cdata.clear();
		break;

	case tt_trk_trkseg_trkpt_ele:
		importer->tp->altitude = SGUtils::c_to_double(importer->cdata.toUtf8().constData());
		importer->cdata.clear();
		break;

	case tt_waypoint_name: /* .loc name is really description. */
	case tt_wpt_desc:
		importer->wp->set_description(importer->cdata);
		importer->cdata.clear();
		break;

	case tt_wpt_cmt:
		importer->wp->set_comment(importer->cdata);
		importer->cdata.clear();
		break;

	case tt_wpt_src:
		importer->wp->set_source(importer->cdata);
		importer->cdata.clear();
		break;

	case tt_wpt_type:
		importer->wp->set_type(importer->cdata);
		importer->cdata.clear();
		break;

	case tt_wpt_url:
		importer->wp->set_url(importer->cdata);
		importer->cdata.clear();
		break;

	case tt_wpt_link:
		importer->wp->set_image_full_path(importer->cdata);
		importer->cdata.clear();
		break;

	case tt_wpt_sym:
		importer->wp->set_symbol(importer->cdata);
		importer->cdata.clear();
		break;

	case tt_trk_desc:
		importer->trk->set_description(importer->cdata);
		importer->cdata.clear();
		break;

	case tt_trk_src:
		importer->trk->set_source(importer->cdata);
		importer->cdata.clear();
		break;

	case tt_trk_type:
		importer->trk->set_type(importer->cdata);
		importer->cdata.clear();
		break;

	case tt_trk_cmt:
		importer->trk->set_comment(importer->cdata);
		importer->cdata.clear();
		break;

	case tt_wpt_time:
		{
			GTimeVal wp_time;
			if (g_time_val_from_iso8601(importer->cdata.toUtf8().constData(), &wp_time)) {
				importer->wp->timestamp = wp_time.tv_sec;
				importer->wp->has_timestamp = true;
			}
			importer->cdata.clear();
		}
		break;

	case tt_trk_trkseg_trkpt_name:
		importer->tp->set_name(importer->cdata);
		importer->cdata.clear();
		break;

	case tt_trk_trkseg_trkpt_time:
		{
			GTimeVal tp_time;
			if (g_time_val_from_iso8601(importer->cdata.toUtf8().constData(), &tp_time)) {
				importer->tp->timestamp = tp_time.tv_sec;
				importer->tp->has_timestamp = true;
			}
			importer->cdata.clear();
		}
		break;

	case tt_trk_trkseg_trkpt_course:
		importer->tp->course = SGUtils::c_to_double(importer->cdata.toUtf8().constData());
		importer->cdata.clear();
		break;

	case tt_trk_trkseg_trkpt_speed:
		importer->tp->speed = SGUtils::c_to_double(importer->cdata.toUtf8().constData());
		importer->cdata.clear();
		break;

	case tt_trk_trkseg_trkpt_fix:
		if ("2d" == importer->cdata) {
			importer->tp->fix_mode = GPSFixMode::Fix2D;
		} else if ("3d" == importer->cdata) {
			importer->tp->fix_mode = GPSFixMode::Fix3D;
		} else if ("dgps" == importer->cdata) {
			importer->tp->fix_mode = GPSFixMode::DGPS;
		} else if ("pps" == importer->cdata) {
			importer->tp->fix_mode = GPSFixMode::PPS;
		} else {
			importer->tp->fix_mode = GPSFixMode::NotSeen;
		}
		importer->cdata.clear();
		break;

	case tt_trk_trkseg_trkpt_sat:
		importer->tp->nsats = importer->cdata.toInt();
		importer->cdata.clear();
		break;

	case tt_trk_trkseg_trkpt_hdop:
		importer->tp->hdop = strtod(importer->cdata.toUtf8().constData(), NULL);
		importer->cdata.clear();
		break;

	case tt_trk_trkseg_trkpt_vdop:
		importer->tp->vdop = strtod(importer->cdata.toUtf8().constData(), NULL);
		importer->cdata.clear();
		break;

	case tt_trk_trkseg_trkpt_pdop:
		importer->tp->pdop = strtod(importer->cdata.toUtf8().constData(), NULL);
		importer->cdata.clear();
		break;

	default: break;
	}

	importer->current_tag_type = get_tag_type(importer->xpath);
}




static void gpx_cdata(GPXImporter * importer, const XML_Char * s, int len)
{
	switch (importer->current_tag_type) {
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
		importer->cdata.append(QString(s).left(len));
		break;

	default:
		break;  /* Ignore cdata from other things. */
	}
}



sg_ret GPX::read_layer_from_file(QFile & file, LayerTRW * trw)
{
	GPXImporter importer(trw);
	char buf[GPX_BUFFER_SIZE];
	bool finish = false;

	while (!finish) {
		const size_t n_read = file.read(buf, sizeof (buf));
		finish = file.atEnd() || 0 == n_read;

		if (sg_ret::ok != importer.write(buf, n_read)) {
			qDebug() << SG_PREFIX_E << "Failed to write" << n_read << "bytes of data to GPX importer";
			return sg_ret::err;
		}
	}

	return sg_ret::ok;
}




#if 0 /* Keeping as reference. */
/* Make like a "stack" of tag names like gpspoint's separated like /gpx/wpt/whatever. */

sg_ret GPX::read_layer_from_file2(QFile & file, LayerTRW * trw)
{
	assert (trw != NULL);

	XML_Parser parser = XML_ParserCreate(NULL);
	int done = 0;
	int len;
	enum XML_Status status = XML_STATUS_ERROR;

	XML_SetElementHandler(parser, (XML_StartElementHandler) gpx_start, (XML_EndElementHandler) gpx_end);
	XML_SetUserData(parser, trw); /* In the future we could remove all global variables. */
	XML_SetCharacterDataHandler(parser, (XML_CharacterDataHandler) gpx_cdata);

	char buf[GPX_BUFFER_SIZE];

	importer->xpath = g_string_new("");
	importer->cdata = g_string_new("");

	importer->unnamed_waypoints = 1;
	importer->unnamed_tracks = 1;
	importer->unnamed_routes = 1;

	FILE * file2 = fdopen(file.handle(), "r"); /* OLD_TODO: close the file? */

	while (!done) {
		len = fread(buf, 1, sizeof(buf)-7, file2);
		done = feof(file2) || !len;
		status = XML_Parse(parser, buf, len, done);
	}

	XML_ParserFree(parser);
	g_string_free(importer->xpath, true);
	g_string_free(importer->cdata, true);

	return status != XML_STATUS_ERROR ? sg_ret::ok : sg_ret::err;
}
#endif






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




static QString entitize(const QString & input)
{
	char * str = strdup(input.toUtf8().constData());

	QString result;

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
		result = tmp;
		free(tmp);
                return result;
	}

        if (ecount != 0) {
                for (ep = stdentities; ep->text; ep++) {
                        p = tmp;
                        while ((p = strstr(p, ep->text)) != NULL) {
                                elen = strlen(ep->entity);

                                xstr = strdup(p + strlen(ep->text));

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
                                        xstr = strdup(p + bytes);
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
	result = tmp;
	free(tmp);
        return result;
}
/**** End GPSBabel code. ****/




/* Export GPX. */

static void gpx_write_waypoint(Waypoint * wp, GPXWriteContext * context)
{
	/* Don't write invisible waypoints when specified. */
	if (context->options && !context->options->hidden && !wp->visible) {
		return;
	}

	FILE * f = context->file;
	static LatLon lat_lon = wp->coord.get_latlon();
	/* NB 'hidden' is not part of any GPX standard - this appears to be a made up Viking 'extension'.
	   Luckily most other GPX processing software ignores things they don't understand. */
	fprintf(f, "<wpt lat=\"%s\" lon=\"%s\"%s>\n", SGUtils::double_to_c(lat_lon.lat).toUtf8().constData(), SGUtils::double_to_c(lat_lon.lon).toUtf8().constData(), wp->visible ? "" : " hidden=\"hidden\"");

	/* Sanity clause. */
	if (wp->name.isEmpty()) {
		fprintf(f, "  <name>%s</name>\n", "waypoint"); /* TODO_MAYBE: localize? */
	} else {
		fprintf(f, "  <name>%s</name>\n", entitize(wp->name).toUtf8().constData());
	}

	if (wp->altitude.is_valid()) {
		fprintf(f, "  <ele>%s</ele>\n", wp->altitude.value_to_string_for_file().toUtf8().constData());
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

	if (!wp->comment.isEmpty()) {
		fprintf(f, "  <cmt>%s</cmt>\n", entitize(wp->comment).toUtf8().constData());
	}
	if (!wp->description.isEmpty()) {
		fprintf(f, "  <desc>%s</desc>\n", entitize(wp->description).toUtf8().constData());
	}
	if (wp->source.isEmpty()) {
		fprintf(f, "  <src>%s</src>\n", entitize(wp->source).toUtf8().constData());
	}
	if (wp->type.isEmpty()) {
		fprintf(f, "  <type>%s</type>\n", entitize(wp->type).toUtf8().constData());
	}
	if (wp->url.isEmpty()) {
		fprintf(f, "  <url>%s</url>\n", entitize(wp->url).toUtf8().constData());
	}
	if (wp->image_full_path.isEmpty()) {
		fprintf(f, "  <link>%s</link>\n", entitize(wp->image_full_path).toUtf8().constData());
	}
	if (!wp->symbol_name.isEmpty()) {
		const GPXExportWptSymName pref = Preferences::get_gpx_export_wpt_sym_name();
		switch (pref) {
		case GPXExportWptSymName::Titlecase:
			fprintf(f, "  <sym>%s</sym>\n", entitize(wp->symbol_name).toUtf8().constData());
			break;
		case GPXExportWptSymName::Lowercase:
			fprintf(f, "  <sym>%s</sym>\n", entitize(wp->symbol_name).toLower().toUtf8().constData());
			break;
		default:
			qDebug() << SG_PREFIX_E << "Invalid GPX Export Waypoint Symbol Name preference" << (int) pref;
			break;
		}
	}

	fprintf(f, "</wpt>\n");
}




static void gpx_write_trackpoint(Trackpoint * tp, GPXWriteContext * context)
{
	FILE * f = context->file;

	char *time_iso8601;

	/* No such thing as a rteseg! So make sure we don't put them in. */
	if (context->options && !context->options->is_route && tp->newsegment) {
		fprintf(f, "  </trkseg>\n  <trkseg>\n");
	}

	static LatLon lat_lon = tp->coord.get_latlon();
	fprintf(f, "  <%spt lat=\"%s\" lon=\"%s\">\n", (context->options && context->options->is_route) ? "rte" : "trk", SGUtils::double_to_c(lat_lon.lat).toUtf8().constData(), SGUtils::double_to_c(lat_lon.lon).toUtf8().constData());

	if (!tp->name.isEmpty()) {
		fprintf(f, "    <name>%s</name>\n", entitize(tp->name).toUtf8().constData());
	}

	QString s_alt;
	if (tp->altitude != VIK_DEFAULT_ALTITUDE) {
		s_alt = SGUtils::double_to_c(tp->altitude);
	} else if (context->options != NULL && context->options->force_ele) {
		s_alt = SGUtils::double_to_c(0);
	}
	if (!s_alt.isEmpty()) {
		fprintf(f, "    <ele>%s</ele>\n", s_alt.toUtf8().constData());
	}

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

	if (!std::isnan(tp->course)) {
		fprintf(f, "    <course>%s</course>\n", SGUtils::double_to_c(tp->course).toUtf8().constData());
	}
	if (!std::isnan(tp->speed)) {
		fprintf(f, "    <speed>%s</speed>\n", SGUtils::double_to_c(tp->speed).toUtf8().constData());
	}
	if (tp->fix_mode == GPSFixMode::Fix2D) {
		fprintf(f, "    <fix>2d</fix>\n");
	}
	if (tp->fix_mode == GPSFixMode::Fix3D) {
		fprintf(f, "    <fix>3d</fix>\n");
	}
	if (tp->fix_mode == GPSFixMode::DGPS) {
		fprintf(f, "    <fix>dgps</fix>\n");
	}
	if (tp->fix_mode == GPSFixMode::PPS) {
		fprintf(f, "    <fix>pps</fix>\n");
	}
	if (tp->nsats > 0) {
		fprintf(f, "    <sat>%d</sat>\n", tp->nsats);
	}

	QString s_dop;

	if (tp->hdop != VIK_DEFAULT_DOP) {
		s_dop = SGUtils::double_to_c(tp->hdop);
		if (!s_dop.isEmpty()) {
			fprintf(f, "    <hdop>%s</hdop>\n", s_dop.toUtf8().constData());
		}
	}

	if (tp->vdop != VIK_DEFAULT_DOP) {
		s_dop = SGUtils::double_to_c(tp->vdop);
		if (!s_dop.isEmpty()) {
			fprintf(f, "    <vdop>%s</vdop>\n", s_dop.toUtf8().constData());
		}
	}

	if (tp->pdop != VIK_DEFAULT_DOP) {
		s_dop = SGUtils::double_to_c(tp->pdop);
		if (!s_dop.isEmpty()) {
			fprintf(f, "    <pdop>%s</pdop>\n", s_dop.toUtf8().constData());
		}
	}

	fprintf(f, "  </%spt>\n", (context->options && context->options->is_route) ? "rte" : "trk");
}




static void gpx_write_track(Track * trk, GPXWriteContext * context)
{
	/* Don't write invisible tracks when specified. */
	if (context->options && !context->options->hidden && !trk->visible) {
		return;
	}

	FILE * f = context->file;

	QString tmp;
	/* Sanity clause. */
	if (trk->name.isEmpty()) {
		tmp = "track"; /* TODO_MAYBE: localize? */
	} else {
		tmp = entitize(trk->name);
	}

	/* NB 'hidden' is not part of any GPX standard - this appears to be a made up Viking 'extension'.
	   Luckily most other GPX processing software ignores things they don't understand. */
	fprintf(f, "<%s%s>\n  <name>%s</name>\n",
		trk->type_id == "sg.trw.route" ? "rte" : "trk",
		trk->visible ? "" : " hidden=\"hidden\"",
		tmp.toUtf8().constData());

	if (!trk->comment.isEmpty()) {
		fprintf(f, "  <cmt>%s</cmt>\n", entitize(trk->comment).toUtf8().constData());
	}

	if (!trk->description.isEmpty()) {
		fprintf(f, "  <desc>%s</desc>\n", entitize(trk->description).toUtf8().constData());
	}

	if (!trk->source.isEmpty()) {
		fprintf(f, "  <src>%s</src>\n", entitize(trk->source).toUtf8().constData());
	}

	if (!trk->type.isEmpty()) {
		fprintf(f, "  <type>%s</type>\n", entitize(trk->type).toUtf8().constData());
	}

	/* No such thing as a rteseg! */
	if (trk->type_id == "sg.trw.track") {
		fprintf(f, "  <trkseg>\n");
	}

	if (!trk->empty()) {

		auto first = trk->trackpoints.begin();
		bool first_tp_is_newsegment = (*first)->newsegment;
		(*first)->newsegment = false; /* So we won't write </trkseg><trkseg> already. */

		for (auto iter = trk->trackpoints.begin(); iter != trk->trackpoints.end(); iter++) {
			gpx_write_trackpoint(*iter, context);
		}

		(*first)->newsegment = first_tp_is_newsegment; /* Restore state. */
	}

	/* NB apparently no such thing as a rteseg! */
	if (trk->type_id == "sg.trw.track") {
		fprintf(f, "  </trkseg>\n");
	}

	fprintf(f, "</%s>\n", trk->type_id == "sg.trw.route" ? "rte" : "trk");
}




static void gpx_write_header(FILE * file)
{
	fprintf(file, "<?xml version=\"1.0\"?>\n"
		"<gpx version=\"1.0\" creator=\"Viking -- http://viking.sf.net/\"\n"
		"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
		"xmlns=\"http://www.topografix.com/GPX/1/0\"\n"
		"xsi:schemaLocation=\"http://www.topografix.com/GPX/1/0 http://www.topografix.com/GPX/1/0/gpx.xsd\">\n");
}




static void gpx_write_footer(FILE * file)
{
	fprintf(file, "</gpx>\n");
}




static int gpx_waypoint_compare(const void * x, const void * y)
{
	Waypoint * a = (Waypoint *) x;
	Waypoint * b = (Waypoint *) y;
	return strcmp(a->name.toUtf8().constData(), b->name.toUtf8().constData());
}




sg_ret GPX::write_layer_to_file(FILE * file, LayerTRW * trw, GPXWriteOptions * options)
{
	GPXWriteContext context(options, file);

	gpx_write_header(file);

	const QString name = trw->get_name();
	if (!name.isEmpty()) {
		fprintf(file, "  <name>%s</name>\n", entitize(name).toUtf8().constData());
	}

	TRWMetadata * md = trw->get_metadata();
	if (md) {
		if (!md->author.isEmpty()) {
			fprintf(file, "  <author>%s</author>\n", entitize(md->author).toUtf8().constData());
		}
		if (!md->description.isEmpty()) {
			fprintf(file, "  <desc>%s</desc>\n", entitize(md->description).toUtf8().constData());
		}
		if (!md->iso8601_timestamp.isEmpty()) {
			fprintf(file, "  <time>%s</time>\n", entitize(md->iso8601_timestamp).toUtf8().constData());
		}
		if (!md->keywords.isEmpty()) {
			fprintf(file, "  <keywords>%s</keywords>\n", entitize(md->keywords).toUtf8().constData());
		}
	}

	if (trw->get_waypoints_visibility() || (options && options->hidden)) {
		std::list<Waypoint *> copy = trw->get_waypoints();
		copy.sort(gpx_waypoint_compare);

		for (auto iter = copy.begin(); iter != copy.end(); iter++) {
			gpx_write_waypoint(*iter, &context);
		}
	}


	GPXWriteOptions trk_options(false, false, false, false);
	/* Force trackpoints on tracks. */
	if (!context.options) {
		context.options = &trk_options;
	}


	/* Tracks sorted according to preferences. */
	if (trw->tracks.size() && (trw->get_tracks_visibility() || (options && options->hidden))) {

		std::list<Track *> track_values;
		trw->tracks.get_tracks_list(track_values);

		if (track_values.size()) {
			switch (Preferences::get_gpx_export_trk_sort()) {
			case GPXExportTrackSort::Time:
				track_values.sort(Track::compare_timestamp);
				break;
			case GPXExportTrackSort::Alpha:
				track_values.sort(TreeItem::compare_name_ascending);
				break;
			default:
				break;
			}

			/* Loop around each list and write each one. */
			context.options->is_route = false;
			for (auto iter = track_values.begin(); iter != track_values.end(); iter++) {
				gpx_write_track(*iter, &context);
			}
		}
	}


	/* Routes always sorted by name. */
	if (trw->routes.size() && (trw->get_routes_visibility() || (options && options->hidden))) {

		std::list<Track *> route_values;
		trw->routes.get_tracks_list(route_values);

		if (route_values.size()) {
			route_values.sort(TreeItem::compare_name_ascending);

			context.options->is_route = true;
			for (auto iter = route_values.begin(); iter != route_values.end(); iter++) {
				gpx_write_track(*iter, &context);
			}
		}
	}


	gpx_write_footer(file);

	return sg_ret::ok;
}




sg_ret GPX::write_track_to_file(FILE * file, Track * trk, GPXWriteOptions * options)
{
	GPXWriteContext context(options, file);
	gpx_write_header(file);
	gpx_write_track(trk, &context);
	gpx_write_footer(file);

	return sg_ret::ok;
}




/**
 * Common write of a temporary GPX file.
 */
sg_ret GPX::write_layer_track_to_tmp_file(QString & file_full_path, LayerTRW * trw, Track * trk, GPXWriteOptions * options)
{
	QTemporaryFile tmp_file;
	tmp_file.setFileTemplate("viking_XXXXXX.gpx");
	if (!tmp_file.open()) {
		qDebug() << SG_PREFIX_E << "Failed to open temporary file, error =" << tmp_file.error();
		return sg_ret::err;
	}
	tmp_file.setAutoRemove(false);
	file_full_path = tmp_file.fileName();
	qDebug() << SG_PREFIX_D << "Temporary file =" << file_full_path;

	FILE * file2 = fdopen(tmp_file.handle(), "w"); /* TODO: close the file? */

	sg_ret rv;
	if (trk) {
		rv = GPX::write_track_to_file(file2, trk, options);
	} else {
		rv = GPX::write_layer_to_file(file2, trw, options);
	}

	tmp_file.close();

	return rv;
}




/*
 * @trw:     The #LayerTRW to write
 * @options: Possible ways of writing the file data (can be NULL)
 *
 * Returns: The name of newly created temporary GPX file.
 *          This file should be removed once used and the string freed.
 *          If NULL then the process failed.
 */
sg_ret GPX::write_layer_to_tmp_file(QString & file_full_path, LayerTRW * trw, GPXWriteOptions * options)
{
	return GPX::write_layer_track_to_tmp_file(file_full_path, trw, NULL, options);
}




/*
 * @trk:     The #Track to write
 * @options: Possible ways of writing the file data (can be NULL)
 *
 * Returns: The name of newly created temporary GPX file.
 *          This file should be removed once used and the string freed.
 *          If NULL then the process failed.
 */
sg_ret GPX::write_track_to_tmp_file(QString & file_full_path, Track * trk, GPXWriteOptions * options)
{
	return GPX::write_layer_track_to_tmp_file(file_full_path, NULL, trk, options);
}




GPXImporter::GPXImporter(LayerTRW * new_trw)
{
	this->trw = new_trw;

	this->parser = XML_ParserCreate(NULL);

	XML_SetElementHandler(this->parser, (XML_StartElementHandler) gpx_start, (XML_EndElementHandler) gpx_end);
	XML_SetUserData(this->parser, this);
	XML_SetCharacterDataHandler(this->parser, (XML_CharacterDataHandler) gpx_cdata);

	this->unnamed_waypoints = 1;
	this->unnamed_tracks = 1;
	this->unnamed_routes = 1;

	qDebug() << SG_PREFIX_I << "Importer for TRW layer" << this->trw->name << "created";
}




GPXImporter::~GPXImporter()
{
	XML_ParserFree(this->parser);

	qDebug() << SG_PREFIX_I << "Importer for TRW layer" << this->trw->name << "deleted," << this->n_bytes << "bytes processed";
}




sg_ret GPXImporter::write(const char * data, size_t size)
{
#if 0
	char buffer[GPX_BUFFER_SIZE];
	snprintf(buffer, std::min(sizeof (buffer), size), "%s", data);
	qDebug() << SG_PREFIX_D << "Writing to importer" << buffer;
#endif

	const int finish = (size <= 0);
	this->status = XML_Parse(this->parser, data, size, finish);

	if (this->status != XML_STATUS_OK) {
		qDebug() << SG_PREFIX_E << "XML parsing returns non-ok:" << this->status;
	}

	this->n_bytes += size;
#if 0
	qDebug() << SG_PREFIX_I << "Finish =" << finish << ", success =" << ();
#endif

	return this->status != XML_STATUS_ERROR ? sg_ret::ok : sg_ret::err;
}
